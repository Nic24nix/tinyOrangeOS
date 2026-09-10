TARGET = tinyOrangeOS-v0.4.0-i386
ISO = $(TARGET).iso

CC = gcc
AS = as
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra
LDFLAGS = -m elf_i386 -T linker.ld

BOOT_OBJ = boot/boot.o
KERNEL_OBJ = kernel/kernel.o

all: $(ISO)

boot/boot.o: boot/boot.s
	$(AS) --32 $< -o $@

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(BOOT_OBJ) $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) -o $@ $(BOOT_OBJ) $(KERNEL_OBJ)

iso/boot/kernel.bin: kernel.bin
	mkdir -p iso/boot
	cp kernel.bin iso/boot/kernel.bin

iso/boot/grub/grub.cfg:
	mkdir -p iso/boot/grub
	printf '%s\n' \
		'set timeout=0' \
		'set default=0' \
		'' \
		'menuentry "tinyOrangeOS" {' \
		'    multiboot /boot/kernel.bin' \
		'    boot' \
		'}' > iso/boot/grub/grub.cfg

$(ISO): iso/boot/kernel.bin iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

clean:
	rm -f boot/boot.o
	rm -f kernel/kernel.o
	rm -f kernel.bin
	rm -rf iso
	rm -f *.iso

rebuild: clean all

.PHONY: all clean rebuild
