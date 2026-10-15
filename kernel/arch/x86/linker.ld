ENTRY(start)

/* Tell the linker to output a proper 64-bit ELF binary */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

SECTIONS
{
    /* Multiboot kernels usually start at the 1MB physical memory mark */
    . = 0x100000;

    .text : {
       
        KEEP(*(.multiboot))
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
}
