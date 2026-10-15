#include "Graphics/Framebuffer.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using Lumora::Graphics::Color;
using Lumora::Graphics::Framebuffer;
using Lumora::Graphics::FramebufferInfo;

int main() {
    constexpr FramebufferInfo rgba{3, 2, 16, 32, 16, 8, 8, 8, 0, 8};
    std::array<uint8_t, 32> bytes{};
    bytes.fill(0xA5);
    size_t required = 0;
    assert(Framebuffer::Validate(rgba, bytes.size(), required));
    assert(required == 32); // pitch (16), not width*bpp (12), sizes each row

    Framebuffer fb;
    assert(fb.Initialize(bytes.data(), bytes.size(), rgba));
    fb.DrawPixel(0, 0, {255, 0, 0});
    assert(bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 255 && bytes[3] == 0);
    assert(bytes[12] == 0xA5 && bytes[15] == 0xA5); // row padding untouched

    fb.FillRect(-1, -1, 3, 3, {0, 255, 0}); // clipped to [0,2)x[0,2)
    assert(bytes[0] == 0 && bytes[1] == 255 && bytes[2] == 0);
    assert(bytes[16] == 0 && bytes[17] == 255 && bytes[18] == 0);
    assert(bytes[12] == 0xA5 && bytes[28] == 0xA5);
    fb.DrawPixel(3, 0, {255, 0, 0});
    fb.FillRect(INT64_MAX - 1, 0, INT64_MAX, 2, {0, 0, 255});

    FramebufferInfo bad_pitch = rgba;
    bad_pitch.pitch = 11;
    assert(!Framebuffer::Validate(bad_pitch, bytes.size(), required));
    FramebufferInfo overlapping = rgba;
    overlapping.green_position = 16;
    assert(!Framebuffer::Validate(overlapping, bytes.size(), required));
    FramebufferInfo truncated = rgba;
    assert(!Framebuffer::Validate(truncated, 31, required));
    FramebufferInfo large_surface = rgba;
    large_surface.height = UINT32_MAX;
    assert(Framebuffer::Validate(large_surface, SIZE_MAX, required));
    assert(required == static_cast<size_t>(large_surface.pitch) * large_surface.height);

    constexpr FramebufferInfo rgb24{1, 1, 5, 24, 16, 8, 8, 8, 0, 8};
    std::array<uint8_t, 5> bytes24{};
    assert(fb.Initialize(bytes24.data(), bytes24.size(), rgb24));
    fb.Clear({0x12, 0x34, 0x56});
    assert(bytes24[0] == 0x56 && bytes24[1] == 0x34 && bytes24[2] == 0x12);
    assert(bytes24[3] == 0 && bytes24[4] == 0); // pitch padding retained
    return 0;
}
