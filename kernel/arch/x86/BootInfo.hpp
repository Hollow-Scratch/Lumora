#pragma once
#include <cstddef>
#include <cstdint>

namespace Lumora::Arch::x86 {

struct BootHandoff { uint32_t magic; uint32_t information_address; };
static_assert(sizeof(BootHandoff) == 8);

struct MemoryRegion { uint64_t address, length; uint32_t type; };
struct Framebuffer {
    uint64_t address;
    uint32_t pitch, width, height;
    uint8_t bits_per_pixel, type;
    bool has_rgb_fields;
    uint8_t red_position, red_mask_size;
    uint8_t green_position, green_mask_size;
    uint8_t blue_position, blue_mask_size;
};
struct BootInfo {
    MemoryRegion memory[128];
    size_t memory_count;
    Framebuffer framebuffer;
    bool has_framebuffer;
    char bootloader[128];
    char command_line[256];
    bool has_bootloader, has_command_line;
};

// The pointer is physical. Caller must first map the containing boot-info
// range. False means malformed, unsupported/truncated, or wrong MB2 magic.
bool ParseBootInfo(const BootHandoff& handoff, BootInfo& result);

} // namespace Lumora::Arch::x86
