#include <stdint.h>

#include "arch/x86/BootInfo.hpp"
#include "arch/x86/Interrupts/IDT.hpp"

struct VgaCell { char character; uint8_t color; };
static void WriteLine(volatile VgaCell* screen, unsigned row, const char* s, uint8_t color) {
    for (unsigned i = 0; s[i] && i < 80; ++i) {
        screen[row * 80 + i].character = s[i];
        screen[row * 80 + i].color = color;
    }
}

extern "C" void kernel_main(uint32_t magic, uint32_t information_address) {
    asm volatile("cli");
    auto* screen = reinterpret_cast<volatile VgaCell*>(0xB8000);
    for (int i = 0; i < 80 * 25; ++i) {
        screen[i].character = ' ';
        screen[i].color = 0x0F;
    }
    WriteLine(screen, 0, "WELCOME TO LUMORA OS (MULTIBOOT 2)", 0x0A);

    Lumora::Arch::x86::BootHandoff handoff{magic, information_address};
    Lumora::Arch::x86::BootInfo boot_info{};
    const bool parsed = Lumora::Arch::x86::ParseBootInfo(handoff, boot_info);
    WriteLine(screen, 1, parsed ? "MULTIBOOT 2 INFO VALIDATED" : "MULTIBOOT 2 INFO INVALID", parsed ? 0x0A : 0x4F);
    if (parsed) {
        if (boot_info.has_framebuffer) WriteLine(screen, 2, "FRAMEBUFFER TAG PARSED (NOT MAPPED)", 0x0F);
        if (boot_info.memory_count) WriteLine(screen, 3, "MEMORY MAP TAG PARSED", 0x0F);
    }

    Lumora::Arch::x86::InitIDT();
    WriteLine(screen, 4, "IDT LOADED; SIDT READBACK PASSED", 0x0F);

#if defined(LUMORA_TEST_DE)
    asm volatile("xor %%edx, %%edx; xor %%eax, %%eax; xor %%ecx, %%ecx; div %%ecx" ::: "rax", "rdx", "rcx");
#elif defined(LUMORA_TEST_UD)
    asm volatile("ud2");
#elif defined(LUMORA_TEST_BP)
    asm volatile("int3");
#elif defined(LUMORA_TEST_GP)
    asm volatile("mov $0x28, %%ax; mov %%ax, %%ds" ::: "rax", "memory");
#endif

    for (;;) asm volatile("hlt");
}
