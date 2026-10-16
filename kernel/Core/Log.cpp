#include "Log.hpp"

namespace Lumora::Core::Log {
namespace {
History history;
Sink output_sink = nullptr;
#if !__STDC_HOSTED__
bool serial_checked = false;
bool serial_available = false;

void Out8(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
uint8_t In8(uint16_t port) noexcept {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
void SerialInit() noexcept {
    if (serial_checked) return;
    serial_checked = true;
    Out8(0x3FF, 0x5A);
    if (In8(0x3FF) != 0x5A) return;
    Out8(0x3F9, 0); Out8(0x3FB, 0x80); Out8(0x3F8, 1);
    Out8(0x3F9, 0); Out8(0x3FB, 3); Out8(0x3FA, 0xC7); Out8(0x3FC, 0x0B);
    serial_available = true;
}
void SerialChar(char c) noexcept {
    for (uint32_t n = 0; n < 10000; ++n) {
        if (In8(0x3FD) & 0x20) { Out8(0x3F8, static_cast<uint8_t>(c)); return; }
        asm volatile("pause");
    }
}
void SerialEntry(const Entry& entry) noexcept {
    SerialInit();
    if (!serial_available) return;
    const char* label = entry.level == Level::Info ? "INFO" :
        (entry.level == Level::Warning ? "WARN" : "ERROR");
    SerialChar('[');
    for (size_t i = 0; label[i]; ++i) SerialChar(label[i]);
    SerialChar(']'); SerialChar(' ');
    for (size_t i = 0; i < sizeof(entry.text) && entry.text[i]; ++i) {
        if (entry.text[i] == '\n') SerialChar('\r');
        SerialChar(entry.text[i]);
    }
    SerialChar('\r'); SerialChar('\n');
}
#endif
}

void History::Push(Level level, const char* text) noexcept {
    Entry& entry = entries_[next_];
    entry.level = level;
    entry.sequence = ++sequence_;
    size_t i = 0;
    if (text) {
        while (i + 1 < sizeof(entry.text) && text[i]) { entry.text[i] = text[i]; ++i; }
        if (text[i]) {
            constexpr char ellipsis[] = "...";
            const size_t start = sizeof(entry.text) - sizeof(ellipsis);
            for (size_t j = 0; j < sizeof(ellipsis); ++j) entry.text[start + j] = ellipsis[j];
        } else entry.text[i] = '\0';
    }
    if (!text) entry.text[0] = '\0';
    next_ = (next_ + 1) % Capacity;
    if (count_ < Capacity) ++count_;
}

const Entry* History::At(size_t index) const noexcept {
    if (index >= count_) return nullptr;
    const size_t first = (next_ + Capacity - count_) % Capacity;
    return &entries_[(first + index) % Capacity];
}

size_t History::FirstVisible(size_t rows, size_t scrollback) const noexcept {
    if (rows >= count_) return 0;
    const size_t max_back = count_ - rows;
    if (scrollback > max_back) scrollback = max_back;
    return count_ - rows - scrollback;
}

void SetSink(Sink sink, bool replay) noexcept {
    output_sink = sink;
    if (sink && replay) {
        for (size_t i = 0; i < history.Count(); ++i) {
            const Entry* entry = history.At(i);
            if (entry) sink(*entry);
        }
    }
}

void Write(Level level, const char* message) noexcept {
    history.Push(level, message);
    const Entry* entry = history.At(history.Count() - 1);
    if (entry) {
#if !__STDC_HOSTED__
        SerialEntry(*entry);
#endif
        if (output_sink) output_sink(*entry);
    }
}
void Info(const char* message) noexcept { Write(Level::Info, message); }
void Warning(const char* message) noexcept { Write(Level::Warning, message); }
void Error(const char* message) noexcept { Write(Level::Error, message); }
const History& Entries() noexcept { return history; }

} // namespace Lumora::Core::Log
