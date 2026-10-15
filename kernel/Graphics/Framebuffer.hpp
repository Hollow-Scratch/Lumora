#pragma once

#include <cstddef>
#include <cstdint>

namespace Lumora::Graphics {

struct Color { uint8_t red, green, blue; };

struct FramebufferInfo {
    uint32_t width, height, pitch;
    uint8_t bits_per_pixel;
    uint8_t red_position, red_size;
    uint8_t green_position, green_size;
    uint8_t blue_position, blue_size;
};

class Framebuffer final {
public:
    [[nodiscard]] bool Initialize(volatile uint8_t* mapped_address,
                                  size_t mapped_bytes,
                                  const FramebufferInfo& info) noexcept;
    [[nodiscard]] bool IsReady() const noexcept { return pixels_ != nullptr; }
    [[nodiscard]] uint32_t Width() const noexcept { return info_.width; }
    [[nodiscard]] uint32_t Height() const noexcept { return info_.height; }

    void Clear(Color color) noexcept;
    void DrawPixel(int64_t x, int64_t y, Color color) noexcept;
    void FillRect(int64_t x, int64_t y, int64_t width, int64_t height,
                  Color color) noexcept;
    void DrawHorizontalLine(int64_t x, int64_t y, int64_t length,
                            Color color) noexcept;
    void DrawVerticalLine(int64_t x, int64_t y, int64_t length,
                          Color color) noexcept;
    void DrawCharacter(int64_t x, int64_t y, char character, Color color,
                       uint8_t scale = 1) noexcept;
    void DrawString(int64_t x, int64_t y, const char* text, Color color,
                    uint8_t scale = 1) noexcept;
    void DrawHex(int64_t x, int64_t y, uint64_t value, uint8_t digits,
                 Color color, uint8_t scale = 1) noexcept;
    void DrawDecimal(int64_t x, int64_t y, uint64_t value, Color color,
                     uint8_t scale = 1) noexcept;

    [[nodiscard]] static bool Validate(const FramebufferInfo& info,
                                       size_t mapped_bytes,
                                       size_t& required_bytes) noexcept;
    [[nodiscard]] static uint32_t PackColor(const FramebufferInfo& info,
                                            Color color) noexcept;

private:
    void StorePixel(uint32_t x, uint32_t y, uint32_t packed) noexcept;
    volatile uint8_t* pixels_ = nullptr;
    size_t mapped_bytes_ = 0;
    FramebufferInfo info_{};
    uint8_t bytes_per_pixel_ = 0;
};

// Trivial global storage: safe before heap/runtime initialization.
[[nodiscard]] Framebuffer& Console() noexcept;

} // namespace Lumora::Graphics
