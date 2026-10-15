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

	; 1. Load clean 32-bit GDT layout into the CPU
	lgdt [gdt_descriptor]

	; 2. Reload the Code Segment (CS) register using a 32-bit Far Jump
	jmp 0x08:.reload_segments

.reload_segments:
	; 3. Reload all data and stack segment registers with 32-bit Data index (0x10)
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; 4. Set up the 4-level Page Tables
	call setup_page_tables

	; 5. Enable PAE (Physical Address Extension) - Required for Long Mode
	mov eax, cr4
	or eax, 1 << 5      ; Set PAE-bit (bit 5)
	mov cr4, eax

	; 6. Set Long Mode Bit in the EFER MSR (Extended Feature Enable Register)
	mov ecx, 0xC0000080 ; EFER MSR register address
	rdmsr               ; Read current MSR into EAX
	or eax, 1 << 8      ; Set Long Mode Active bit (bit 8)
	wrmsr               ; Write it back

	; 7. Enable Paging to turn on Long Mode execution
	mov eax, pml4_table ; Load the base address of our PML4 table directly
	mov cr3, eax

	mov eax, cr0
	or eax, 1 << 31     ; Set Paging-bit (bit 31)
	mov cr0, eax

	; 8. Load the GDT Pointer again (Updates GDTR to handle 64-bit base safely)
	lgdt [gdt_descriptor]

	; 9. Jump into the 64-bit Code Segment (0x18)
	jmp 0x18:start_64bit


; --- Helper function to build page tables safely in 32-bit mode ---
setup_page_tables:
	; Zero out all 4 page tables first to remove uninitialized RAM garbage
	; (4 tables * 4096 bytes = 16384 bytes = 4096 dwords)
	mov ecx, 4096
	xor eax, eax
	mov edi, pml4_table
	rep stosd

	; Link PML4 Entry 0 -> PDPT Table
	mov eax, pdpt_table
	or eax, 0b11        ; Present + Writable flags
	mov [pml4_table], eax

	; Link PDPT Entry 0 -> Page Directory Table
	mov eax, pd_table
	or eax, 0b11        ; Present + Writable flags
	mov [pdpt_table], eax

	; Link Page Directory Entry 0 -> Page Table
	mov eax, pt_table
	or eax, 0b11        ; Present + Writable flags
	mov [pd_table], eax

	; Map the Page Table entries (Identity map 512 entries * 4KB = 2MB total space)
	mov ecx, 0          ; Loop counter index

.map_pt_loop:
	mov eax, 0x1000     ; 4096 bytes per page frame
	mul ecx             ; EAX = counter * 4096 (Physical address)
	or eax, 0b11        ; Present + Writable flags
	
	; Safely write the 64-bit entry using two 32-bit steps
	mov [pt_table + ecx * 8], eax         ; Write lower 32-bits (address + flags)
	mov dword [pt_table + ecx * 8 + 4], 0 ; Explicitly zero out upper 32-bits

	inc ecx
	cmp ecx, 512        ; Have we filled all 512 entries?
	jne .map_pt_loop
	ret


; --- 64-Bit Mode Code Execution Section ---
bits 64
start_64bit:
	; Reload data registers with your 64-bit Data Segment index (0x20)
	mov ax, 0x20
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; Jump to C/C++ 64-bit kernel main function
	call kernel_main

.hang_64:
	hlt
	jmp .hang_64


align 8
gdt:
	; 0x00 - Null Descriptor
	dq 0x0000000000000000

	; 0x08 - 32-Bit code segment descriptor
	dw 0xffff     
	dw 0          
	db 0          
	db 0x9a       
	db 11001111b  
	db 0          

	; 0x10 - 32 bit Data Segment Descriptor
	dw 0xffff     
	dw 0          
	db 0          
	db 0x92       
	db 11001111b  
	db 0          

	; 0x18 - 64 bit code segment descriptor
	dw 0x0000     
	dw 0x0000     
	db 0x00       
	db 0x9A       
	db 0x20       ; Long mode flag active
	db 0x00       

	; 0x20 - 64 bit data segment descriptor
	dw 0x0000     
	dw 0x0000     
	db 0x00       
	db 0x92       
	db 0x00       
	db 0x00       

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt - 1 
	dq gdt               

section .bss
align 4096
pml4_table:
	resb 4096
pdpt_table:
	resb 4096
pd_table:
	resb 4096
pt_table:
	resb 4096

align 16
stack_bottom:
	resb STACK_SIZE
stack_top:
