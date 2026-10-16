#include "Terminal.hpp"
#include "TerminalModel.hpp"
#include "../Core/Log.hpp"
#include "../Graphics/Framebuffer.hpp"
#include "../Version.hpp"

namespace Lumora::Console::Terminal {
namespace {
using Lumora::Core::Log::Entry;
using Lumora::Core::Log::Level;
using Lumora::Graphics::Color;
using KeyCode = Lumora::Driver::Input::PS2Keyboard::KeyCode;
constexpr size_t OutputLines = 128;
constexpr size_t OutputColumns = 160;
struct Line { char text[OutputColumns]{}; size_t length = 0; Level level = Level::Info; };
Line lines[OutputLines]{};
size_t first_line = 0, line_count = 0, scrollback = 0;
LineEditor editor;
constexpr Color Background{12, 15, 23}, Foreground{219, 228, 241};
constexpr Color PromptColor{105, 202, 150}, Muted{130, 145, 165};

Color LevelColor(Level level) noexcept {
    if (level == Level::Error) return {255, 120, 120};
    if (level == Level::Warning) return {255, 205, 105};
    return Foreground;
}
size_t Columns() noexcept {
    const auto width = Lumora::Graphics::Console().Width();
    const size_t cols = width > 40 ? (width - 40) / 6 : 1;
    return cols < OutputColumns ? cols : OutputColumns;
}
Line& CurrentLine() noexcept { return lines[(first_line + line_count - 1) % OutputLines]; }
void NewLine(Level level) noexcept {
    size_t index;
    if (line_count < OutputLines) {
        index = (first_line + line_count) % OutputLines;
        ++line_count;
    } else {
        first_line = (first_line + 1) % OutputLines;
        index = (first_line + line_count - 1) % OutputLines;
    }
    lines[index] = {};
    lines[index].level = level;
}
void Append(char c, Level level) noexcept {
    if (!line_count) NewLine(level);
    if (c == '\n') { NewLine(level); return; }
    Line& line = CurrentLine();
    if (line.length >= Columns() || line.length + 1 >= OutputColumns) {
        NewLine(level);
    }
    Line& destination = CurrentLine();
    destination.level = level;
    destination.text[destination.length++] = c;
    destination.text[destination.length] = '\0';
}
void WriteText(const char* text, Level level) noexcept {
    if (!text) return;
    if (!line_count) NewLine(level);
    for (size_t i = 0; i < 512 && text[i]; ++i) Append(text[i], level);
}
void WriteLine(const char* text, Level level = Level::Info) noexcept {
    WriteText(text, level); Append('\n', level);
}
void ClearOutput() noexcept { first_line = line_count = scrollback = 0; NewLine(Level::Info); }

void Render(bool redraw_output = false) noexcept {
    auto& fb = Lumora::Graphics::Console();
    if (!fb.IsReady()) return;
    if (redraw_output) {
        fb.FillRect(0, 0, fb.Width(), fb.Height(), Background);
        fb.DrawString(20, 10, "LUMORA KERNEL CONSOLE", PromptColor);
        fb.DrawString(20, 22, Lumora::KernelVersion, Muted);
        fb.DrawHorizontalLine(20, 36, static_cast<int64_t>(fb.Width()) - 40, {52, 64, 82});
    }
    const size_t rows = fb.Height() > 80 ? (fb.Height() - 78) / 10 : 0;
    const size_t max_back = line_count > rows ? line_count - rows : 0;
    const size_t back = scrollback > max_back ? max_back : scrollback;
    const size_t visible = line_count - back < rows ? line_count - back : rows;
    const size_t skip = line_count - back - visible;
    if (redraw_output) {
        const int64_t output_y = 40;
        const int64_t output_height = static_cast<int64_t>(fb.Height()) - 70;
        if (output_height > 0) fb.FillRect(0, output_y, fb.Width(), output_height, Background);
        for (size_t row = 0; row < visible; ++row) {
            const size_t logical = skip + row;
            const Line& line = lines[(first_line + logical) % OutputLines];
            fb.DrawString(20, 46 + static_cast<int64_t>(row) * 10,
                          line.text, LevelColor(line.level));
        }
    }
    const int64_t input_y = static_cast<int64_t>(fb.Height()) - 22;
    fb.FillRect(0, input_y - 4, fb.Width(), 26, {24, 30, 42});
    fb.DrawString(20, input_y, "lumora> ", PromptColor);
    const size_t cols = Columns();
    const size_t prompt_cols = 8;
    size_t offset = 0;
    if (editor.Cursor() + prompt_cols > cols) offset = editor.Cursor() + prompt_cols - cols;
    const char* input = editor.Text();
    size_t xcol = prompt_cols;
    for (size_t i = offset; i < editor.Length() && xcol < cols; ++i, ++xcol)
        fb.DrawCharacter(20 + static_cast<int64_t>(xcol) * 6, input_y, input[i], Foreground);
    const size_t cursor_col = prompt_cols + editor.Cursor() - offset;
    if (cursor_col < cols) {
        const int64_t cursor_x = 20 + static_cast<int64_t>(cursor_col) * 6;
        fb.FillRect(cursor_x, input_y - 1, 5, 8, PromptColor);
        if (editor.Cursor() < editor.Length())
            fb.DrawCharacter(cursor_x, input_y, input[editor.Cursor()], Background);
    }
}

void OutputCommand(Level level, const char* text, void*) noexcept {
    WriteLine(text, level);
}
#if defined(LUMORA_TEST_CONSOLE)
bool SameText(const char* left, const char* right) noexcept {
    size_t i = 0;
    while (left[i] && right[i] && left[i] == right[i]) ++i;
    return left[i] == right[i];
}
void TestSerialMarker(const char* text) noexcept {
    constexpr uint16_t Data = 0x3F8, Lsr = 0x3FD;
    for (size_t i = 0; text[i]; ++i) {
        for (uint32_t wait = 0; wait < 10000; ++wait) {
            uint8_t status;
            asm volatile("inb %1, %0" : "=a"(status) : "Nd"(Lsr));
            if (status & 0x20) {
                asm volatile("outb %0, %1" : : "a"(static_cast<uint8_t>(text[i])), "Nd"(Data));
                break;
            }
        }
    }
}
#endif
void OnLog(const Entry& entry) noexcept {
    const char* tag = entry.level == Level::Error ? "[ERROR] " :
        (entry.level == Level::Warning ? "[WARN] " : "[INFO] ");
    WriteText(tag, entry.level);
    WriteLine(entry.text, entry.level);
    // Retain an existing scrollback view while logs append; the prompt and edit
    // buffer are redrawn from their model and cannot be overwritten by output.
    Render(true);
}
}

void Initialize() noexcept {
    ClearOutput();
    Lumora::Core::Log::SetSink(OnLog, false);
    WriteLine("Boot complete. Type 'help' for commands.");
    WriteLine("");
    Render(true);
}

void Handle(const Lumora::Driver::Input::PS2Keyboard::KeyEvent& event) noexcept {
    if (!event.pressed) return;
    if (event.key == KeyCode::PageUp) {
        const size_t rows = Lumora::Graphics::Console().Height() > 80 ?
            (Lumora::Graphics::Console().Height() - 78) / 10 : 1;
        scrollback += rows ? rows : 1;
        Render(true); return;
    }
    if (event.key == KeyCode::PageDown) {
        const size_t rows = Lumora::Graphics::Console().Height() > 80 ?
            (Lumora::Graphics::Console().Height() - 78) / 10 : 1;
        scrollback = scrollback > rows ? scrollback - rows : 0;
        Render(true); return;
    }
    char submitted[LineEditor::LineCapacity]{};
    const auto result = editor.Handle(event, submitted, sizeof(submitted));
    if (result == LineEditor::Result::Submitted) {
        WriteText("lumora> ", Level::Info);
        WriteLine(submitted, Level::Info);
        const auto command_result = ExecuteBuiltin(submitted, OutputCommand, nullptr);
#if defined(LUMORA_TEST_CONSOLE)
        if (SameText(submitted, "help")) TestSerialMarker("CONSOLE_COMMAND_HELP_OK\r\n");
        if (SameText(submitted, "logs")) {
            const auto& history = Lumora::Core::Log::Entries();
            bool found_boot = false;
            for (size_t i = 0; i < history.Count(); ++i) {
                const Entry* entry = history.At(i);
                if (entry && entry->text[0] == 'L' && entry->text[1] == 'u') found_boot = true;
            }
            if (found_boot) TestSerialMarker("CONSOLE_BOOT_LOGS_RETAINED\r\n");
        }
#endif
        if (command_result == CommandResult::Clear) ClearOutput();
        scrollback = 0;
    }
    if (result != LineEditor::Result::None) {
        scrollback = 0;
        Render(result == LineEditor::Result::Submitted);
    }
}

} // namespace Lumora::Console::Terminal
