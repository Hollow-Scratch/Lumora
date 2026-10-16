#include "BootScreen.hpp"
#include "../Core/Log.hpp"
#include "../Graphics/Framebuffer.hpp"
#include "../Version.hpp"

namespace Lumora::Console::BootScreen {
namespace {
using Lumora::Graphics::Color;
constexpr Color Background{17, 20, 30};
constexpr Color Foreground{224, 231, 242};
constexpr Color Accent{147, 197, 253};
constexpr Color Muted{138, 151, 170};

Color LevelColor(Lumora::Core::Log::Level level) noexcept {
    switch (level) {
    case Lumora::Core::Log::Level::Warning: return {255, 204, 102};
    case Lumora::Core::Log::Level::Error: return {255, 120, 120};
    default: return {151, 225, 175};
    }
}
void DrawLog(const Lumora::Core::Log::Entry& entry, uint32_t row) noexcept {
    auto& fb = Lumora::Graphics::Console();
    const int64_t y = 94 + static_cast<int64_t>(row) * 10;
    const char* tag = entry.level == Lumora::Core::Log::Level::Error ? "ERR" :
        (entry.level == Lumora::Core::Log::Level::Warning ? "WRN" : " OK");
    fb.DrawString(20, y, tag, LevelColor(entry.level));
    fb.DrawString(50, y, entry.text, Foreground);
}
void OnLog(const Lumora::Core::Log::Entry&) noexcept { Refresh(); }
}

void Initialize() noexcept {
    auto& fb = Lumora::Graphics::Console();
    fb.Clear(Background);
    fb.FillRect(0, 0, fb.Width(), 76, {25, 31, 45});
    fb.DrawString(20, 17, "LUMORA OS", Accent, fb.Width() >= 700 ? 2 : 1);
    fb.DrawString(20, 43, "KERNEL VERSION", Muted);
    fb.DrawString(128, 43, Lumora::KernelVersion, Foreground);
    fb.DrawHorizontalLine(20, 82, static_cast<int64_t>(fb.Width()) - 40, {59, 75, 99});
    Lumora::Core::Log::SetSink(OnLog, false);
    Refresh();
#if defined(LUMORA_TEST_CONSOLE)
    Lumora::Core::Log::Info("TEST_BOOT_LOG_SCREEN_RENDERED");
#endif
}

void Refresh() noexcept {
    auto& fb = Lumora::Graphics::Console();
    if (!fb.IsReady()) return;
    fb.Clear(Background);
    fb.FillRect(0, 0, fb.Width(), 76, {25, 31, 45});
    fb.DrawString(20, 17, "LUMORA OS", Accent, fb.Width() >= 700 ? 2 : 1);
    fb.DrawString(20, 43, "KERNEL VERSION", Muted);
    fb.DrawString(128, 43, Lumora::KernelVersion, Foreground);
    fb.DrawHorizontalLine(20, 82, static_cast<int64_t>(fb.Width()) - 40, {59, 75, 99});
    const uint32_t rows = fb.Height() > 104 ? (fb.Height() - 104) / 10 : 0;
    const auto& history = Lumora::Core::Log::Entries();
    const size_t first = history.FirstVisible(rows);
    for (size_t i = first; i < history.Count(); ++i) {
        const auto* entry = history.At(i);
        if (entry) DrawLog(*entry, static_cast<uint32_t>(i - first));
    }
}

} // namespace Lumora::Console::BootScreen
