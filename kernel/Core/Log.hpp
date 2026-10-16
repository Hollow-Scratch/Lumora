#pragma once
#include <cstddef>
#include <cstdint>

namespace Lumora::Core::Log {

enum class Level : uint8_t { Info, Warning, Error };
struct Entry { Level level; uint32_t sequence; char text[112]; };
using Sink = void (*)(const Entry&) noexcept;

class History final {
public:
    static constexpr size_t Capacity = 64;
    static constexpr size_t TextCapacity = sizeof(Entry::text);
    void Push(Level level, const char* text) noexcept;
    size_t Count() const noexcept { return count_; }
    const Entry* At(size_t oldest_index) const noexcept;
    size_t FirstVisible(size_t visible_rows, size_t scrollback_rows = 0) const noexcept;
private:
    Entry entries_[Capacity]{};
    size_t next_ = 0;
    size_t count_ = 0;
    uint32_t sequence_ = 0;
};

void Write(Level level, const char* message) noexcept;
void Info(const char* message) noexcept;
void Warning(const char* message) noexcept;
void Error(const char* message) noexcept;
void SetSink(Sink sink, bool replay_history = true) noexcept;
const History& Entries() noexcept;

} // namespace Lumora::Core::Log
