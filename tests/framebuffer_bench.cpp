#include "Graphics/Framebuffer.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using Lumora::Graphics::Color;
using Lumora::Graphics::Framebuffer;
using Lumora::Graphics::FramebufferInfo;

namespace {
using Clock = std::chrono::steady_clock;

void ScalarFill(uint8_t* pixels, const FramebufferInfo& info, int64_t x,
                int64_t y, int64_t width, int64_t height, Color color) {
    if (width <= 0 || height <= 0) return;
    const int64_t x1 = x + width, y1 = y + height;
    const int64_t x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    const int64_t clipped_x1 = x1 > info.width ? info.width : x1;
    const int64_t clipped_y1 = y1 > info.height ? info.height : y1;
    const uint32_t packed = Framebuffer::PackColor(info, color);
    const uint8_t bpp = info.bits_per_pixel / 8;
    for (int64_t row = y0; row < clipped_y1; ++row)
        for (int64_t column = x0; column < clipped_x1; ++column) {
            const size_t offset = static_cast<size_t>(row) * info.pitch + column * bpp;
            for (uint8_t byte = 0; byte < bpp; ++byte)
                pixels[offset + byte] = static_cast<uint8_t>(packed >> (byte * 8));
        }
}

void Run(const char* name, Framebuffer& fb, int64_t x, int64_t y,
         int64_t width, int64_t height, unsigned iterations) {
    const auto start = Clock::now();
    for (unsigned i = 0; i < iterations; ++i)
        fb.FillRect(x, y, width, height, {42, 108, 196});
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    const double pixels = static_cast<double>(width) * height * iterations;
    const double bytes = pixels * 4.0;
    std::printf("%s: %.2f MPixel/s, %.2f MiB/s (%.3f s)\n", name,
                pixels / seconds / 1e6, bytes / seconds / (1024.0 * 1024.0), seconds);
}
}

int main() {
    constexpr uint32_t Width = 1024, Height = 768, Pitch = Width * 4;
    constexpr FramebufferInfo format{Width, Height, Pitch, 32, 16, 8, 8, 8, 0, 8};
    std::vector<uint8_t> storage(static_cast<size_t>(Pitch) * Height);
    Framebuffer fb;
    if (!fb.Initialize(storage.data(), storage.size(), format)) return 1;
    std::puts("Framebuffer host benchmark; compiler flags supplied by make host-bench");
    Run("pixel (one store)", fb, 24, 24, 1, 1, 2'000'000);
    Run("scanline 1024x1", fb, 0, 0, Width, 1, 100'000);
    Run("rectangle 64x32", fb, 40, 40, 64, 32, 10'000);
    Run("rectangle 256x128", fb, 20, 20, 256, 128, 500);
    const auto scalar_start = Clock::now();
    for (unsigned i = 0; i < 80; ++i)
        ScalarFill(storage.data(), format, 0, 0, Width, Height, {42, 108, 196});
    const double scalar_seconds =
        std::chrono::duration<double>(Clock::now() - scalar_start).count();
    const double full_pixels = static_cast<double>(Width) * Height * 80;
    std::printf("scalar reference full clear: %.2f MPixel/s, %.2f MiB/s (%.3f s)\n",
                full_pixels / scalar_seconds / 1e6,
                full_pixels * 4 / scalar_seconds / (1024.0 * 1024.0), scalar_seconds);
    Run("full clear 1024x768", fb, 0, 0, Width, Height, 80);
    // Approximate a terminal update: repaint a 26-pixel input row and 10 log
    // rows at 160 characters x 8 pixels each.
    const auto terminal_start = Clock::now();
    for (unsigned i = 0; i < 2000; ++i) {
        fb.FillRect(0, Height - 26, Width, 26, {24, 30, 42});
        fb.FillRect(0, 40, Width, 100, {12, 15, 23});
    }
    const double terminal_seconds =
        std::chrono::duration<double>(Clock::now() - terminal_start).count();
    const double terminal_pixels = static_cast<double>(Width) * 126 * 2000;
    std::printf("terminal-like dirty rows: %.2f MPixel/s, %.2f MiB/s (%.3f s)\n",
                terminal_pixels / terminal_seconds / 1e6,
                terminal_pixels * 4 / terminal_seconds / (1024.0 * 1024.0),
                terminal_seconds);
}
