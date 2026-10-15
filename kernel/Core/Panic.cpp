#include "Panic.hpp"

#include "../Graphics/Framebuffer.hpp"

namespace Lumora::Core {
namespace {
volatile uint32_t g_panicking = 0;

inline void Out8(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
inline uint8_t In8(uint16_t port) noexcept {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void SerialInitialize() noexcept {
    Out8(0x3F9, 0x00); Out8(0x3FB, 0x80);
    Out8(0x3F8, 0x03); Out8(0x3F9, 0x00);
    Out8(0x3FB, 0x03); Out8(0x3FA, 0xC7); Out8(0x3FC, 0x0B);
}
void SerialChar(char c) noexcept {
    for (uint32_t wait = 0; wait < 100000; ++wait) {
        if (In8(0x3FD) & 0x20) { Out8(0x3F8, static_cast<uint8_t>(c)); return; }
        asm volatile("pause");
    }
}
void SerialText(const char* s) noexcept {
    if (!s) s = "(unavailable)";
    for (size_t i = 0; i < 512 && s[i]; ++i) {
        if (s[i] == '\n') SerialChar('\r');
        SerialChar(s[i]);
    }
}
void SerialHex(uint64_t value) noexcept {
    static constexpr char digits[] = "0123456789ABCDEF";
    for (int shift = 60; shift >= 0; shift -= 4) SerialChar(digits[(value >> shift) & 0xF]);
}
void SerialDecimal(uint64_t value) noexcept {
    char digits[20]; size_t count = 0;
    do { digits[count++] = static_cast<char>('0' + value % 10); value /= 10; } while (value && count < sizeof(digits));
    while (count) SerialChar(digits[--count]);
}
const char* ReasonName(PanicReason reason) noexcept {
    switch (reason) {
    case PanicReason::InvalidBootInfo: return "INVALID BOOT INFO";
    case PanicReason::FramebufferUnavailable: return "FRAMEBUFFER UNAVAILABLE";
    case PanicReason::InvalidMemoryState: return "INVALID MEMORY STATE";
    case PanicReason::KernelInvariantViolation: return "KERNEL INVARIANT VIOLATION";
    case PanicReason::CPUException: return "CPU EXCEPTION";
    default: return "UNKNOWN PANIC";
    }
}

void SerialReport(PanicReason reason, const char* message, const char* file,
                  uint32_t line, const PanicContext* context) noexcept {
    SerialInitialize();
    SerialText("\n\nLUMORA KERNEL PANIC\nREASON: "); SerialText(ReasonName(reason));
    SerialText("\nCAUSE: "); SerialText(message);
    SerialText("\nSOURCE: "); SerialText(file);
    SerialText(":"); SerialDecimal(line);
    if (context && context->has_exception) {
        SerialText("\nEXCEPTION: "); SerialText(context->exception_name);
        SerialText("\nVECTOR: 0x"); SerialHex(context->vector);
        SerialText("\nERROR: ");
        if (context->has_error_code) { SerialText("0x"); SerialHex(context->error_code); }
        else SerialText("not pushed by CPU");
        SerialText("\nRIP: 0x"); SerialHex(context->rip);
    }
    SerialText("\nSYSTEM HALTED\n");
}

void DrawLabelValue(Graphics::Framebuffer& fb, int64_t y, const char* label,
                    uint64_t value) noexcept {
    constexpr Graphics::Color white{245, 239, 255};
    fb.DrawString(32, y, label, white);
    fb.DrawHex(190, y, value, 16, white);
}

void DrawReport(PanicReason reason, const char* message, const char* file,
                uint32_t line, const PanicContext* context) noexcept {
    using namespace Graphics;
    auto& fb = Console();
    if (!fb.IsReady()) return;
    constexpr Color background{35, 13, 58};
    constexpr Color panel{54, 25, 82};
    constexpr Color accent{205, 166, 255};
    constexpr Color white{250, 246, 255};
    constexpr Color muted{208, 190, 226};
    fb.Clear(background);
    const int64_t margin = fb.Width() < 480 ? 12 : 28;
    fb.FillRect(margin, 18, static_cast<int64_t>(fb.Width()) - margin * 2, 56, panel);
    fb.DrawString(margin + 12, 35, "LUMORA KERNEL PANIC", accent,
                  fb.Width() >= 520 ? 2 : 1);
    fb.DrawHorizontalLine(margin, 88, static_cast<int64_t>(fb.Width()) - margin * 2, accent);

    int64_t y = 108;
    fb.DrawString(margin, y, "REASON:", accent);
    fb.DrawString(margin + 96, y, ReasonName(reason), white);
    y += 28;
    fb.DrawString(margin, y, "CAUSE:", accent);
    fb.DrawString(margin + 96, y, message ? message : "(unavailable)", white);
    y += 28;
    fb.DrawString(margin, y, "SOURCE:", accent);
    fb.DrawString(margin + 96, y, file ? file : "(unavailable)", muted);
    y += 20;
    fb.DrawString(margin + 96, y, "LINE:", muted);
    fb.DrawDecimal(margin + 144, y, line, white);
    y += 30;

    if (context && context->has_exception) {
        fb.FillRect(margin, y - 5, static_cast<int64_t>(fb.Width()) - margin * 2, 1, accent);
        fb.DrawString(margin, y, "EXCEPTION:", accent);
        fb.DrawString(margin + 116, y, context->exception_name ? context->exception_name : "Unknown exception", white);
        y += 22;
        DrawLabelValue(fb, y, "VECTOR:", context->vector); y += 20;
        fb.DrawString(margin, y, "ERROR CODE:", accent);
        if (context->has_error_code) fb.DrawHex(margin + 116, y, context->error_code, 16, white);
        else fb.DrawString(margin + 116, y, "NOT PUSHED BY CPU", muted);
        y += 20;
        DrawLabelValue(fb, y, "FAULT RIP:", context->rip); y += 24;
        if (context->registers) {
            fb.DrawString(margin, y, "RAX:", accent); fb.DrawHex(margin + 54, y, context->registers[14], 16, white);
            fb.DrawString(margin + 250, y, "RBX:", accent); fb.DrawHex(margin + 304, y, context->registers[13], 16, white);
            y += 20;
            fb.DrawString(margin, y, "RCX:", accent); fb.DrawHex(margin + 54, y, context->registers[12], 16, white);
            fb.DrawString(margin + 250, y, "RDX:", accent); fb.DrawHex(margin + 304, y, context->registers[11], 16, white);
        }
    }
    const int64_t footer_y = static_cast<int64_t>(fb.Height()) - 30;
    fb.DrawHorizontalLine(margin, footer_y - 8, static_cast<int64_t>(fb.Width()) - margin * 2, accent);
    fb.DrawString(margin, footer_y, "THE SYSTEM HAS HALTED. POWER CYCLE TO RESTART.", white);
}

[[noreturn]] void Halt() noexcept {
    for (;;) asm volatile("cli; hlt" ::: "memory");
}
}

[[noreturn]] void Panic(PanicReason reason, const char* message,
                        const char* file, uint32_t line,
                        const PanicContext* context) noexcept {
    asm volatile("cli" ::: "memory");
    if (__atomic_exchange_n(&g_panicking, 1, __ATOMIC_ACQ_REL)) {
        // The first panic path may have failed in serial or framebuffer I/O;
        // do not touch either device again from the recursive path.
        Halt();
    }
    SerialReport(reason, message, file, line, context);
    DrawReport(reason, message, file, line, context);
    Halt();
}
} // namespace Lumora::Core
