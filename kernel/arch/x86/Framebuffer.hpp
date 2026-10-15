#pragma once
#include <cstddef>
#include <cstdint>
#include "BootInfo.hpp"

namespace Lumora::Arch::x86 {

struct FramebufferMapping {
    volatile uint8_t* address;
    size_t length;
};

// Maps the complete validated pitch*height range. The high-address window is
// fixed-capacity, uncached, and intended for one early-boot framebuffer.
[[nodiscard]] bool MapFramebuffer(const Framebuffer& framebuffer,
                                  FramebufferMapping& mapping) noexcept;

} // namespace Lumora::Arch::x86
