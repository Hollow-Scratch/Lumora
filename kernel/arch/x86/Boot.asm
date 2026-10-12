MULTIBOOT_MAGIC equ 0x1BADB002
MULTIBOOT_FLAG equ 0
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAG)

STACK_SIZE equ 16384

section .multiboot
dd MULTIBOOT_MAGIC
dd MULTIBOOT_FLAG
dd MULTIBOOT_CHECKSUM

section .text32
bits 32
global start
extern kernel_main

start:
	cli
	mov esp, stack_top

	call kernel_main

.hang:
	hlt
	jmp .hang

section .bss
align 16
stack_bottom:
	resb STACK_SIZE
stack_top:
