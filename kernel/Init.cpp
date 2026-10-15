#include <stdint.h>

#include "Core/Panic.hpp"
#include "Graphics/Framebuffer.hpp"
#include "arch/x86/BootInfo.hpp"
#include "arch/x86/Framebuffer.hpp"
#include "arch/x86/Interrupts/IDT.hpp"

namespace {
using Lumora::Graphics::Color;
void RenderBootScreen(const Lumora::Arch::x86::BootInfo& info) noexcept {
    auto& fb = Lumora::Graphics::Console();
    constexpr Color background{20, 18, 31};
    constexpr Color foreground{236, 231, 246};
    constexpr Color accent{192, 150, 255};
    fb.Clear(background);
    fb.DrawString(24, 24, "LUMORA OS", accent, fb.Width() >= 600 ? 2 : 1);
    fb.DrawString(24, 68, "MULTIBOOT 2 BOOT COMPLETE", foreground);
    fb.DrawString(24, 88, info.has_bootloader ? info.bootloader : "BOOTLOADER: UNAVAILABLE", foreground);
    fb.DrawString(24, 108, "MEMORY MAP ENTRIES:", accent);
    fb.DrawDecimal(150, 108, info.memory_count, foreground);

    // Simple, deliberate color/pitch pattern to exercise the renderer.
    const int64_t bar_y = 145;
    const int64_t bar_width = fb.Width() > 180 ? 48 : fb.Width() / 4;
    fb.FillRect(24, bar_y, bar_width, 12, {255, 0, 0});
    fb.FillRect(24 + bar_width + 8, bar_y, bar_width, 12, {0, 255, 0});
    fb.FillRect(24 + (bar_width + 8) * 2, bar_y, bar_width, 12, {0, 0, 255});
    fb.DrawString(24, bar_y + 20, "FRAMEBUFFER RGB TEST PATTERN", foreground);
    fb.DrawHorizontalLine(24, bar_y + 42, static_cast<int64_t>(fb.Width()) - 48, accent);
}
}

extern "C" void kernel_main(uint32_t magic, uint32_t information_address) {
    asm volatile("cli" ::: "memory");

    // Install the exception path before parsing or touching externally supplied
    // metadata. Until graphics comes up, Panic reports through serial and halts.
    Lumora::Arch::x86::InitIDT();

    const Lumora::Arch::x86::BootHandoff handoff{magic, information_address};
    Lumora::Arch::x86::BootInfo boot_info{};
    if (!Lumora::Arch::x86::ParseBootInfo(handoff, boot_info))
        LUMORA_PANIC(Lumora::Core::PanicReason::InvalidBootInfo,
                     "Multiboot 2 handoff or information tags are invalid");
    if (!boot_info.has_framebuffer || boot_info.framebuffer.type != 1 ||
        !boot_info.framebuffer.has_rgb_fields)
        LUMORA_PANIC(Lumora::Core::PanicReason::FramebufferUnavailable,
                     "No supported direct-RGB framebuffer tag was supplied");

    Lumora::Arch::x86::FramebufferMapping mapping{};
    if (!Lumora::Arch::x86::MapFramebuffer(boot_info.framebuffer, mapping))
        LUMORA_PANIC(Lumora::Core::PanicReason::FramebufferUnavailable,
                     "Framebuffer range could not be mapped safely");

    const auto& source = boot_info.framebuffer;
    const Lumora::Graphics::FramebufferInfo format{
        source.width, source.height, source.pitch, source.bits_per_pixel,
        source.red_position, source.red_mask_size,
        source.green_position, source.green_mask_size,
        source.blue_position, source.blue_mask_size
    };
    auto& framebuffer = Lumora::Graphics::Console();
    if (!framebuffer.Initialize(mapping.address, mapping.length, format))
        LUMORA_PANIC(Lumora::Core::PanicReason::FramebufferUnavailable,
                     "Framebuffer format, pitch, or channel masks are unsupported");

    RenderBootScreen(boot_info);

#if defined(LUMORA_TEST_PANIC)
    LUMORA_PANIC(Lumora::Core::PanicReason::KernelInvariantViolation,
                 "Controlled panic screen test");
#elif defined(LUMORA_TEST_DE)
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
