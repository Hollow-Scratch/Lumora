#pragma once
#include <cstddef>
#include <cstdint>

namespace Lumora::Arch::x86::GDT {

// These offsets match the bootstrap GDT in Boot.asm. Keep selectors here as
// the C++ authority (notably for IDT gates); no ring-3/TSS entries exist yet.
inline constexpr uint16_t NullSelector = 0x00;
inline constexpr uint16_t KernelCode32 = 0x08;
inline constexpr uint16_t KernelData   = 0x10;
inline constexpr uint16_t KernelCode64 = 0x18;
inline constexpr uint16_t KernelData64 = 0x20;
inline constexpr size_t EntryCount = 5;
inline constexpr size_t TableBytes = EntryCount * sizeof(uint64_t);
inline constexpr uint16_t TableLimit = TableBytes - 1;

static_assert(KernelCode32 == 1 * sizeof(uint64_t));
static_assert(KernelData == 2 * sizeof(uint64_t));
static_assert(KernelCode64 == 3 * sizeof(uint64_t));
static_assert(KernelData64 == 4 * sizeof(uint64_t));
static_assert(TableBytes == 40 && TableLimit == 39);

} // namespace Lumora::Arch::x86::GDT
