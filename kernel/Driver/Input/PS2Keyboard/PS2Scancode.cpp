#include "PS2Scancode.hpp"

namespace Lumora::Driver::Input::PS2Keyboard {
namespace {
struct Mapping { KeyCode key; char plain; char shifted; };
constexpr Mapping set1[0x3B] = {
    {KeyCode::Unknown,0,0}, {KeyCode::Escape,0,0},
    {KeyCode::Digit1,'1','!'}, {KeyCode::Digit2,'2','@'}, {KeyCode::Digit3,'3','#'},
    {KeyCode::Digit4,'4','$'}, {KeyCode::Digit5,'5','%'}, {KeyCode::Digit6,'6','^'},
    {KeyCode::Digit7,'7','&'}, {KeyCode::Digit8,'8','*'}, {KeyCode::Digit9,'9','('},
    {KeyCode::Digit0,'0',')'}, {KeyCode::Unknown,'-','_'}, {KeyCode::Unknown,'=','+'},
    {KeyCode::Backspace,'\b','\b'}, {KeyCode::Tab,'\t','\t'},
    {KeyCode::Q,'q','Q'}, {KeyCode::W,'w','W'}, {KeyCode::E,'e','E'}, {KeyCode::R,'r','R'},
    {KeyCode::T,'t','T'}, {KeyCode::Y,'y','Y'}, {KeyCode::U,'u','U'}, {KeyCode::I,'i','I'},
    {KeyCode::O,'o','O'}, {KeyCode::P,'p','P'}, {KeyCode::Unknown,'[','{'}, {KeyCode::Unknown,']','}'},
    {KeyCode::Enter,'\n','\n'}, {KeyCode::Unknown,0,0}, {KeyCode::A,'a','A'}, {KeyCode::S,'s','S'},
    {KeyCode::D,'d','D'}, {KeyCode::F,'f','F'}, {KeyCode::G,'g','G'}, {KeyCode::H,'h','H'},
    {KeyCode::J,'j','J'}, {KeyCode::K,'k','K'}, {KeyCode::L,'l','L'}, {KeyCode::Unknown,';',':'},
    {KeyCode::Unknown,'\'', '"'}, {KeyCode::Unknown,'`','~'}, {KeyCode::Unknown,0,0},
    {KeyCode::Unknown,'\\','|'}, {KeyCode::Z,'z','Z'}, {KeyCode::X,'x','X'}, {KeyCode::C,'c','C'},
    {KeyCode::V,'v','V'}, {KeyCode::B,'b','B'}, {KeyCode::N,'n','N'}, {KeyCode::M,'m','M'},
    {KeyCode::Unknown,',','<'}, {KeyCode::Unknown,'.','>'}, {KeyCode::Unknown,'/','?'},
    {KeyCode::Unknown,0,0}, {KeyCode::Unknown,'*','*'}, {KeyCode::Unknown,0,0}, {KeyCode::Space,' ',' '}
};
}

bool ScancodeDecoder::Feed(uint8_t byte, KeyEvent& event) noexcept {
    event = {};
    if (pause_bytes_) { --pause_bytes_; return false; }
    if (byte == 0xE1) { extended_ = false; pause_bytes_ = 5; return false; }
    if (byte == 0xE0) { extended_ = true; return false; }

    const bool released = (byte & 0x80) != 0;
    const uint8_t code = byte & 0x7F;
    const bool ext = extended_;
    extended_ = false;

    if (!ext && (code == 0x2A || code == 0x36)) {
        if (code == 0x2A) left_shift_ = !released; else right_shift_ = !released;
        event.key = KeyCode::Unknown; event.pressed = !released;
        event.shift = left_shift_ || right_shift_; event.caps_lock = caps_lock_;
        return true;
    }
    if (!ext && code == 0x3A && !released) caps_lock_ = !caps_lock_;
    event.pressed = !released;
    event.shift = left_shift_ || right_shift_;
    event.caps_lock = caps_lock_;
    if (ext) {
        switch (code) {
        case 0x4B: event.key = KeyCode::Left; break;
        case 0x4D: event.key = KeyCode::Right; break;
        case 0x48: event.key = KeyCode::Up; break;
        case 0x50: event.key = KeyCode::Down; break;
        case 0x47: event.key = KeyCode::Home; break;
        case 0x4F: event.key = KeyCode::End; break;
        case 0x49: event.key = KeyCode::PageUp; break;
        case 0x51: event.key = KeyCode::PageDown; break;
        case 0x52: event.key = KeyCode::Insert; break;
        case 0x53: event.key = KeyCode::Delete; break;
        case 0x1C: event.key = KeyCode::Enter; if (!released) event.character = '\n'; break;
        default: event.key = KeyCode::Unknown; break;
        }
        return true;
    }
    if (code == 0x3A) { event.key = KeyCode::Unknown; return true; }
    if (code >= sizeof(set1) / sizeof(set1[0])) { event.key = KeyCode::Unknown; return true; }
    const Mapping& m = set1[code];
    event.key = m.key;
    if (!released) {
        if (m.plain >= 'a' && m.plain <= 'z')
            event.character = (event.shift != caps_lock_) ? m.shifted : m.plain;
        else event.character = event.shift ? m.shifted : m.plain;
    }
    return true;
}
} // namespace Lumora::Driver::Input::PS2Keyboard
