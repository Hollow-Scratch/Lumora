#include <stdint.h>
#include <stddef.h>

#include "Core/Log.hpp"
#include "Core/Panic.hpp"
#include "Console/BootScreen.hpp"
#include "Console/Terminal.hpp"
#include "Graphics/Framebuffer.hpp"
#include "Version.hpp"
#include "arch/x86/BootInfo.hpp"
#include "arch/x86/Framebuffer.hpp"
#include "arch/x86/Interrupts/IDT.hpp"
#include "Driver/InterruptController/PIC8259/PIC8259.hpp"
#include "Driver/Input/PS2Keyboard/PS2Keyboard.hpp"

namespace {
void AppendText(char* output, size_t& used, const char* text) noexcept {
    while (*text && used < 104) output[used++] = *text++;
}

void AppendDecimal(char* output, size_t& used, uint64_t value) noexcept {
    char digits[20];
    size_t count = 0;
    do { digits[count++] = static_cast<char>('0' + value % 10); value /= 10; }
    while (value && count < sizeof(digits));
    while (count && used < 104) output[used++] = digits[--count];
}

void AppendHex(char* output, size_t& used, uint64_t value) noexcept {
    static constexpr char hex[] = "0123456789ABCDEF";
    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
        const uint8_t nibble = static_cast<uint8_t>((value >> shift) & 0xF);
        if (nibble || started || shift == 0) {
            if (used < 104) output[used++] = hex[nibble];
            started = true;
        }
    }
}

void LogFramebufferMode(const Lumora::Arch::x86::Framebuffer& f) noexcept {
    char message[112]{};
    size_t used = 0;
    AppendText(message, used, "Framebuffer ");
    AppendDecimal(message, used, f.width); AppendText(message, used, "x");
    AppendDecimal(message, used, f.height); AppendText(message, used, " pitch=");
    AppendDecimal(message, used, f.pitch); AppendText(message, used, " bpp=");
    AppendDecimal(message, used, f.bits_per_pixel); AppendText(message, used, " rgb=");
    AppendDecimal(message, used, f.red_position); AppendText(message, used, "/");
    AppendDecimal(message, used, f.red_mask_size); AppendText(message, used, ",");
    AppendDecimal(message, used, f.green_position); AppendText(message, used, "/");
    AppendDecimal(message, used, f.green_mask_size); AppendText(message, used, ",");
    AppendDecimal(message, used, f.blue_position); AppendText(message, used, "/");
    AppendDecimal(message, used, f.blue_mask_size); AppendText(message, used, " addr=0x");
    AppendHex(message, used, f.address);
    message[used] = '\0';
    Lumora::Core::Log::Info(message);
}

