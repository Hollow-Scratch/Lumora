bits 64
default rel

section .text

extern interrupt_dispatch
global isr_default
global isr_stub_table

; Common entry point.
; Keep this label defined before the generated stubs.
isr_common:
    cld

    ; Save all general-purpose registers.
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Keep the frame pointer outside the C argument register. RBX is saved in
    ; the frame and restored below; the call may freely clobber caller-saved regs.
    mov r12, rsp
    mov rdi, rsp

    ; Align the stack before calling C++.
    and rsp, -16
    call interrupt_dispatch

    ; Restore the original stack.
    mov rsp, r12

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Discard vector and error code.
    add rsp, 16
    iretq

; Exception without a CPU-generated error code.
%macro ISR_NO_ERROR 1
global isr_%1
isr_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

; Exception with a CPU-generated error code.
%macro ISR_ERROR 1
global isr_%1
isr_%1:
    push qword %1
    jmp isr_common
%endmacro

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_ERROR    8
ISR_NO_ERROR 9
ISR_ERROR    10
ISR_ERROR    11
ISR_ERROR    12
ISR_ERROR    13
ISR_ERROR    14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_ERROR    17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_ERROR    21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_ERROR    29
ISR_ERROR    30
ISR_NO_ERROR 31

; Fallback for vectors without a dedicated stub.
isr_default:
    push qword 0
    push qword 255
    jmp isr_common

section .rodata
align 8

isr_stub_table:
    dq isr_0,  isr_1,  isr_2,  isr_3
    dq isr_4,  isr_5,  isr_6,  isr_7
    dq isr_8,  isr_9,  isr_10, isr_11
    dq isr_12, isr_13, isr_14, isr_15
    dq isr_16, isr_17, isr_18, isr_19
    dq isr_20, isr_21, isr_22, isr_23
    dq isr_24, isr_25, isr_26, isr_27
    dq isr_28, isr_29, isr_30, isr_31
