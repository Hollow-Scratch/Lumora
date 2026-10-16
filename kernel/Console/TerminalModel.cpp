#include "TerminalModel.hpp"
#include "../Version.hpp"

namespace Lumora::Console {
namespace {
using KeyCode = Lumora::Driver::Input::PS2Keyboard::KeyCode;
bool Equal(const char* a, const char* b) noexcept {
    if (!a || !b) return false;
    size_t i = 0;
    while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == b[i];
}
bool EchoPrefix(const char* command) noexcept {
    constexpr char prefix[] = "echo ";
    if (!command) return false;
    size_t i = 0;
    while (prefix[i] && command[i] && prefix[i] == command[i]) ++i;
    return prefix[i] == '\0';
}
void Emit(Output output, void* context, const char* text,
          Lumora::Core::Log::Level level = Lumora::Core::Log::Level::Info) noexcept {
    if (output) output(level, text, context);
}
}

void LineEditor::Recall(int direction) noexcept {
    if (!history_count_) return;
    if (direction < 0) {
        if (history_position_ > 0) --history_position_;
    } else if (history_position_ < history_count_) ++history_position_;
    const char* source = history_position_ == history_count_ ? "" : history_[history_position_];
    size_t i = 0;
    while (i + 1 < LineCapacity && source[i]) { line_[i] = source[i]; ++i; }
    line_[i] = '\0'; length_ = cursor_ = i;
}

void LineEditor::SaveHistory() noexcept {
    if (!length_) return;
    if (history_count_ && Equal(history_[history_count_ - 1], line_)) return;
    if (history_count_ == HistoryCapacity) {
        for (size_t row = 1; row < HistoryCapacity; ++row)
            for (size_t col = 0; col < LineCapacity; ++col) history_[row - 1][col] = history_[row][col];
        --history_count_;
    }
    for (size_t i = 0; i <= length_; ++i) history_[history_count_][i] = line_[i];
    ++history_count_;
}

LineEditor::Result LineEditor::Handle(const Lumora::Driver::Input::PS2Keyboard::KeyEvent& event,
                                     char* submitted, size_t capacity) noexcept {
    if (!event.pressed) return Result::None;
    KeyCode key = event.key;
    if (event.character == 0 && event.num_lock == event.shift) {
        switch (key) {
        case KeyCode::Keypad4: key = KeyCode::Left; break;
        case KeyCode::Keypad6: key = KeyCode::Right; break;
        case KeyCode::Keypad8: key = KeyCode::Up; break;
        case KeyCode::Keypad2: key = KeyCode::Down; break;
        case KeyCode::Keypad7: key = KeyCode::Home; break;
        case KeyCode::Keypad1: key = KeyCode::End; break;
        case KeyCode::Keypad9: key = KeyCode::PageUp; break;
        case KeyCode::Keypad3: key = KeyCode::PageDown; break;
        case KeyCode::Keypad0: key = KeyCode::Insert; break;
        case KeyCode::KeypadDecimal: key = KeyCode::Delete; break;
        default: break;
        }
    }
    switch (key) {
    case KeyCode::Left:
        if (cursor_) { --cursor_; return Result::Changed; }
        return Result::None;
    case KeyCode::Right:
        if (cursor_ < length_) { ++cursor_; return Result::Changed; }
        return Result::None;
    case KeyCode::Home: cursor_ = 0; return Result::Changed;
    case KeyCode::End: cursor_ = length_; return Result::Changed;
    case KeyCode::Up: Recall(-1); return Result::Changed;
    case KeyCode::Down: Recall(1); return Result::Changed;
    case KeyCode::Backspace:
        if (!cursor_) return Result::None;
        for (size_t i = cursor_; i < length_; ++i) line_[i - 1] = line_[i];
        --cursor_; --length_; line_[length_] = '\0'; return Result::Changed;
    case KeyCode::Delete:
        if (cursor_ >= length_) return Result::None;
        for (size_t i = cursor_ + 1; i <= length_; ++i) line_[i - 1] = line_[i];
        --length_; return Result::Changed;
    case KeyCode::Enter: {
        if (submitted && capacity) {
            size_t n = length_ < capacity - 1 ? length_ : capacity - 1;
            for (size_t i = 0; i < n; ++i) submitted[i] = line_[i];
            submitted[n] = '\0';
        }
        SaveHistory();
        length_ = cursor_ = 0; line_[0] = '\0'; history_position_ = history_count_;
        return Result::Submitted;
    }
    default: break;
    }
    const char c = event.character;
    if (c < 32 || c > 126 || length_ >= LineCapacity - 1) return Result::None;
    for (size_t i = length_ + 1; i > cursor_; --i) line_[i] = line_[i - 1];
    line_[cursor_++] = c; ++length_;
    return Result::Changed;
}

CommandResult ExecuteBuiltin(const char* command, Output output, void* context) noexcept {
    if (!command || !*command) return CommandResult::Continue;
    if (Equal(command, "help")) {
        Emit(output, context, "Commands: help clear about version logs echo <text>");
    } else if (Equal(command, "clear")) {
        return CommandResult::Clear;
    } else if (Equal(command, "about")) {
        Emit(output, context, "Lumora experimental freestanding x86-64 kernel.");
        Emit(output, context, "Kernel version:");
        Emit(output, context, Lumora::KernelVersion);
    } else if (Equal(command, "version")) {
        Emit(output, context, "Lumora kernel version:");
        Emit(output, context, Lumora::KernelVersion);
    } else if (EchoPrefix(command)) {
        Emit(output, context, command + 5);
    } else if (Equal(command, "logs")) {
        const auto& history = Lumora::Core::Log::Entries();
        for (size_t i = 0; i < history.Count(); ++i) {
            const auto* entry = history.At(i);
            if (entry) Emit(output, context, entry->text, entry->level);
        }
    } else {
        Emit(output, context, "Unknown command. Type 'help' for available commands.", Lumora::Core::Log::Level::Warning);
    }
    return CommandResult::Continue;
}

} // namespace Lumora::Console
