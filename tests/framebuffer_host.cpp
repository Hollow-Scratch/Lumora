#include "Graphics/Framebuffer.hpp"

#include <array>
#include <cassert>
#include <algorithm>
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

    // Differential coverage: compare clipped fast fills against an independent
    // scalar implementation over odd pitches, both supported pixel widths,
    // arbitrary channel order, and extreme signed coordinates.
    uint32_t seed = 0x12345678;
    auto next = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };
    for (const FramebufferInfo format : {
             FramebufferInfo{13, 9, 43, 24, 0, 8, 8, 8, 16, 8},
             FramebufferInfo{13, 9, 57, 32, 0, 8, 8, 8, 16, 8}}) {
        constexpr size_t Capacity = 700, Guard = 16;
        std::array<uint8_t, Capacity + 2 * Guard> actual_storage{}, expected_storage{};
        auto* actual = actual_storage.data() + Guard;
        auto* expected = expected_storage.data() + Guard;
        for (unsigned iteration = 0; iteration < 1000; ++iteration) {
            for (size_t i = 0; i < Capacity; ++i)
                actual[i] = expected[i] = static_cast<uint8_t>(next());
            for (size_t i = 0; i < Guard; ++i) {
                actual_storage[i] = expected_storage[i] = 0xD3;
                actual_storage[Guard + Capacity + i] =
                    expected_storage[Guard + Capacity + i] = 0x7E;
            }
            Framebuffer tested;
            assert(tested.Initialize(actual, Capacity, format));
            const int64_t x = iteration % 7 == 0 ? INT64_MIN :
                static_cast<int32_t>(next() % 35) - 11;
            const int64_t y = iteration % 11 == 0 ? INT64_MAX :
                static_cast<int32_t>(next() % 27) - 9;
            const int64_t w = iteration % 13 == 0 ? INT64_MAX : next() % 30;
            const int64_t h = iteration % 17 == 0 ? INT64_MAX : next() % 20;
            const Color color{static_cast<uint8_t>(next()), static_cast<uint8_t>(next()),
                              static_cast<uint8_t>(next())};
            tested.FillRect(x, y, w, h, color);
            size_t required = 0;
            assert(Framebuffer::Validate(format, Capacity, required));
            const auto packed = Framebuffer::PackColor(format, color);
            const uint8_t bpp = format.bits_per_pixel / 8;
            if (w > 0 && h > 0) {
                const int64_t ex = x > INT64_MAX - w ? INT64_MAX : x + w;
                const int64_t ey = y > INT64_MAX - h ? INT64_MAX : y + h;
                const int64_t x0 = std::max<int64_t>(x, 0);
                const int64_t y0 = std::max<int64_t>(y, 0);
                const int64_t x1 = std::min<int64_t>(ex, format.width);
                const int64_t y1 = std::min<int64_t>(ey, format.height);
                for (int64_t row = y0; row < y1; ++row)
                    for (int64_t col = x0; col < x1; ++col)
                        for (uint8_t byte = 0; byte < bpp; ++byte)
                            expected[static_cast<size_t>(row) * format.pitch + col * bpp + byte] =
                                static_cast<uint8_t>(packed >> (8 * byte));
            }
            assert(actual_storage == expected_storage);
        }
        Framebuffer tested;
        assert(tested.Initialize(actual, Capacity, format));
        tested.FillRect(0, 0, INT64_MAX, INT64_MAX, {3, 5, 7});
        assert(actual_storage[0] == 0xD3 && actual_storage[Guard - 1] == 0xD3);
        assert(actual_storage[Guard + Capacity] == 0x7E &&
               actual_storage.back() == 0x7E);
    }
    return 0;
}
