#include "IDT.hpp"

namespace Lumora::Arch::x86 {

alignas(16) static IDTEntry idt[256]{};
static IDTPointer idt_pointer{};

extern "C" {
    extern void* isr_stub_table[32];
    extern void isr_default();
    extern uint32_t multiboot_magic;
    extern uint32_t multiboot_info;
    void interrupt_dispatch(InterruptFrame* frame);
}

static void SetGate(uint8_t vector, void (*handler)()) {
    const uint64_t address = reinterpret_cast<uint64_t>(handler);
    IDTEntry& entry = idt[vector];
    entry.offset_1 = static_cast<uint16_t>(address);
    entry.selector = 0x18; // GDT long-mode kernel code selector.
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
        volatile auto* screen = reinterpret_cast<volatile uint16_t*>(0xB8000);
        const char* message = "IDTR VERIFY FAILED - HALTING";
        for (unsigned i = 0; message[i]; ++i)
            screen[i] = static_cast<uint16_t>(0x4F00 | message[i]);
        asm volatile("cli");
        for (;;) asm volatile("hlt");
    }
}

static const char* ExceptionName(uint64_t vector) {
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

static void WriteText(unsigned row, const char* text) {
    volatile auto* screen = reinterpret_cast<volatile uint16_t*>(0xB8000);
    unsigned column = 0;
    while (text[column] && column < 80) {
        screen[row * 80 + column] = static_cast<uint16_t>(0x0F00 | text[column]);
        ++column;
    }
}

static void WriteHex(unsigned row, unsigned column, uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    volatile auto* screen = reinterpret_cast<volatile uint16_t*>(0xB8000);
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned shift = (15 - i) * 4;
        screen[row * 80 + column + i] =
            static_cast<uint16_t>(0x0F00 | digits[(value >> shift) & 0xF]);
    }
}

extern "C" void interrupt_dispatch(InterruptFrame* frame) {
    const uint64_t vector = frame->vector;
    const bool is_exception = vector < 32;
    const bool has_error = vector == 8 || (vector >= 10 && vector <= 14) ||
        vector == 17 || vector == 21 || vector == 29 || vector == 30;

    asm volatile("cli" ::: "memory");
    WriteText(0, "LUMORA FATAL EXCEPTION - CPU HALTED");
    WriteText(1, is_exception ? ExceptionName(vector) : "Unexpected interrupt");
    WriteText(2, "VECTOR:"); WriteHex(2, 8, vector);
    WriteText(3, "ERROR CODE:");
    if (has_error) WriteHex(3, 12, frame->error_code);
    else {
        const char* text = "not pushed by CPU";
        volatile auto* screen = reinterpret_cast<volatile uint16_t*>(0xB8000);
        for (unsigned i = 0; text[i]; ++i)
            screen[3 * 80 + 12 + i] = static_cast<uint16_t>(0x0F00 | text[i]);
    }
    WriteText(4, "SAVED RIP:"); WriteHex(4, 12, frame->rip);
    WriteText(5, "HALTING: YES");
    for (;;) asm volatile("hlt");
}

} // namespace Lumora::Arch::x86
