ENTRY(start)

/* Tell the linker to output a proper 64-bit ELF binary */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

SECTIONS
{
    /* Keep the Multiboot 2 header inside its first-32-KiB search window. */
    . = 0x100000;

    .multiboot2 : ALIGN(8) { KEEP(*(.multiboot2)) }
    .text : {
        *(.text32)
        *(.text)

    }

    /* Align sections to 4KB boundaries */
    . = ALIGN(4096);

    .data : {
        *(.data)
        *(.rodata)
    }

    . = ALIGN(4096);

    .bss : {
        *(.bss)
        *(COMMON)
    }

    /DISCARD/ : { *(.note.GNU-stack) }
}
