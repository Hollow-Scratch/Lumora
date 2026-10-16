#pragma once
#include <cstdint>
#include "PS2Scancode.hpp"

namespace Lumora::Driver::Input::PS2Keyboard {

enum class InitResult : uint8_t {
    Ready, ControllerTimeout, NoController, InterfaceUnavailable,
    DeviceNoResponse, DeviceRejectedCommand, IRQRegistrationFailed
};

// Lock-free, bounded SPSC byte ring: IRQ1 is the sole producer and normal
// kernel execution is the sole consumer. On overflow the newest byte is
// dropped; DroppedBytes() reports the total.
class ByteQueue final {
public:
    bool Push(uint8_t byte) noexcept {
        const uint8_t head = __atomic_load_n(&head_, __ATOMIC_RELAXED);
        const uint8_t tail = __atomic_load_n(&tail_, __ATOMIC_ACQUIRE);
        if (static_cast<uint8_t>(head - tail) >= Capacity - 1) {
            __atomic_add_fetch(&dropped_, 1, __ATOMIC_RELAXED);
            return false;
        }
        bytes_[head % Capacity] = byte;
        __atomic_store_n(&head_, static_cast<uint8_t>(head + 1), __ATOMIC_RELEASE);
        return true;
    }
    bool Pop(uint8_t& byte) noexcept {
        const uint8_t tail = __atomic_load_n(&tail_, __ATOMIC_RELAXED);
        const uint8_t head = __atomic_load_n(&head_, __ATOMIC_ACQUIRE);
        if (head == tail) return false;
        byte = bytes_[tail % Capacity];
        __atomic_store_n(&tail_, static_cast<uint8_t>(tail + 1), __ATOMIC_RELEASE);
        return true;
    }
    uint32_t DroppedBytes() const noexcept { return __atomic_load_n(&dropped_, __ATOMIC_RELAXED); }
private:
    static constexpr uint8_t Capacity = 64;
    uint8_t bytes_[Capacity]{};
    uint8_t head_ = 0;
    uint8_t tail_ = 0;
    uint32_t dropped_ = 0;
};

InitResult Initialize() noexcept;
// Decodes buffered bytes until one event is available. Call only outside IRQ context.
bool ReadEvent(KeyEvent& event) noexcept;
uint32_t DroppedBytes() noexcept;

} // namespace Lumora::Driver::Input::PS2Keyboard
