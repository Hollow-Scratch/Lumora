#include "Framebuffer.hpp"

namespace Lumora::Arch::x86 {
extern "C" {
extern uint64_t pml4_table[512];
extern uint64_t pd0_table[512];
extern uint64_t pd1_table[512];
extern uint64_t pd2_table[512];
extern uint64_t pd3_table[512];
}

namespace {
constexpr uint64_t FourGiB = 0x100000000ull;
constexpr uint64_t PhysicalLimit = 0x0010000000000000ull; // x86-64 52-bit PTE address
constexpr uint64_t Window = 0xFFFF800000000000ull;         // PML4 slot 256
constexpr uint64_t PageSize = 4096;
constexpr uint64_t HugePageSize = 0x200000;
constexpr size_t PageTableCount = 32;                      // 64 MiB virtual window
constexpr uint64_t AddressMask = 0x000FFFFFFFFFF000ull;
constexpr uint32_t Ia32Pat = 0x277;
constexpr uint64_t PatWriteCombiningEntry = 1ull << 32; // PAT entry 4 = WC
constexpr uint64_t PatIndexFlags = 0x18;                // PWT + PCD selects entry 3

alignas(4096) uint64_t framebuffer_pdpt[512]{};
alignas(4096) uint64_t framebuffer_pd[512]{};
alignas(4096) uint64_t framebuffer_pts[PageTableCount][512]{};
bool mapped_once = false;
bool pat_wc_ready = false;
bool pat_wc_checked = false;

uint64_t* DirectoryFor(uint64_t physical) noexcept {
    const uint64_t index = physical / HugePageSize;
    if (index < 512) return reinterpret_cast<uint64_t*>(pd0_table);
    if (index < 1024) return reinterpret_cast<uint64_t*>(pd1_table);
    if (index < 1536) return reinterpret_cast<uint64_t*>(pd2_table);
    return reinterpret_cast<uint64_t*>(pd3_table);
}

void FlushTlb() noexcept {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

uint64_t ReadMsr(uint32_t msr) noexcept {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (static_cast<uint64_t>(high) << 32) | low;
}

bool ConfigureWriteCombining() noexcept {
    if (pat_wc_checked) return pat_wc_ready;
    pat_wc_checked = true;
    uint32_t eax = 1, ebx, ecx, edx;
    asm volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    (void)ebx;
    (void)ecx;
    if (!(edx & (1u << 16))) return false; // CPUID.1:EDX.PAT

    uint64_t pat = ReadMsr(Ia32Pat);
    // Index 3 is the scalar UC fallback selected by PWT+PCD; index 4 is WC.
    pat = (pat & ~((0xFFull << 24) | (0xFFull << 32))) |
          PatWriteCombiningEntry;
    // No framebuffer writes have occurred yet. Flush before and after changing
    // PAT so no cached line survives a memory-type transition.
    asm volatile("wbinvd" ::: "memory");
    asm volatile("wrmsr" : : "c"(Ia32Pat), "a"(static_cast<uint32_t>(pat)),
                  "d"(static_cast<uint32_t>(pat >> 32)) : "memory");
    asm volatile("wbinvd" ::: "memory");
    FlushTlb();
    pat_wc_ready = ReadMsr(Ia32Pat) == pat;
    return pat_wc_ready;
}

void SetLargePageType(uint64_t& entry, bool write_combining) noexcept {
    if (write_combining) {
        entry = (entry & ~PatIndexFlags) | (1ull << 12); // large-page PAT bit
    } else {
        entry = (entry & ~(1ull << 12)) | PatIndexFlags;
    }
}
}

bool MapFramebuffer(const Framebuffer& f, FramebufferMapping& out) noexcept {
    out = {};
    if (!f.address || !f.width || !f.height || !f.pitch ||
        (f.bits_per_pixel != 24 && f.bits_per_pixel != 32)) return false;
    const uint64_t minimum_pitch = static_cast<uint64_t>(f.width) * (f.bits_per_pixel / 8);
    const uint64_t length = static_cast<uint64_t>(f.pitch) * f.height;
    if (minimum_pitch > f.pitch || !length || f.address >= PhysicalLimit ||
        length > PhysicalLimit - f.address || length > 64 * 1024 * 1024ull ||
        length > SIZE_MAX) return false;

    if (f.address < FourGiB && length <= FourGiB - f.address) {
        const uint64_t first = f.address / HugePageSize;
        const uint64_t last = (f.address + length - 1) / HugePageSize;
        for (uint64_t index = first; index <= last; ++index) {
            uint64_t physical = index * HugePageSize;
            uint64_t* directory = DirectoryFor(physical);
            const unsigned entry = static_cast<unsigned>(index & 511);
            if ((directory[entry] & 0x83) != 0x83) return false;
        }
        const bool wc = ConfigureWriteCombining();
        for (uint64_t index = first; index <= last; ++index) {
            uint64_t* directory = DirectoryFor(index * HugePageSize);
            SetLargePageType(directory[index & 511], wc);
        }
        FlushTlb();
        out.address = reinterpret_cast<volatile uint8_t*>(f.address);
        out.length = static_cast<size_t>(length);
        return true;
    }

    if (mapped_once || (pml4_table[256] & 1) ||
        (Window >> 39) != 256) return false;
    const uint64_t page_offset = f.address & (PageSize - 1);
    const uint64_t physical_start = f.address - page_offset;
    const uint64_t covered = page_offset + length;
    const uint64_t pages = (covered + PageSize - 1) / PageSize;
    const uint64_t pt_count = (pages + 511) / 512;
    if (pt_count > PageTableCount || pt_count > 32) return false;

    // If a boundary-crossing range aliases the pre-existing low identity map,
    // give that alias the same PAT/MTRR-selected type as the high window.
    if (physical_start < FourGiB) {
        const uint64_t low_last = ((f.address + length - 1) < FourGiB)
            ? ((f.address + length - 1) / HugePageSize)
            : ((FourGiB - 1) / HugePageSize);
        const uint64_t low_first = physical_start / HugePageSize;
        for (uint64_t index = low_first; index <= low_last; ++index) {
            uint64_t* directory = DirectoryFor(index * HugePageSize);
            if ((directory[index & 511] & 0x83) != 0x83) return false;
        }
    }

    const bool wc = ConfigureWriteCombining();
    if (physical_start < FourGiB) {
        const uint64_t low_last = ((f.address + length - 1) < FourGiB)
            ? ((f.address + length - 1) / HugePageSize)
            : ((FourGiB - 1) / HugePageSize);
        const uint64_t low_first = physical_start / HugePageSize;
        for (uint64_t index = low_first; index <= low_last; ++index) {
            uint64_t* directory = DirectoryFor(index * HugePageSize);
            SetLargePageType(directory[index & 511], wc);
        }
    }

    framebuffer_pdpt[0] = reinterpret_cast<uint64_t>(framebuffer_pd) | 3;
    for (uint64_t i = 0; i < pt_count; ++i)
        framebuffer_pd[i] = reinterpret_cast<uint64_t>(framebuffer_pts[i]) | 3;
    for (uint64_t i = 0; i < pages; ++i) {
        const uint64_t physical_page = physical_start + i * PageSize;
        if (physical_page >= PhysicalLimit) return false;
        framebuffer_pts[i / 512][i % 512] =
            (physical_page & AddressMask) | (wc ? 0x83 : 0x1B);
    }
    // Publish only after every entry has been validated and populated.
    framebuffer_pdpt[0] = reinterpret_cast<uint64_t>(framebuffer_pd) | 3;
    pml4_table[256] = reinterpret_cast<uint64_t>(framebuffer_pdpt) | 3;
    FlushTlb();
    mapped_once = true;
    out.address = reinterpret_cast<volatile uint8_t*>(Window + page_offset);
    out.length = static_cast<size_t>(length);
    return true;
}
} // namespace Lumora::Arch::x86
