#include "PS2Keyboard.hpp"
#include "../../InterruptController/PIC8259/PIC8259.hpp"
#include "../../../arch/x86/Interrupts/IDT.hpp"

namespace Lumora::Driver::Input::PS2Keyboard {
namespace {
constexpr uint16_t DataPort = 0x60, StatusCommandPort = 0x64;
constexpr uint8_t StatusOutputFull = 1u << 0, StatusInputFull = 1u << 1;
constexpr uint8_t StatusAuxiliary = 1u << 5, StatusTimeout = 1u << 6, StatusParity = 1u << 7;
constexpr uint32_t PollLimit = 100000;
constexpr uint8_t CmdReadConfig = 0x20, CmdWriteConfig = 0x60;
constexpr uint8_t CmdDisableKeyboard = 0xAD, CmdEnableKeyboard = 0xAE;
constexpr uint8_t CmdSelfTest = 0xAA, CmdInterfaceTest = 0xAB;
constexpr uint8_t ConfigIRQ1 = 1u << 0, ConfigTranslation = 1u << 6, ConfigClockDisabled = 1u << 4;
constexpr uint8_t DeviceDisableScanning = 0xF5, DeviceSetScancodeSet = 0xF0;
constexpr uint8_t DeviceEnableScanning = 0xF4, DeviceAck = 0xFA, DeviceResend = 0xFE;
ByteQueue raw_bytes;
ScancodeDecoder decoder;
bool ready = false;

inline uint8_t In8(uint16_t port) noexcept {
    uint8_t value; asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port)); return value;
}
inline void Out8(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
bool WaitInputEmpty() noexcept {
    for (uint32_t i = 0; i < PollLimit; ++i) {
        if ((In8(StatusCommandPort) & StatusInputFull) == 0) return true;
        asm volatile("pause");
    }
    return false;
}
bool WaitOutputFull(uint8_t& status) noexcept {
    for (uint32_t i = 0; i < PollLimit; ++i) {
        status = In8(StatusCommandPort);
        if (status & StatusOutputFull) return true;
        asm volatile("pause");
    }
    return false;
}
bool ReadControllerByte(uint8_t& value) noexcept {
    uint8_t status;
    if (!WaitOutputFull(status)) return false;
    value = In8(DataPort);
    return (status & (StatusAuxiliary | StatusTimeout | StatusParity)) == 0;
}
void DrainOutput() noexcept {
    for (uint32_t i = 0; i < 32; ++i) {
        const uint8_t status = In8(StatusCommandPort);
        if ((status & StatusOutputFull) == 0) break;
        (void)In8(DataPort);
    }
}
bool ControllerCommand(uint8_t command) noexcept {
    if (!WaitInputEmpty()) return false;
    Out8(StatusCommandPort, command);
    return true;
}
bool ControllerData(uint8_t value) noexcept {
    if (!WaitInputEmpty()) return false;
    Out8(DataPort, value);
    return true;
}
bool DeviceCommand(uint8_t command) noexcept {
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (!ControllerData(command)) return false;
        uint8_t status, response;
        if (!WaitOutputFull(status)) return false;
        response = In8(DataPort);
        if (status & (StatusAuxiliary | StatusTimeout | StatusParity)) return false;
        if (response == DeviceAck) return true;
        if (response != DeviceResend) return false;
    }
    return false;
}
bool DeviceData(uint8_t value) noexcept {
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (!ControllerData(value)) return false;
        uint8_t status;
        if (!WaitOutputFull(status)) return false;
        const uint8_t response = In8(DataPort);
        if (status & (StatusAuxiliary | StatusTimeout | StatusParity)) return false;
        if (response == DeviceAck) return true;
        if (response != DeviceResend) return false;
    }
    return false;
}
void KeyboardIRQ(uint8_t, Lumora::Arch::x86::InterruptFrame*) {
    const uint8_t status = In8(StatusCommandPort);
    if ((status & StatusOutputFull) == 0 || (status & StatusAuxiliary) != 0) return;
    const uint8_t byte = In8(DataPort);
    if ((status & (StatusTimeout | StatusParity)) == 0)
        (void)raw_bytes.Push(byte);
}
}

InitResult Initialize() noexcept {
    if (!ControllerCommand(CmdDisableKeyboard)) return InitResult::ControllerTimeout;
    DrainOutput();
    if (!ControllerCommand(CmdSelfTest)) return InitResult::ControllerTimeout;
    uint8_t response;
    if (!ReadControllerByte(response)) return InitResult::NoController;
    if (response != 0x55) return InitResult::NoController;
    if (!ControllerCommand(CmdInterfaceTest)) return InitResult::ControllerTimeout;
    if (!ReadControllerByte(response)) return InitResult::InterfaceUnavailable;
    if (response != 0x00) return InitResult::InterfaceUnavailable;

    // Enable keyboard clock; preserve unrelated controller configuration.
    if (!ControllerCommand(CmdReadConfig) || !ReadControllerByte(response)) return InitResult::ControllerTimeout;
    uint8_t config = static_cast<uint8_t>((response | ConfigTranslation) & ~(ConfigIRQ1 | ConfigClockDisabled));
    if (!ControllerCommand(CmdWriteConfig) || !ControllerData(config)) return InitResult::ControllerTimeout;
    if (!ControllerCommand(CmdEnableKeyboard)) return InitResult::ControllerTimeout;

    if (!DeviceCommand(DeviceDisableScanning)) return InitResult::DeviceNoResponse;
    if (!DeviceCommand(DeviceSetScancodeSet)) return InitResult::DeviceRejectedCommand;
    if (!DeviceData(0x02)) return InitResult::DeviceRejectedCommand;
    // Translation is enabled, so set 2 reaches the kernel as set 1.
    // All handler state is ready before registration unsets the PIC mask.
    ready = true;
    if (!Lumora::Arch::x86::RegisterIRQHandler(1, KeyboardIRQ)) {
        ready = false;
        return InitResult::IRQRegistrationFailed;
    }

    config |= ConfigIRQ1;
    if (!ControllerCommand(CmdWriteConfig) || !ControllerData(config) ||
        !DeviceCommand(DeviceEnableScanning)) {
        Lumora::Driver::InterruptController::PIC8259::SetIRQMask(1, true);
        ready = false;
        return InitResult::DeviceRejectedCommand;
    }
    return InitResult::Ready;
}

bool ReadEvent(KeyEvent& event) noexcept {
    if (!ready) return false;
    uint8_t byte;
    while (raw_bytes.Pop(byte)) if (decoder.Feed(byte, event)) return true;
    return false;
}
uint32_t DroppedBytes() noexcept { return raw_bytes.DroppedBytes(); }

} // namespace Lumora::Driver::Input::PS2Keyboard
