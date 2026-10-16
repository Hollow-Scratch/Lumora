#pragma once
#include "../Driver/Input/PS2Keyboard/PS2Scancode.hpp"
namespace Lumora::Console::Terminal {
void Initialize() noexcept;
void Handle(const Lumora::Driver::Input::PS2Keyboard::KeyEvent& event) noexcept;
}
