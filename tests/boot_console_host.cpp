#include <cassert>
#include <cstddef>
#include "../kernel/Core/Log.hpp"
#include "../kernel/Console/TerminalModel.hpp"

using namespace Lumora::Core::Log;
using namespace Lumora::Console;
using Lumora::Driver::Input::PS2Keyboard::KeyCode;
using Lumora::Driver::Input::PS2Keyboard::KeyEvent;

static KeyEvent Key(KeyCode code, char character = 0) {
    KeyEvent event{}; event.key = code; event.character = character; event.pressed = true; return event;
}

struct Capture { char lines[16][128]{}; size_t count = 0; };
static void CaptureOutput(Level, const char* text, void* context) noexcept {
    auto& capture = *static_cast<Capture*>(context);
    if (capture.count >= 16) return;
    size_t i = 0;
    while (i + 1 < sizeof(capture.lines[0]) && text[i]) { capture.lines[capture.count][i] = text[i]; ++i; }
    capture.lines[capture.count][i] = '\0'; ++capture.count;
}

int main() {
    History history;
    for (uint32_t i = 1; i <= 70; ++i) {
        char text[8] = {'L', static_cast<char>('0' + (i / 10) % 10), static_cast<char>('0' + i % 10), 0};
        history.Push(Level::Info, text);
    }
    assert(history.Count() == History::Capacity);
    assert(history.At(0)->sequence == 7 && history.At(63)->sequence == 70);
    assert(history.FirstVisible(3) == 61);
    assert(history.FirstVisible(3, 2) == 59);
    char long_message[140];
    for (size_t i = 0; i + 1 < sizeof(long_message); ++i) long_message[i] = 'x';
    long_message[sizeof(long_message) - 1] = '\0';
    history.Push(Level::Warning, long_message);
    const Entry* truncated = history.At(history.Count() - 1);
    assert(truncated->level == Level::Warning);
    assert(truncated->text[History::TextCapacity - 4] == '.' &&
           truncated->text[History::TextCapacity - 2] == '.' &&
           truncated->text[History::TextCapacity - 1] == '\0');

    LineEditor editor;
    char submitted[LineEditor::LineCapacity]{};
    editor.Handle(Key(KeyCode::Unknown, 'h'), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Unknown, 'e'), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Unknown, 'l'), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Unknown, 'p'), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Left), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Unknown, 'X'), submitted, sizeof(submitted));
    assert(editor.Length() == 5 && editor.Cursor() == 4);
    editor.Handle(Key(KeyCode::Backspace), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Home), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Delete), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::End), submitted, sizeof(submitted));
    assert(editor.Handle(Key(KeyCode::Enter), submitted, sizeof(submitted)) == LineEditor::Result::Submitted);
    assert(submitted[0] == 'e' && submitted[1] == 'l' && submitted[2] == 'p' && submitted[3] == 0);
    editor.Handle(Key(KeyCode::Up), submitted, sizeof(submitted));
    assert(editor.Length() == 3 && editor.Text()[0] == 'e');
    editor.Handle(Key(KeyCode::Down), submitted, sizeof(submitted));
    assert(editor.Length() == 0);
    for (char c : "about") if (c) editor.Handle(Key(KeyCode::Unknown, c), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Enter), submitted, sizeof(submitted));
    editor.Handle(Key(KeyCode::Up), submitted, sizeof(submitted));
    assert(editor.Text()[0] == 'a' && editor.Length() == 5);
    editor.Handle(Key(KeyCode::Up), submitted, sizeof(submitted));
    assert(editor.Text()[0] == 'e' && editor.Length() == 3);
    KeyEvent keypad_left = Key(KeyCode::Keypad4);
    editor.Handle(keypad_left, submitted, sizeof(submitted));
    assert(editor.Cursor() == 2);
    for (size_t i = 0; i < 100; ++i) editor.Handle(Key(KeyCode::Unknown, 'x'), submitted, sizeof(submitted));
    assert(editor.Length() <= LineEditor::LineCapacity - 1);

    Capture capture;
    assert(ExecuteBuiltin("help", CaptureOutput, &capture) == CommandResult::Continue);
    assert(capture.count == 1 && capture.lines[0][0] == 'C');
    assert(ExecuteBuiltin("about", CaptureOutput, &capture) == CommandResult::Continue);
    assert(capture.count == 4 && capture.lines[3][0] == '0');
    assert(ExecuteBuiltin("echo hello world", CaptureOutput, &capture) == CommandResult::Continue);
    assert(capture.lines[4][0] == 'h');
    assert(ExecuteBuiltin("ech", CaptureOutput, &capture) == CommandResult::Continue);
    assert(capture.lines[5][0] == 'U');
    assert(ExecuteBuiltin("clear", CaptureOutput, &capture) == CommandResult::Clear);

    Info("retained boot entry");
    capture.count = 0;
    ExecuteBuiltin("logs", CaptureOutput, &capture);
    assert(capture.count == 1 && capture.lines[0][0] == 'r');
}
