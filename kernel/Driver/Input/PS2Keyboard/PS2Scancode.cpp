#include "PS2Scancode.hpp"

namespace Lumora::Driver::Input::PS2Keyboard {
namespace {
using K = KeyCode;
struct Mapping { KeyCode key; char plain; char shifted; };
constexpr Mapping base[0x3B] = {
    {K::Unknown,0,0}, {K::Escape,0,0},
    {K::Digit1,'1','!'}, {K::Digit2,'2','@'}, {K::Digit3,'3','#'},
    {K::Digit4,'4','$'}, {K::Digit5,'5','%'}, {K::Digit6,'6','^'},
    {K::Digit7,'7','&'}, {K::Digit8,'8','*'}, {K::Digit9,'9','('},
    {K::Digit0,'0',')'}, {K::Minus,'-','_'}, {K::Equal,'=','+'},
    {K::Backspace,0,0}, {K::Tab,0,0},
    {K::Q,0,0}, {K::W,0,0}, {K::E,0,0}, {K::R,0,0}, {K::T,0,0}, {K::Y,0,0},
    {K::U,0,0}, {K::I,0,0}, {K::O,0,0}, {K::P,0,0}, {K::LeftBracket,0,0},
    {K::RightBracket,0,0}, {K::Enter,0,0}, {K::LeftControl,0,0},
    {K::A,0,0}, {K::S,0,0}, {K::D,0,0}, {K::F,0,0}, {K::G,0,0}, {K::H,0,0},
    {K::J,0,0}, {K::K,0,0}, {K::L,0,0}, {K::Semicolon,0,0},
    {K::Apostrophe,0,0}, {K::Grave,0,0}, {K::LeftShift,0,0},
    {K::Backslash,0,0}, {K::Z,0,0}, {K::X,0,0}, {K::C,0,0}, {K::V,0,0},
    {K::B,0,0}, {K::N,0,0}, {K::M,0,0}, {K::Comma,0,0},
    {K::Period,0,0}, {K::Slash,0,0}, {K::RightShift,0,0},
    {K::KeypadMultiply,0,0}, {K::LeftAlt,0,0}, {K::Space,0,0}
};

KeyCode KeypadKey(uint8_t code) noexcept {
    switch (code) {
    case 0x47: return K::Keypad7; case 0x48: return K::Keypad8;
    case 0x49: return K::Keypad9; case 0x4A: return K::KeypadMinus;
    case 0x4B: return K::Keypad4; case 0x4C: return K::Keypad5;
    case 0x4D: return K::Keypad6; case 0x4E: return K::KeypadPlus;
    case 0x4F: return K::Keypad1; case 0x50: return K::Keypad2;
    case 0x51: return K::Keypad3; case 0x52: return K::Keypad0;
    case 0x53: return K::KeypadDecimal; default: return K::Unknown;
    }
}
void FillModifiers(KeyEvent& event, bool shift, bool control, bool alt,
                   bool caps, bool num, bool scroll) noexcept {
    event.shift = shift; event.control = control; event.alt = alt;
    event.caps_lock = caps; event.num_lock = num; event.scroll_lock = scroll;
}
}

bool ScancodeDecoder::Feed(uint8_t byte, KeyEvent& event) noexcept {
    event = {};
    if (pause_bytes_) { --pause_bytes_; return false; }
    if (byte == 0xE1) {
        extended_ = false;
        pause_bytes_ = 5;
        event.key = K::Pause; event.pressed = true;
        FillModifiers(event, left_shift_ || right_shift_, left_control_ || right_control_,
                      left_alt_ || right_alt_, caps_lock_, num_lock_, scroll_lock_);
        return true;
    }
    if (byte == 0xE0) { extended_ = true; return false; }

    const bool released = (byte & 0x80) != 0;
    const uint8_t code = byte & 0x7F;
    const bool ext = extended_;
    extended_ = false;
    KeyCode key = K::Unknown;

    if (ext) {
        // E0 2A/E0 36 and E0 AA/E0 B6 are fake shifts in Print Screen sequences.
        if (code == 0x2A || code == 0x36) return false;
        switch (code) {
        case 0x1C: key = K::KeypadEnter; break;
        case 0x1D: key = K::RightControl; if (released) right_control_ = false; else right_control_ = true; break;
        case 0x38: key = K::RightAlt; if (released) right_alt_ = false; else right_alt_ = true; break;
        case 0x35: key = K::KeypadDivide; break;
        case 0x37: key = K::PrintScreen; break;
        case 0x47: key = K::Home; break; case 0x48: key = K::Up; break;
        case 0x49: key = K::PageUp; break; case 0x4B: key = K::Left; break;
        case 0x4D: key = K::Right; break; case 0x4F: key = K::End; break;
        case 0x50: key = K::Down; break; case 0x51: key = K::PageDown; break;
        case 0x52: key = K::Insert; break; case 0x53: key = K::Delete; break;
        default: key = K::Unknown; break;
        }
    } else {
        if (code == 0x2A || code == 0x36) {
            if (code == 0x2A) left_shift_ = !released; else right_shift_ = !released;
            key = code == 0x2A ? K::LeftShift : K::RightShift;
        } else if (code == 0x1D) { left_control_ = !released; key = K::LeftControl; }
        else if (code == 0x38) { left_alt_ = !released; key = K::LeftAlt; }
        else if (code == 0x3A) { if (!released) caps_lock_ = !caps_lock_; key = K::CapsLock; }
        else if (code == 0x45) { if (!released) num_lock_ = !num_lock_; key = K::NumLock; }
        else if (code == 0x46) { if (!released) scroll_lock_ = !scroll_lock_; key = K::ScrollLock; }
        else if (code >= 0x3B && code <= 0x44) key = static_cast<KeyCode>(static_cast<uint16_t>(K::F1) + code - 0x3B);
        else if (code == 0x57) key = K::F11;
        else if (code == 0x58) key = K::F12;
        else if (code >= 0x47 && code <= 0x53) key = KeypadKey(code);
        else if (code == 0x37) key = K::KeypadMultiply;
        else if (code < sizeof(base) / sizeof(base[0])) key = base[code].key;
    }

    event.key = key;
    event.pressed = !released;
    FillModifiers(event, left_shift_ || right_shift_, left_control_ || right_control_,
                  left_alt_ || right_alt_, caps_lock_, num_lock_, scroll_lock_);
    return true;
}

char USCharacter(const KeyEvent& event) noexcept {
    if (!event.pressed) return 0;
    const bool shifted = event.shift;
    switch (event.key) {
    case K::A: return (shifted != event.caps_lock) ? 'A' : 'a';
    case K::B: return (shifted != event.caps_lock) ? 'B' : 'b';
    case K::C: return (shifted != event.caps_lock) ? 'C' : 'c';
    case K::D: return (shifted != event.caps_lock) ? 'D' : 'd';
    case K::E: return (shifted != event.caps_lock) ? 'E' : 'e';
    case K::F: return (shifted != event.caps_lock) ? 'F' : 'f';
    case K::G: return (shifted != event.caps_lock) ? 'G' : 'g';
    case K::H: return (shifted != event.caps_lock) ? 'H' : 'h';
    case K::I: return (shifted != event.caps_lock) ? 'I' : 'i';
    case K::J: return (shifted != event.caps_lock) ? 'J' : 'j';
    case K::K: return (shifted != event.caps_lock) ? 'K' : 'k';
    case K::L: return (shifted != event.caps_lock) ? 'L' : 'l';
    case K::M: return (shifted != event.caps_lock) ? 'M' : 'm';
    case K::N: return (shifted != event.caps_lock) ? 'N' : 'n';
    case K::O: return (shifted != event.caps_lock) ? 'O' : 'o';
    case K::P: return (shifted != event.caps_lock) ? 'P' : 'p';
    case K::Q: return (shifted != event.caps_lock) ? 'Q' : 'q';
    case K::R: return (shifted != event.caps_lock) ? 'R' : 'r';
    case K::S: return (shifted != event.caps_lock) ? 'S' : 's';
    case K::T: return (shifted != event.caps_lock) ? 'T' : 't';
    case K::U: return (shifted != event.caps_lock) ? 'U' : 'u';
    case K::V: return (shifted != event.caps_lock) ? 'V' : 'v';
    case K::W: return (shifted != event.caps_lock) ? 'W' : 'w';
    case K::X: return (shifted != event.caps_lock) ? 'X' : 'x';
    case K::Y: return (shifted != event.caps_lock) ? 'Y' : 'y';
    case K::Z: return (shifted != event.caps_lock) ? 'Z' : 'z';
    case K::Digit1: return shifted ? '!' : '1'; case K::Digit2: return shifted ? '@' : '2';
    case K::Digit3: return shifted ? '#' : '3'; case K::Digit4: return shifted ? '$' : '4';
    case K::Digit5: return shifted ? '%' : '5'; case K::Digit6: return shifted ? '^' : '6';
    case K::Digit7: return shifted ? '&' : '7'; case K::Digit8: return shifted ? '*' : '8';
    case K::Digit9: return shifted ? '(' : '9'; case K::Digit0: return shifted ? ')' : '0';
    case K::Minus: return shifted ? '_' : '-'; case K::Equal: return shifted ? '+' : '=';
    case K::LeftBracket: return shifted ? '{' : '['; case K::RightBracket: return shifted ? '}' : ']';
    case K::Semicolon: return shifted ? ':' : ';'; case K::Apostrophe: return shifted ? '"' : '\'';
    case K::Grave: return shifted ? '~' : '`'; case K::Backslash: return shifted ? '|' : '\\';
    case K::Comma: return shifted ? '<' : ','; case K::Period: return shifted ? '>' : '.';
    case K::Slash: return shifted ? '?' : '/';
    case K::Enter: case K::KeypadEnter: return '\n';
    case K::Backspace: return '\b'; case K::Tab: return '\t'; case K::Space: return ' ';
    case K::KeypadMultiply: return '*'; case K::KeypadDivide: return '/';
    case K::KeypadPlus: return '+'; case K::KeypadMinus: return '-';
    case K::Keypad0: return (event.num_lock != shifted) ? '0' : 0;
    case K::Keypad1: return (event.num_lock != shifted) ? '1' : 0;
    case K::Keypad2: return (event.num_lock != shifted) ? '2' : 0;
    case K::Keypad3: return (event.num_lock != shifted) ? '3' : 0;
    case K::Keypad4: return (event.num_lock != shifted) ? '4' : 0;
    case K::Keypad5: return (event.num_lock != shifted) ? '5' : 0;
    case K::Keypad6: return (event.num_lock != shifted) ? '6' : 0;
    case K::Keypad7: return (event.num_lock != shifted) ? '7' : 0;
    case K::Keypad8: return (event.num_lock != shifted) ? '8' : 0;
    case K::Keypad9: return (event.num_lock != shifted) ? '9' : 0;
    case K::KeypadDecimal: return (event.num_lock != shifted) ? '.' : 0;
    default: return 0;
    }
}

} // namespace Lumora::Driver::Input::PS2Keyboard
