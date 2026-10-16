#include "Framebuffer.hpp"

#include "Font.hpp"

namespace Lumora::Graphics {
namespace {
Framebuffer g_console;

bool ChannelMask(uint8_t position, uint8_t size, uint8_t bpp,
                 uint64_t& mask) noexcept {
    if (size == 0 || size > 16 || position >= bpp ||
        static_cast<unsigned>(position) + size > bpp) return false;
    mask = ((uint64_t{1} << size) - 1) << position;
    return true;
}

int64_t SaturatingAdd(int64_t a, int64_t positive) noexcept {
    return a > INT64_MAX - positive ? INT64_MAX : a + positive;
}
}

Framebuffer& Console() noexcept { return g_console; }

bool Framebuffer::Validate(const FramebufferInfo& f, size_t mapped_bytes,
                           size_t& required_bytes) noexcept {
    required_bytes = 0;
    if (!f.width || !f.height || !f.pitch ||
        (f.bits_per_pixel != 24 && f.bits_per_pixel != 32)) return false;
    const uint8_t bytes = static_cast<uint8_t>(f.bits_per_pixel / 8);
    const uint64_t minimum_row = static_cast<uint64_t>(f.width) * bytes;
    if (minimum_row > f.pitch) return false;

    uint64_t red, green, blue;
    if (!ChannelMask(f.red_position, f.red_size, f.bits_per_pixel, red) ||
        !ChannelMask(f.green_position, f.green_size, f.bits_per_pixel, green) ||
        !ChannelMask(f.blue_position, f.blue_size, f.bits_per_pixel, blue) ||
        (red & green) || (red & blue) || (green & blue)) return false;

    const uint64_t length = static_cast<uint64_t>(f.pitch) * f.height;
    if (length > SIZE_MAX || length > mapped_bytes) return false;
    required_bytes = static_cast<size_t>(length);
    return true;
}

bool Framebuffer::Initialize(volatile uint8_t* address, size_t mapped_bytes,
                             const FramebufferInfo& info) noexcept {
    size_t required;
    if (!address || !Validate(info, mapped_bytes, required)) {
        pixels_ = nullptr;
        mapped_bytes_ = 0;
        info_ = {};
        bytes_per_pixel_ = 0;
        return false;
    }
    pixels_ = address;
    mapped_bytes_ = required;
    info_ = info;
    bytes_per_pixel_ = static_cast<uint8_t>(info.bits_per_pixel / 8);
    return true;
}

uint32_t Framebuffer::PackColor(const FramebufferInfo& f, Color color) noexcept {
    uint64_t ignored;
    if ((f.bits_per_pixel != 24 && f.bits_per_pixel != 32) ||
        !ChannelMask(f.red_position, f.red_size, f.bits_per_pixel, ignored) ||
        !ChannelMask(f.green_position, f.green_size, f.bits_per_pixel, ignored) ||
        !ChannelMask(f.blue_position, f.blue_size, f.bits_per_pixel, ignored)) return 0;
    const auto scale = [](uint8_t component, uint8_t size) -> uint64_t {
        const uint64_t max = (uint64_t{1} << size) - 1;
        return (static_cast<uint64_t>(component) * max + 127) / 255;
    };
    return static_cast<uint32_t>(
        (scale(color.red, f.red_size) << f.red_position) |
        (scale(color.green, f.green_size) << f.green_position) |
        (scale(color.blue, f.blue_size) << f.blue_position));
}

void Framebuffer::StorePixel(uint32_t x, uint32_t y, uint32_t packed) noexcept {
    const uint64_t offset = static_cast<uint64_t>(y) * info_.pitch +
        static_cast<uint64_t>(x) * bytes_per_pixel_;
    if (offset + bytes_per_pixel_ > mapped_bytes_) return;
    for (uint8_t byte = 0; byte < bytes_per_pixel_; ++byte)
        pixels_[offset + byte] = static_cast<uint8_t>(packed >> (byte * 8));
}

void Framebuffer::Clear(Color color) noexcept {
    if (!IsReady()) return;
    FillRect(0, 0, info_.width, info_.height, color);
}

void Framebuffer::DrawPixel(int64_t x, int64_t y, Color color) noexcept {
    if (!IsReady() || x < 0 || y < 0 ||
        static_cast<uint64_t>(x) >= info_.width ||
        static_cast<uint64_t>(y) >= info_.height) return;
    StorePixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), PackColor(info_, color));
}

