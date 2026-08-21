TARGET := kernel.bin
ISO := lumora.iso

AS := nasm
CC := g++
LD := ld

CFLAGS := -m64 -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -mno-mmx -mno-sse -Iinclude

LDFLAGS := -m elf_x86_64 -T arch/x86/linker.ld

.PHONY: all clean iso run

all: $(ISO)

# FIX: Changed -f elf32 to -f elf64
boot.o: arch/x86/boot.asm
	$(AS) -f elf64 $< -o $@

kernel.o: kernel/kernel.cpp
	$(CC) $(CFLAGS) -c $< -o $@

printv.o: kernel/printv.cpp
	$(CC) $(CFLAGS) -c $< -o $@

gdt.o: kernel/gdt.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): boot.o kernel.o printv.o gdt.o arch/x86/linker.ld
	$(LD) $(LDFLAGS) -o $@ boot.o kernel.o printv.o gdt.o

iso: $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/kernel.bin
	grub-mkrescue -o $(ISO) iso

$(ISO): iso

run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO)

clean:
	rm -f boot.o kernel.o printv.o gdt.o $(TARGET) $(ISO)
	rm -f iso/boot/kernel.bin
