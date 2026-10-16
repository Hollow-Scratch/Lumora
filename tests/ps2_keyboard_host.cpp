#include <cassert>
#include <cstdint>
#include "../kernel/Driver/Input/PS2Keyboard/PS2Keyboard.hpp"

using namespace Lumora::Driver::Input::PS2Keyboard;

static KeyEvent Feed(ScancodeDecoder& decoder, uint8_t byte, bool expected = true) {
    KeyEvent event{};
    const bool actual = decoder.Feed(byte, event);
    assert(actual == expected);
    if (actual) event.character = USCharacter(event);
    return event;
}

int main() {
    ScancodeDecoder decoder;
    auto e = Feed(decoder, 0x1E); // a
    assert(e.key == KeyCode::A && e.character == 'a' && e.pressed);
    e = Feed(decoder, 0x9E);
    assert(e.key == KeyCode::A && e.character == 0 && !e.pressed);
    Feed(decoder, 0x2A); // left shift down
    e = Feed(decoder, 0x1E);
    assert(e.character == 'A' && e.shift);
    Feed(decoder, 0xAA);
    Feed(decoder, 0x3A); // caps on
    e = Feed(decoder, 0x1E);
    assert(e.character == 'A' && e.caps_lock);
    Feed(decoder, 0x2A);
    e = Feed(decoder, 0x1E);
    assert(e.character == 'a' && e.shift && e.caps_lock);
    Feed(decoder, 0xAA);
    Feed(decoder, 0xBA); // caps release does not toggle
    e = Feed(decoder, 0x02);
    assert(e.key == KeyCode::Digit1 && e.character == '1');
    Feed(decoder, 0x2A);
    e = Feed(decoder, 0x02);
    assert(e.character == '!');
    Feed(decoder, 0xAA);
    assert(!decoder.Feed(0xE0, e));
    e = Feed(decoder, 0x4B);
    assert(e.key == KeyCode::Left && e.character == 0 && e.pressed);
    assert(!decoder.Feed(0xE0, e));
    e = Feed(decoder, 0xCB);
    assert(e.key == KeyCode::Left && !e.pressed && e.character == 0);
    e = Feed(decoder, 0xE1);
    assert(e.key == KeyCode::Pause && e.pressed);
    for (int i = 0; i < 5; ++i) assert(!decoder.Feed(0, e));
    e = Feed(decoder, 0x1C);
    assert(e.key == KeyCode::Enter && e.character == '\n');
    e = Feed(decoder, 0x0E);
    assert(e.key == KeyCode::Backspace && e.character == '\b');
    e = Feed(decoder, 0x0F);
    assert(e.key == KeyCode::Tab && e.character == '\t');
    e = Feed(decoder, 0x39);
    assert(e.key == KeyCode::Space && e.character == ' ');
    e = Feed(decoder, 0x0C);
    assert(e.key == KeyCode::Minus && e.character == '-');
    Feed(decoder, 0x2A);
    e = Feed(decoder, 0x0C);
    assert(e.character == '_');
    Feed(decoder, 0xAA);
    e = Feed(decoder, 0x3B);
    assert(e.key == KeyCode::F1 && e.pressed);
    e = Feed(decoder, 0x57);
    assert(e.key == KeyCode::F11);
    e = Feed(decoder, 0x58);
    assert(e.key == KeyCode::F12);
    e = Feed(decoder, 0x1D);
    assert(e.key == KeyCode::LeftControl && e.control);
    e = Feed(decoder, 0x38);
    assert(e.key == KeyCode::LeftAlt && e.alt && e.control);
    Feed(decoder, 0x9D); Feed(decoder, 0xB8);
    assert(!decoder.Feed(0xE0, e)); e = Feed(decoder, 0x1D);
    assert(e.key == KeyCode::RightControl && e.control);
    assert(!decoder.Feed(0xE0, e)); e = Feed(decoder, 0x9D);
    assert(e.key == KeyCode::RightControl && !e.control && !e.pressed);
    assert(!decoder.Feed(0xE0, e)); e = Feed(decoder, 0x38);
    assert(e.key == KeyCode::RightAlt && e.alt);
    assert(!decoder.Feed(0xE0, e)); e = Feed(decoder, 0xB8);
    assert(e.key == KeyCode::RightAlt && !e.alt && !e.pressed);
    e = Feed(decoder, 0x46);
    assert(e.key == KeyCode::ScrollLock && e.scroll_lock);
    e = Feed(decoder, 0xC6);
    assert(e.key == KeyCode::ScrollLock && e.scroll_lock && !e.pressed);
    e = Feed(decoder, 0x45);
    assert(e.key == KeyCode::NumLock && e.num_lock);
    e = Feed(decoder, 0x47);
    assert(e.key == KeyCode::Keypad7 && e.character == '7');
    assert(!decoder.Feed(0xE0, e));
    assert(!decoder.Feed(0x2A, e));
    assert(!decoder.Feed(0xE0, e));
    e = Feed(decoder, 0x37);
    assert(e.key == KeyCode::PrintScreen && e.pressed);
    assert(!decoder.Feed(0xE0, e));
    e = Feed(decoder, 0xB7);
    assert(e.key == KeyCode::PrintScreen && !e.pressed);
    assert(!decoder.Feed(0xE0, e));
    assert(!decoder.Feed(0xAA, e));
    e = Feed(decoder, 0x00);
    assert(e.key == KeyCode::Unknown && e.pressed);

    ByteQueue queue;
    for (uint8_t i = 0; i < 63; ++i) assert(queue.Push(i));
    assert(!queue.Push(99));
    assert(queue.DroppedBytes() == 1);
    uint8_t value;
    for (uint8_t i = 0; i < 40; ++i) { assert(queue.Pop(value)); assert(value == i); }
    for (uint8_t i = 0; i < 40; ++i) assert(queue.Push(static_cast<uint8_t>(100 + i)));
    for (uint8_t i = 40; i < 63; ++i) { assert(queue.Pop(value)); assert(value == i); }
    for (uint8_t i = 0; i < 40; ++i) { assert(queue.Pop(value)); assert(value == static_cast<uint8_t>(100 + i)); }
    assert(!queue.Pop(value));
}
