#include <stdint.h>

#include "Core/Panic.hpp"
#include "Graphics/Framebuffer.hpp"
#include "arch/x86/BootInfo.hpp"
#include "arch/x86/Framebuffer.hpp"
#include "arch/x86/Interrupts/IDT.hpp"
#include "Driver/InterruptController/PIC8259/PIC8259.hpp"
#include "Driver/Input/PS2Keyboard/PS2Keyboard.hpp"

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

void DrawKeyboardPrompt() noexcept {
    using Lumora::Graphics::Color;
    auto& fb = Lumora::Graphics::Console();
    constexpr Color text{236, 231, 246};
    fb.DrawString(24, 210, "KEYBOARD: TYPE TEXT; ENTER NEW LINE", text);
}

void EchoKey(const Lumora::Driver::Input::PS2Keyboard::KeyEvent& event,
             int64_t& x, int64_t& y) noexcept {
    using Lumora::Graphics::Color;
    constexpr Color foreground{236, 231, 246};
    constexpr Color background{20, 18, 31};
    auto& fb = Lumora::Graphics::Console();
    if (!event.pressed) return;
    if (event.key == Lumora::Driver::Input::PS2Keyboard::KeyCode::Enter) {
        x = 24;
        y += 10;
        if (y + 8 >= fb.Height()) y = 230;
        return;
    }
    if (event.key == Lumora::Driver::Input::PS2Keyboard::KeyCode::Backspace && x > 24) {
        x -= 6;
        fb.FillRect(x, y - 1, 6, 9, background);
        return;
    }
    if (event.character >= 32 && event.character <= 126) {
        fb.DrawCharacter(x, y, event.character, foreground);
        x += 6;
        if (x + 6 >= fb.Width()) { x = 24; y += 10; }
    }
}

#if defined(LUMORA_TEST_KEYBOARD)
void LogKeyboardEvent(const Lumora::Driver::Input::PS2Keyboard::KeyEvent& event) noexcept {
    auto port_out = [](uint16_t port, uint8_t value) {
        asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
    };
    static bool serial_ready = false;
    if (!serial_ready) {
        port_out(0x3F9, 0); port_out(0x3FB, 0x80); port_out(0x3F8, 1);
        port_out(0x3F9, 0); port_out(0x3FB, 3); port_out(0x3FA, 0xC7);
        port_out(0x3FC, 0x0B);
        serial_ready = true;
    }
    auto out = [&port_out](char value) {
        for (uint32_t wait = 0; wait < 10000; ++wait) {
            uint8_t status;
            asm volatile("inb %1, %0" : "=a"(status) : "Nd"(uint16_t{0x3FD}));
            if (status & 0x20) { port_out(0x3F8, static_cast<uint8_t>(value)); return; }
            asm volatile("pause");
        }
    };
    constexpr char prefix[] = "KBD_EVENT key=";
    for (char c : prefix) { if (!c) break; out(c); }
    const uint16_t key = static_cast<uint16_t>(event.key);
    constexpr char digits[] = "0123456789ABCDEF";
    out(digits[(key >> 12) & 15]); out(digits[(key >> 8) & 15]);
    out(digits[(key >> 4) & 15]); out(digits[key & 15]);
    out(event.pressed ? ' ' : '-');
    out(event.character >= 32 && event.character <= 126 ? event.character : '.');
    out('\r'); out('\n');
}
#endif
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

    // Input setup runs only after framebuffer diagnostics are available. PIC
    // remains fully masked until the keyboard driver registers IRQ1's handler.
    Lumora::Driver::InterruptController::PIC8259::Initialize();
    const auto keyboard_result = Lumora::Driver::Input::PS2Keyboard::Initialize();
    if (keyboard_result == Lumora::Driver::Input::PS2Keyboard::InitResult::Ready)
        DrawKeyboardPrompt();
    else {
        static constexpr const char* failures[] = {
            "READY", "PS/2 CONTROLLER TIMEOUT", "NO PS/2 CONTROLLER",
            "KEYBOARD INTERFACE UNAVAILABLE", "KEYBOARD DID NOT RESPOND",
            "KEYBOARD REJECTED SCANCODE CONFIG", "IRQ1 REGISTRATION FAILED"
        };
        const auto index = static_cast<unsigned>(keyboard_result);
        Lumora::Graphics::Console().DrawString(24, 210,
            index < sizeof(failures) / sizeof(failures[0]) ? failures[index] : "KEYBOARD INIT FAILED",
            {255, 150, 150});
    }

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

    if (keyboard_result == Lumora::Driver::Input::PS2Keyboard::InitResult::Ready) {
        int64_t cursor_x = 24, cursor_y = 230;
        for (;;) {
            asm volatile("cli" ::: "memory");
            Lumora::Driver::Input::PS2Keyboard::KeyEvent event{};
            const bool has_event = Lumora::Driver::Input::PS2Keyboard::ReadEvent(event);
            if (!has_event) {
                // STI's one-instruction interrupt shadow makes HLT race-free:
                // a queued IRQ cannot be missed between the empty check and sleep.
                asm volatile("sti; hlt" ::: "memory");
                continue;
            }
            asm volatile("sti" ::: "memory");
#if defined(LUMORA_TEST_KEYBOARD)
            LogKeyboardEvent(event);
#endif
            EchoKey(event, cursor_x, cursor_y);
        }
    }
    for (;;) asm volatile("cli; hlt" ::: "memory");
}
