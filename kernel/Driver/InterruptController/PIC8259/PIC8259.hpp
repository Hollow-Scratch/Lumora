#pragma once
#include <cstdint>

namespace Lumora::Driver::InterruptController::PIC8259 {

inline constexpr uint8_t MasterVectorBase = 0x20;
inline constexpr uint8_t SlaveVectorBase = 0x28;
inline constexpr uint8_t IRQCount = 16;

// Remaps both 8259s and leaves every IRQ masked.
void Initialize() noexcept;
bool IsInitialized() noexcept;
void SetIRQMask(uint8_t irq, bool masked) noexcept;
// Returns true when the IRQ was spurious. A spurious IRQ15 still needs a
// master EOI; a spurious IRQ7 needs none.
bool IsSpurious(uint8_t irq) noexcept;
void AcknowledgeSpurious(uint8_t irq) noexcept;
void EndOfInterrupt(uint8_t irq) noexcept;

} // namespace Lumora::Driver::InterruptController::PIC8259
