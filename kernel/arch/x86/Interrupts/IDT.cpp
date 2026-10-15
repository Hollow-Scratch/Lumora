#include "IDT.hpp"
#include "../GDT.hpp"
#include "../../../Core/Panic.hpp"

namespace Lumora::Arch::x86 {

alignas(16) static IDTEntry idt[256]{};
static IDTPointer idt_pointer{};

extern "C" {
    extern void* isr_stub_table[32];
    extern void isr_default();
    [[noreturn]] void interrupt_dispatch(InterruptFrame* frame);
}

static void SetGate(uint8_t vector, void (*handler)()) {
    const uint64_t address = reinterpret_cast<uint64_t>(handler);
    IDTEntry& entry = idt[vector];
    entry.offset_1 = static_cast<uint16_t>(address);
    entry.selector = GDT::KernelCode64;
    entry.ist = 0;
    entry.type_attributes = 0x8E; // Present, DPL 0, 64-bit interrupt gate.
    entry.offset_2 = static_cast<uint16_t>(address >> 16);
    entry.offset_3 = static_cast<uint32_t>(address >> 32);
    entry.zero = 0;
}

void InitIDT() {
    for (uint16_t i = 0; i < 256; ++i)
        SetGate(static_cast<uint8_t>(i), isr_default);
    for (uint8_t i = 0; i < 32; ++i)
        SetGate(i, reinterpret_cast<void (*)()>(isr_stub_table[i]));

    idt_pointer.limit = static_cast<uint16_t>(sizeof(idt) - 1);
    idt_pointer.base = reinterpret_cast<uint64_t>(idt);

    asm volatile("lidt %0" : : "m"(idt_pointer) : "memory");

    IDTPointer actual{};
    asm volatile("sidt %0" : "=m"(actual) : : "memory");
    if (actual.base != idt_pointer.base || actual.limit != idt_pointer.limit) {
        Lumora::Core::Panic(Lumora::Core::PanicReason::KernelInvariantViolation,
                    "IDTR readback did not match the installed table",
                    __FILE__, __LINE__);
    }
}

static const char* ExceptionName(uint64_t vector) noexcept {
    static const char* const names[32] = {
        "Divide Error", "Debug", "NMI", "Breakpoint", "Overflow",
        "BOUND Range", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS",
        "Segment Not Present", "Stack Fault", "General Protection",
        "Page Fault", "Reserved", "x87 Floating Point", "Alignment Check",
        "Machine Check", "SIMD Floating Point", "Virtualization",
        "Control Protection", "Reserved", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Hypervisor Injection",
        "VMM Communication", "Security Exception", "Reserved"
    };
    return vector < 32 ? names[vector] : "Unknown interrupt";
}

extern "C" [[noreturn]] void interrupt_dispatch(InterruptFrame* frame) {
    const uint64_t vector = frame->vector;
    const bool has_error = vector == 8 || (vector >= 10 && vector <= 14) ||
        vector == 17 || vector == 21 || vector == 29 || vector == 30;
    const Lumora::Core::PanicContext context{
        true, has_error, vector, frame->error_code, frame->rip,
        ExceptionName(vector), &frame->r15
    };
    Lumora::Core::Panic(Lumora::Core::PanicReason::CPUException,
                vector < 32 ? "Fatal processor exception" : "Unexpected interrupt; IRQ handling is not initialized",
                __FILE__, __LINE__, &context);
}

} // namespace Lumora::Arch::x86
