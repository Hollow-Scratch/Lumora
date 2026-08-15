TARGET := kernel.bin
ISO := lumora.iso

AS := nasm
CC := g++
LD := ld


CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector
LDFLAGS := -m elf_i386 -T arch/x86/linker.ld

.PHONY: all clean iso run

all: $(ISO)


boot.o: arch/x86/boot.asm
	$(AS) -f elf32 $< -o $@

kernel.o: kernel/kernel.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): boot.o kernel.o arch/x86/linker.ld
	$(LD) $(LDFLAGS) -o $@ boot.o kernel.o

iso: $(TARGET)
	cp $(TARGET) iso/boot/kernel.bin
	grub-mkrescue -o $(ISO) iso

$(ISO): iso

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

clean:
	rm -f boot.o kernel.o $(TARGET) $(ISO)
	rm -f iso/boot/kernel.bin
