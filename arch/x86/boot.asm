; Multiboot header (
section .multiboot
align 4
dd 0x1BADB002               ; Magic number
dd 0x0                      ; Flags
dd -(0x1BADB002 + 0x0)      ; Checksum

section .text32
bits 32                     ; GRUB drops us in 32-bit Protected Mode
global start
extern kernel_main          ; Reverted back to look for your standard kernel_main function

start:
    cli                     ; Disable hardware interrupts
    mov esp, stack_top      ; Set up temporary 32-bit stack

    ; Set up the 4-Level Page Tables for 64-bit mode
    call setup_page_tables

    ; Point CR3 register to the root PML4 table
    mov eax, pml4_table
    mov cr3, eax

    ; Enable PAE (Physical Address Extension)
    mov eax, cr4
    or eax, 0x20            ; Set PAE bit (1 << 5)
    mov cr4, eax

    ; Enable Long Mode inside EFER MSR
    mov ecx, 0xC0000080     ; EFER MSR address
    rdmsr
    or eax, 0x100           ; Set LME bit (1 << 8)
    wrmsr

    ; Turn on Paging and Protected Mode
    mov eax, cr0
    or eax, 0x80000001      ; Set PG bit (1 << 31) and PE bit (1 << 0)
    mov cr0, eax

    ; Load the 64-bit Global Descriptor Table
    lgdt [gdt64_pointer]

    ; Far jump to switch CPU instruction pipeline into 64-bit mode
    jmp 0x08:start_64bit

setup_page_tables:
    ; Link PML4 entry 0 -> PDPT table
    mov eax, pdpt_table
    or eax, 0x3             ; Present bit | Read/Write bit
    mov [pml4_table], eax

    ; Link PDPT entry 0 -> Page Directory table
    mov eax, pd_table
    or eax, 0x3             ; Present bit | Read/Write bit
    mov [pdpt_table], eax

    ; Link Page Directory entry 0 -> Map a huge 2MB block directly
    mov dword [pd_table], 0x83
    ret

section .text
bits 64                     ; Switch context to 64-bit instructions
start_64bit:
    ; Nullify segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax


    call kernel_main

.hang:
    hlt
    jmp .hang

; 64-Bit Transition GDT Data

section .data
align 8
gdt64:
    dq 0x0000000000000000   ; 0x00: Null Descriptor
    dq 0x00209A0000000000   ; 0x08: 64-bit Kernel Code Segment (L-bit set)
    dq 0x0000920000000000   ; 0x10: 64-bit Kernel Data Segment

gdt64_pointer:
    dw $ - gdt64 - 1        ; Limit of GDT table
    dd gdt64                ; Explicit 32-bit pointer width reference address

section .bss
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 4096

align 16
stack_bottom:
    resb 16384
stack_top:
