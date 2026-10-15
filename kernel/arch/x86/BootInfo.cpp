#include "BootInfo.hpp"

namespace Lumora::Arch::x86 {
namespace {
constexpr uint32_t Magic = 0x36D76289;
constexpr uint32_t EndTag = 0, CmdlineTag = 1, BootloaderTag = 2;
constexpr uint32_t MmapTag = 6, FramebufferTag = 8;
constexpr size_t MappedLimit = 0x100000000ull; // bootstrap identity map: 4 GiB

uint32_t Read32(const uint8_t* p) {
    uint32_t v; __builtin_memcpy(&v, p, sizeof(v)); return v;
}
uint64_t Read64(const uint8_t* p) {
    uint64_t v; __builtin_memcpy(&v, p, sizeof(v)); return v;
}
bool CopyString(char* out, size_t capacity, const uint8_t* data, size_t size) {
    size_t n = 0;
    while (n < size && data[n]) ++n;
    if (n == size) return false;
    const size_t copied = n < capacity - 1 ? n : capacity - 1;
    for (size_t i = 0; i < copied; ++i) out[i] = static_cast<char>(data[i]);
    out[copied] = 0;
    return true;
}
}

bool ParseBootInfo(const BootHandoff& handoff, BootInfo& out) {
    if (handoff.magic != Magic || handoff.information_address < 8) return false;
    const uintptr_t base = handoff.information_address;
    if ((base & 7) != 0 || base >= MappedLimit) return false;
    const auto* bytes = reinterpret_cast<const uint8_t*>(base);
    const uint32_t total = Read32(bytes);
    if (total < 16 || (total & 7) != 0 || total > MappedLimit - base) return false;
    if (Read32(bytes + 4) != 0) return false;

    BootInfo parsed{};
    size_t offset = 8;
    bool saw_end = false;
    while (offset <= total - 8) {
        const uint8_t* tag = bytes + offset;
        const uint32_t type = Read32(tag), size = Read32(tag + 4);
        if (size < 8 || size > total - offset) return false;
        if (type == EndTag) {
            if (size != 8 || offset + 8 != total) return false;
            saw_end = true;
            break;
        }
        if (type == CmdlineTag || type == BootloaderTag) {
            if (size <= 8) return false;
            char* dest = type == CmdlineTag ? parsed.command_line : parsed.bootloader;
            size_t cap = type == CmdlineTag ? sizeof(parsed.command_line) : sizeof(parsed.bootloader);
            if (!CopyString(dest, cap, tag + 8, size - 8)) return false;
            if (type == CmdlineTag) parsed.has_command_line = true;
            else parsed.has_bootloader = true;
        } else if (type == MmapTag) {
            if (size < 16) return false;
            const uint32_t entry_size = Read32(tag + 8);
            const uint32_t entry_version = Read32(tag + 12);
            if (entry_version != 0) return false;
            if (entry_size < 24 || (size - 16) % entry_size != 0) return false;
            for (size_t p = 16; p < size; p += entry_size) {
                if (parsed.memory_count == 128) return false;
                auto& e = parsed.memory[parsed.memory_count++];
                e.address = Read64(tag + p);
                e.length = Read64(tag + p + 8);
                e.type = Read32(tag + p + 16);
            }
        } else if (type == FramebufferTag) {
            if (size < 32) return false;
            auto& f = parsed.framebuffer;
            f.address = Read64(tag + 8);
            f.pitch = Read32(tag + 16);
            f.width = Read32(tag + 20);
            f.height = Read32(tag + 24);
            f.bits_per_pixel = tag[28];
            f.type = tag[29];
            if (!f.pitch || !f.width || !f.height || !f.bits_per_pixel) return false;
            if (f.type == 1) {
                if (size < 38) return false;
                f.has_rgb_fields = true;
                f.red_position = tag[32]; f.red_mask_size = tag[33];
                f.green_position = tag[34]; f.green_mask_size = tag[35];
                f.blue_position = tag[36]; f.blue_mask_size = tag[37];
            }
            parsed.has_framebuffer = true;
        }
        const size_t next = (static_cast<size_t>(size) + 7) & ~size_t(7);
        if (next < size || next > total - offset) return false;
        offset += next;
    }
    if (!saw_end) return false;
    out = parsed;
    return true;
}
} // namespace Lumora::Arch::x86
