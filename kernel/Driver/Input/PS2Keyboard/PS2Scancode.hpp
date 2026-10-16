#pragma once
#include <cstdint>

namespace Lumora::Driver::Input::PS2Keyboard {

enum class KeyCode : uint16_t {
    Unknown, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    Enter, Backspace, Tab, Space, Escape, Left, Right, Up, Down, Home, End, PageUp, PageDown, Insert, Delete,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
    CapsLock, NumLock, ScrollLock,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    PrintScreen, Pause, Keypad0, Keypad1, Keypad2, Keypad3, Keypad4,
    Keypad5, Keypad6, Keypad7, Keypad8, Keypad9, KeypadDecimal,
    KeypadPlus, KeypadMinus, KeypadMultiply, KeypadDivide, KeypadEnter,
    Minus, Equal, LeftBracket, RightBracket, Semicolon, Apostrophe,
    Grave, Backslash, Comma, Period, Slash
};

struct KeyEvent {
    KeyCode key = KeyCode::Unknown;
    char character = 0;
    bool pressed = false;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool caps_lock = false;
    bool num_lock = false;
    bool scroll_lock = false;
};

// Set 1 decoder. Feed one translated controller byte at a time; prefixes and
// modifier state are retained between calls. Returns true when an event exists.
class ScancodeDecoder final {
public:
    bool Feed(uint8_t byte, KeyEvent& event) noexcept;
private:
    bool extended_ = false;
    uint8_t pause_bytes_ = 0;
    bool left_shift_ = false, right_shift_ = false, caps_lock_ = false;
    bool left_control_ = false, right_control_ = false;
    bool left_alt_ = false, right_alt_ = false;
    bool num_lock_ = false, scroll_lock_ = false;
};

// US QWERTY character policy is deliberately separate from physical scan decoding.
char USCharacter(const KeyEvent& event) noexcept;

} // namespace Lumora::Driver::Input::PS2Keyboard