[[noreturn]] void Fail(Lumora::Core::PanicReason reason, const char* message) noexcept {
    Lumora::Core::Log::Error(message);
    Lumora::Core::Panic(reason, message, __FILE__, __LINE__);
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
    Lumora::Core::Log::Info("Lumora kernel starting");
    Lumora::Core::Log::Info(Lumora::KernelVersion);
    Lumora::Arch::x86::InitIDT();
    Lumora::Core::Log::Info("[OK] GDT active; IDT installed and verified");

    Lumora::Core::Log::Info("Parsing Multiboot 2 handoff");
    const Lumora::Arch::x86::BootHandoff handoff{magic, information_address};
    Lumora::Arch::x86::BootInfo boot_info{};
    if (!Lumora::Arch::x86::ParseBootInfo(handoff, boot_info))
        Fail(Lumora::Core::PanicReason::InvalidBootInfo,
             "[FAIL] Multiboot 2 handoff or information tags are invalid");
    Lumora::Core::Log::Info("[OK] Multiboot 2 information parsed");
    if (boot_info.memory_count)
        Lumora::Core::Log::Info("[OK] Memory map entries parsed");
    else
        Lumora::Core::Log::Warning("No Multiboot memory map entries were provided");
    if (!boot_info.has_framebuffer || boot_info.framebuffer.type != 1 ||
        !boot_info.framebuffer.has_rgb_fields)
        Fail(Lumora::Core::PanicReason::FramebufferUnavailable,
             "[FAIL] No supported direct-RGB framebuffer tag was supplied");

    Lumora::Core::Log::Info("Mapping Multiboot framebuffer");
    Lumora::Arch::x86::FramebufferMapping mapping{};
    if (!Lumora::Arch::x86::MapFramebuffer(boot_info.framebuffer, mapping))
        Fail(Lumora::Core::PanicReason::FramebufferUnavailable,
             "[FAIL] Framebuffer range could not be mapped safely");
    const auto& source = boot_info.framebuffer;
    const Lumora::Graphics::FramebufferInfo format{
        source.width, source.height, source.pitch, source.bits_per_pixel,
        source.red_position, source.red_mask_size,
        source.green_position, source.green_mask_size,
        source.blue_position, source.blue_mask_size
    };
    if (!Lumora::Graphics::Console().Initialize(mapping.address, mapping.length, format))
        Fail(Lumora::Core::PanicReason::FramebufferUnavailable,
             "[FAIL] Framebuffer format, pitch, or channel masks are unsupported");

    Lumora::Console::BootScreen::Initialize();
    LogFramebufferMode(source);
    Lumora::Core::Log::Info("[OK] Linear framebuffer initialized");
    Lumora::Core::Log::Info("[OK] Framebuffer console attached; early boot log replayed");

    Lumora::Driver::InterruptController::PIC8259::Initialize();
    Lumora::Core::Log::Info("[OK] PIC8259 initialized with all IRQ lines masked");
    const auto keyboard = Lumora::Driver::Input::PS2Keyboard::Initialize();
    if (keyboard != Lumora::Driver::Input::PS2Keyboard::InitResult::Ready) {
        const char* reason = "[FAIL] PS/2 keyboard initialization failed";
        switch (keyboard) {
        case Lumora::Driver::Input::PS2Keyboard::InitResult::ControllerTimeout:
            reason = "[FAIL] PS/2 controller timed out"; break;
        case Lumora::Driver::Input::PS2Keyboard::InitResult::NoController:
            reason = "[FAIL] No usable PS/2 controller detected"; break;
        case Lumora::Driver::Input::PS2Keyboard::InitResult::InterfaceUnavailable:
            reason = "[FAIL] PS/2 keyboard interface unavailable"; break;
        case Lumora::Driver::Input::PS2Keyboard::InitResult::DeviceNoResponse:
            reason = "[FAIL] PS/2 keyboard did not acknowledge commands"; break;
        case Lumora::Driver::Input::PS2Keyboard::InitResult::DeviceRejectedCommand:
            reason = "[FAIL] PS/2 keyboard rejected scancode setup"; break;
        case Lumora::Driver::Input::PS2Keyboard::InitResult::IRQRegistrationFailed:
            reason = "[FAIL] Could not register the keyboard IRQ1 handler"; break;
        case Lumora::Driver::Input::PS2Keyboard::InitResult::Ready: break;
        }
        Fail(Lumora::Core::PanicReason::KernelInvariantViolation, reason);
    }
    Lumora::Core::Log::Info("[OK] PS/2 keyboard ready; IRQ1 handler registered and enabled");
    Lumora::Core::Log::Info("Initialization complete. Switching to interactive console.");

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

    Lumora::Console::Terminal::Initialize();
    for (;;) {
        // The interrupt gate runs with IF clear. Consume events with IF clear,
        // then use the STI shadow on HLT to avoid losing an IRQ between poll/sleep.
        asm volatile("cli" ::: "memory");
        Lumora::Driver::Input::PS2Keyboard::KeyEvent event{};
        if (!Lumora::Driver::Input::PS2Keyboard::ReadEvent(event)) {
            asm volatile("sti; hlt" ::: "memory");
            continue;
        }
        asm volatile("sti" ::: "memory");
#if defined(LUMORA_TEST_KEYBOARD)
        LogKeyboardEvent(event);
#endif
        Lumora::Console::Terminal::Handle(event);
    }
}
