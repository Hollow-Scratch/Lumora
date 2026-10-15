#pragma once
#include <cstdint>

namespace Lumora::Core {

enum class PanicReason : uint32_t {
    Unknown,
    InvalidBootInfo,
    FramebufferUnavailable,
    InvalidMemoryState,
    KernelInvariantViolation,
    CPUException
};

struct PanicContext {
    bool has_exception = false;
    bool has_error_code = false;
    uint64_t vector = 0;
    uint64_t error_code = 0;
    uint64_t rip = 0;
    const char* exception_name = nullptr;
    const uint64_t* registers = nullptr; // r15 .. rax, matching InterruptFrame
};

[[noreturn]] void Panic(PanicReason reason, const char* message,
                        const char* file, uint32_t line,
                        const PanicContext* context = nullptr) noexcept;

} // namespace Lumora::Core

#define LUMORA_PANIC(reason, message) \
    ::Lumora::Core::Panic((reason), (message), __FILE__, __LINE__)
