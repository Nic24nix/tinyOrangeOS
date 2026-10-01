TARGET = tinyOrangeOS-v0.4.0-i386
ISO = $(TARGET).iso

CC = gcc
AS = as
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra -Ikernel
LDFLAGS = -m elf_i386 -T linker.ld

BOOT_OBJ = boot/boot.o
KERNEL_OBJ = \
	kernel/kernel.o \
	kernel/core/io.o \
	kernel/lib/memory.o \
	kernel/lib/string.o \
	kernel/drivers/console.o \
	kernel/drivers/keyboard.o \
	kernel/drivers/ata.o \
	kernel/fs/slicefs.o \
	kernel/apps/leaf.o \
	kernel/apps/neofetch.o \
	kernel/shell/shell.o \
	kernel/timezone.o

all: $(ISO)

boot/boot.o: boot/boot.s
	$(AS) --32 $< -o $@

kernel/kernel.o: kernel/kernel.c kernel/kernel.h kernel/timezone.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/core/%.o: kernel/core/%.c kernel/kernel.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/lib/%.o: kernel/lib/%.c kernel/kernel.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/drivers/%.o: kernel/drivers/%.c kernel/kernel.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/apps/%.o: kernel/apps/%.c kernel/kernel.h kernel/fs/slicefs.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/fs/%.o: kernel/fs/%.c kernel/kernel.h kernel/fs/slicefs.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/shell/%.o: kernel/shell/%.c kernel/kernel.h kernel/fs/slicefs.h kernel/timezone.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/timezone.o: kernel/timezone.c kernel/timezone.h
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
	rm -f kernel/core/*.o kernel/lib/*.o kernel/drivers/*.o kernel/fs/*.o kernel/apps/*.o kernel/shell/*.o kernel/timezone.o
	rm -f kernel.bin
	rm -f iso/boot/kernel.bin
	rm -f *.iso

rebuild: clean all

.PHONY: all clean rebuild
