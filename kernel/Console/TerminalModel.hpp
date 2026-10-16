#pragma once
#include <cstddef>
#include <cstdint>
#include "../Core/Log.hpp"
#include "../Driver/Input/PS2Keyboard/PS2Scancode.hpp"

namespace Lumora::Console {

class LineEditor final {
public:
    static constexpr size_t LineCapacity = 80;
    static constexpr size_t HistoryCapacity = 8;
    enum class Result : uint8_t { None, Changed, Submitted };
    Result Handle(const Lumora::Driver::Input::PS2Keyboard::KeyEvent& event,
                  char* submitted, size_t submitted_capacity) noexcept;
    const char* Text() const noexcept { return line_; }
    size_t Length() const noexcept { return length_; }
    size_t Cursor() const noexcept { return cursor_; }
private:
    void Recall(int direction) noexcept;
    void SaveHistory() noexcept;
    char line_[LineCapacity]{};
    char history_[HistoryCapacity][LineCapacity]{};
    size_t length_ = 0, cursor_ = 0, history_count_ = 0, history_position_ = 0;
};

using Output = void (*)(Lumora::Core::Log::Level level, const char* text, void* context) noexcept;
enum class CommandResult : uint8_t { Continue, Clear };
CommandResult ExecuteBuiltin(const char* command, Output output, void* context) noexcept;

} // namespace Lumora::Console