void Framebuffer::FillRect(int64_t x, int64_t y, int64_t width, int64_t height,
                           Color color) noexcept {
    if (!IsReady() || width <= 0 || height <= 0) return;
    const int64_t end_x = SaturatingAdd(x, width);
    const int64_t end_y = SaturatingAdd(y, height);
    const int64_t x0 = x < 0 ? 0 : x;
    const int64_t y0 = y < 0 ? 0 : y;
    const int64_t x1 = end_x > info_.width ? info_.width : end_x;
    const int64_t y1 = end_y > info_.height ? info_.height : end_y;
    if (x0 >= x1 || y0 >= y1) return;
    const uint32_t packed = PackColor(info_, color);
    const size_t pixel_bytes = bytes_per_pixel_;
    const size_t span_bytes = static_cast<size_t>(x1 - x0) * pixel_bytes;
    const size_t first_column = static_cast<size_t>(x0) * pixel_bytes;
    uint8_t pixel[4]{};
    for (size_t byte = 0; byte < pixel_bytes; ++byte)
        pixel[byte] = static_cast<uint8_t>(packed >> (byte * 8));

    for (int64_t row = y0; row < y1; ++row) {
        const size_t offset = static_cast<size_t>(row) * info_.pitch + first_column;
        if (offset > mapped_bytes_ || span_bytes > mapped_bytes_ - offset) return;
        volatile uint8_t* destination = pixels_ + offset;
        size_t remaining = span_bytes;
        if (pixel_bytes == 4 &&
            (reinterpret_cast<uintptr_t>(destination) & (alignof(uint32_t) - 1)) == 0) {
            const uint32_t value = packed;
            auto* words = reinterpret_cast<volatile uint32_t*>(destination);
            while (remaining >= sizeof(value)) {
                *words++ = value;
                remaining -= sizeof(value);
            }
            destination = reinterpret_cast<volatile uint8_t*>(words);
        }
        size_t phase = (span_bytes - remaining) % pixel_bytes;
        while (remaining--) {
            *destination++ = pixel[phase++];
            if (phase == pixel_bytes) phase = 0;
        }
    }
}

void Framebuffer::DrawHorizontalLine(int64_t x, int64_t y, int64_t length,
                                     Color color) noexcept {
    FillRect(x, y, length, 1, color);
}

void Framebuffer::DrawVerticalLine(int64_t x, int64_t y, int64_t length,
                                   Color color) noexcept {
    FillRect(x, y, 1, length, color);
}

void Framebuffer::DrawCharacter(int64_t x, int64_t y, char c, Color color,
                                uint8_t scale) noexcept {
    if (!IsReady() || scale == 0) return;
    uint8_t rows[7];
    Font::GlyphRows(c, rows);
    for (uint8_t row = 0; row < 7; ++row) {
        uint8_t column = 0;
        while (column < 5) {
            while (column < 5 && !(rows[row] & (1u << (4 - column)))) ++column;
            const uint8_t first = column;
            while (column < 5 && (rows[row] & (1u << (4 - column)))) ++column;
            if (column > first) {
                FillRect(SaturatingAdd(x, static_cast<int64_t>(first) * scale),
                         SaturatingAdd(y, static_cast<int64_t>(row) * scale),
                         static_cast<int64_t>(column - first) * scale,
                         scale, color);
            }
        }
    }
}

void Framebuffer::DrawString(int64_t x, int64_t y, const char* text, Color color,
                             uint8_t scale) noexcept {
    if (!IsReady() || !text || scale == 0) return;
    const int64_t start_x = x;
    for (size_t i = 0; i < 512 && text[i]; ++i) {
        if (text[i] == '\n') { x = start_x; y = SaturatingAdd(y, 8 * scale); continue; }
        DrawCharacter(x, y, text[i], color, scale);
        x = SaturatingAdd(x, 6 * scale);
    }
}

void Framebuffer::DrawHex(int64_t x, int64_t y, uint64_t value, uint8_t digits,
                          Color color, uint8_t scale) noexcept {
    static constexpr char hex[] = "0123456789ABCDEF";
    if (digits > 16) digits = 16;
    char output[17];
    for (uint8_t i = 0; i < digits; ++i) {
        const uint8_t shift = static_cast<uint8_t>((digits - i - 1) * 4);
        output[i] = hex[(value >> shift) & 0xF];
    }
    output[digits] = 0;
    DrawString(x, y, output, color, scale);
}

void Framebuffer::DrawDecimal(int64_t x, int64_t y, uint64_t value,
                              Color color, uint8_t scale) noexcept {
    char output[21];
    size_t length = 0;
    do { output[length++] = static_cast<char>('0' + value % 10); value /= 10; }
    while (value && length < sizeof(output) - 1);
    for (size_t i = 0; i < length / 2; ++i) {
        const char t = output[i]; output[i] = output[length - i - 1]; output[length - i - 1] = t;
    }
    output[length] = 0;
    DrawString(x, y, output, color, scale);
}

} // namespace Lumora::Graphics
