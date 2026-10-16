#pragma once
#include <cstdint>
#include <cstddef>


namespace Lumora::Arch::x86 {

struct [[gnu::packed]] IDTEntry {
   uint16_t offset_1;        // offset bits 0..15
   uint16_t selector;        // a code segment selector in GDT or LDT
   uint8_t  ist;             // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
   uint8_t  type_attributes; // gate type, dpl, and p fields
   uint16_t offset_2;        // offset bits 16..31
   uint32_t offset_3;        // offset bits 32..63
   uint32_t zero;            // reserved
};

struct [[gnu::packed]] IDTPointer  {
    uint16_t limit;
    uint64_t base;
};

// At entry to interrupt_dispatch: registers are in this order, followed by
// vector, normalized error code, and the CPU's RIP/CS/RFLAGS frame.
struct InterruptFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

static_assert(sizeof(IDTEntry) == 16);
static_assert(sizeof(IDTPointer) == 10);
static_assert(offsetof(IDTEntry, offset_3) == 8);
static_assert(offsetof(InterruptFrame, vector) == 15 * sizeof(uint64_t));
static_assert(offsetof(InterruptFrame, error_code) == 16 * sizeof(uint64_t));
static_assert(offsetof(InterruptFrame, rip) == 17 * sizeof(uint64_t));

void InitIDT();
using IRQHandler = void (*)(uint8_t irq, InterruptFrame* frame);
// Registration marks a handler ready and unmasks its PIC line. IRQs remain
// masked until this call succeeds; callers must register with IF clear.
bool RegisterIRQHandler(uint8_t irq, IRQHandler handler) noexcept;

}
