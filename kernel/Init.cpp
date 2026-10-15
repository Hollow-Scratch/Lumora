
#include <stdint.h>

#include "arch/x86/Interrupts/IDT.hpp"

extern "C" uint32_t multiboot_magic;
extern "C" uint32_t multiboot_info;

struct VgaCell {
    char character;
    uint8_t color;
};

extern "C" void kernel_main() {
    asm volatile("cli");

    auto* vga_buffer =
        reinterpret_cast<VgaCell*>(0xB8000);

    constexpr uint8_t white_on_black = 0x0F;

    for (int i = 0; i < 80 * 25; ++i) {
        vga_buffer[i].character = ' ';
        vga_buffer[i].color = white_on_black;
    }

    const char* message =
        "WELCOME TO LUMORA OS (MINIMAL VGA TEXT MODE)";

    for (int i = 0; message[i] != '\0'; ++i) {
        vga_buffer[i].character = message[i];
        vga_buffer[i].color = 0x0A;
    }

    Lumora::Arch::x86::InitIDT();

    const char* status = "IDT LOADED";
    for (int i = 0; status[i] != '\0'; ++i) {
        vga_buffer[80 + i].character = status[i];
        vga_buffer[80 + i].color = 0x0F;
    }

    // Optional fatal exception probes: make clean builds inert by default.
#if defined(LUMORA_TEST_DE)
    asm volatile("xor %%edx, %%edx; xor %%eax, %%eax; xor %%ecx, %%ecx; div %%ecx" ::: "rax", "rdx", "rcx");
#elif defined(LUMORA_TEST_UD)
    asm volatile("ud2");
#elif defined(LUMORA_TEST_BP)
    asm volatile("int3");
#elif defined(LUMORA_TEST_GP)
    asm volatile("mov $0x28, %%ax; mov %%ax, %%ds" ::: "rax", "memory");
#endif

    (void)multiboot_magic;
    (void)multiboot_info;

    for (;;) {
        asm volatile("hlt");
    }
}
