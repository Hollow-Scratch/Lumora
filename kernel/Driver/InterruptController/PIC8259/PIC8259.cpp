#include "PIC8259.hpp"

namespace Lumora::Driver::InterruptController::PIC8259 {
namespace {
constexpr uint16_t MasterCommand = 0x20;
constexpr uint16_t MasterData = 0x21;
constexpr uint16_t SlaveCommand = 0xA0;
constexpr uint16_t SlaveData = 0xA1;
constexpr uint8_t EOI = 0x20;
constexpr uint8_t ReadISR = 0x0B;

inline void Out8(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
inline uint8_t In8(uint16_t port) noexcept {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
inline void Wait() noexcept { Out8(0x80, 0); }
uint16_t masks = 0xFFFF;
bool initialized = false;
}

void Initialize() noexcept {
    // Mask first so no legacy IRQ can arrive during reprogramming.
    Out8(MasterData, 0xFF);
    Out8(SlaveData, 0xFF);
    Out8(MasterCommand, 0x11); Wait();
    Out8(SlaveCommand, 0x11); Wait();
    Out8(MasterData, MasterVectorBase); Wait();
    Out8(SlaveData, SlaveVectorBase); Wait();
    Out8(MasterData, 0x04); Wait();
    Out8(SlaveData, 0x02); Wait();
    Out8(MasterData, 0x01); Wait();
    Out8(SlaveData, 0x01); Wait();
    masks = 0xFFFF;
    Out8(MasterData, static_cast<uint8_t>(masks));
    Out8(SlaveData, static_cast<uint8_t>(masks >> 8));
    initialized = true;
}

bool IsInitialized() noexcept { return initialized; }

void SetIRQMask(uint8_t irq, bool masked) noexcept {
    if (irq >= IRQCount) return;
    const uint16_t bit = static_cast<uint16_t>(1u << irq);
    masks = masked ? static_cast<uint16_t>(masks | bit)
                   : static_cast<uint16_t>(masks & ~bit);
    // A slave IRQ requires the cascade line (IRQ2) enabled on the master.
    if (irq >= 8 && !masked) masks = static_cast<uint16_t>(masks & ~(1u << 2));
    if (irq == 2 && masked && (masks & 0xFF00u) != 0xFF00u) return;
    Out8(MasterData, static_cast<uint8_t>(masks));
    Out8(SlaveData, static_cast<uint8_t>(masks >> 8));
}

bool IsSpurious(uint8_t irq) noexcept {
    if (irq != 7 && irq != 15) return false;
    const uint16_t command = irq == 7 ? MasterCommand : SlaveCommand;
    Out8(command, ReadISR);
    return (In8(command) & 0x80) == 0;
}

void AcknowledgeSpurious(uint8_t irq) noexcept {
    // IRQ7 was never in service. For a spurious IRQ15 only the master saw the
    // cascade request, so acknowledge the master and leave the slave alone.
    if (irq == 15) Out8(MasterCommand, EOI);
}

void EndOfInterrupt(uint8_t irq) noexcept {
    if (irq >= IRQCount) return;
    if (irq >= 8) Out8(SlaveCommand, EOI);
    Out8(MasterCommand, EOI);
}

} // namespace Lumora::Driver::InterruptController::PIC8259
