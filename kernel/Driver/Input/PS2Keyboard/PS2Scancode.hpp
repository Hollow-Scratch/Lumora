#pragma once
#include <cstdint>

namespace Lumora::Driver::Input::PS2Keyboard {

enum class KeyCode : uint16_t {
    Unknown, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    Enter, Backspace, Tab, Space, Escape, Left, Right, Up, Down, Home, End, PageUp, PageDown, Insert, Delete
};

struct KeyEvent {
    KeyCode key = KeyCode::Unknown;
    char character = 0;
    bool pressed = false;
    bool shift = false;
    bool caps_lock = false;
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
};

} // namespace Lumora::Driver::Input::PS2Keyboard
