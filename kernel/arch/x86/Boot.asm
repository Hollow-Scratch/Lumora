; i386 Multiboot 2 entry. GRUB enters in 32-bit protected mode with paging
; disabled, EAX=0x36d76289 and EBX=physical boot-info pointer.
%define MB2_HEADER_MAGIC 0xE85250D6
%define MB2_ARCH_I386    0
%define MB2_BOOT_MAGIC   0x36D76289
%define GDT_SEL_NULL     0x00
%define GDT_SEL_CODE32   0x08
%define GDT_SEL_DATA     0x10
%define GDT_SEL_CODE64   0x18
%define GDT_SEL_DATA64   0x20
%define STACK_SIZE       16384

section .multiboot2
align 8
mb2_header:
    dd MB2_HEADER_MAGIC
    dd MB2_ARCH_I386
    dd mb2_header_end - mb2_header
    dd -(MB2_HEADER_MAGIC + MB2_ARCH_I386 + (mb2_header_end - mb2_header))
    ; Optional information request: mmap (6), framebuffer (8), boot loader (2),
    ; and command line (1). Unknown/unavailable optional tags are harmless.
    dw 1, 1
    dd 24
    dd 6, 8, 2, 1
    align 8, db 0
    ; Request a preferred 1024x768x32 framebuffer; GRUB may choose another.
    dw 5, 0
    dd 20
    dd 1024, 768, 32
    align 8, db 0
    dw 0, 0
    dd 8
mb2_header_end:

section .text32
bits 32
global start
extern kernel_main
global mb2_handoff_magic
global mb2_handoff_info

start:
    cli
    cld
    mov [mb2_handoff_magic], eax
    mov [mb2_handoff_info], ebx
    mov esp, stack_top
    lgdt [gdt_descriptor]
    jmp GDT_SEL_CODE32:.reload_segments
.reload_segments:
    mov ax, GDT_SEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Build four 1 GiB identity-mapped regions using 2 MiB pages (0..4 GiB).
    mov edi, pml4_table
    xor eax, eax
    mov ecx, 6 * 1024
    rep stosd
    mov eax, pdpt_table
    or eax, 3
    mov [pml4_table], eax
    mov dword [pml4_table + 4], 0
    mov edi, pdpt_table
    mov eax, pd0_table
    or eax, 3
    mov [edi], eax
    mov eax, pd1_table
    or eax, 3
    mov [edi + 8], eax
    mov eax, pd2_table
    or eax, 3
    mov [edi + 16], eax
    mov eax, pd3_table
    or eax, 3
    mov [edi + 24], eax
    mov esi, pd0_table
    xor ebx, ebx
    mov ecx, 2048
.map_2m:
    mov eax, ebx
    or eax, 0x83                 ; present, writable, 2 MiB page
    mov [esi], eax
    mov dword [esi + 4], 0
    add esi, 8
    add ebx, 0x200000
    loop .map_2m

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8               ; EFER.LME
    wrmsr
    mov eax, pml4_table
    mov cr3, eax
    mov eax, cr0
    or eax, 1 << 31              ; CR0.PG activates long mode with PAE/LME
    mov cr0, eax
    ; GDTR already references this low, identity-mapped GDT. Far jump reloads CS.
    jmp GDT_SEL_CODE64:start_64bit

bits 64
start_64bit:
    mov ax, GDT_SEL_DATA64
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; SysV ABI: stack_top is 16-byte aligned; call establishes callee entry ABI.
    mov edi, [rel mb2_handoff_magic]
    mov esi, [rel mb2_handoff_info]
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

; Single bootstrap GDT remains valid in both modes. L=1 and D/B=0 for code64.
align 8
gdt:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF         ; 0x08 ring-0 32-bit code, 4 KiB granularity
    dq 0x00CF92000000FFFF         ; 0x10 ring-0 data, writable
    dq 0x00AF9A000000FFFF         ; 0x18 ring-0 64-bit code (L=1, D=0)
    dq 0x00CF92000000FFFF         ; 0x20 ring-0 data
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt - 1
    dd gdt

section .bss
alignb 8
mb2_handoff_magic: resd 1
mb2_handoff_info:  resd 1
alignb 4096
global pml4_table
global pd0_table
global pd1_table
global pd2_table
global pd3_table
pml4_table: resb 4096
pdpt_table: resb 4096
pd0_table:  resb 4096
pd1_table:  resb 4096
pd2_table:  resb 4096
pd3_table:  resb 4096
alignb 16
stack_bottom: resb STACK_SIZE
stack_top:
