.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set VIDEO,    1<<2
.set FLAGS,    ALIGN | MEMINFO | VIDEO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0                  /* header_addr (unused without bit 16) */
.long 0                  /* load_addr */
.long 0                  /* load_end_addr */
.long 0                  /* bss_end_addr */
.long 0                  /* entry_addr */
.long 0                  /* linear graphics mode */
.long 640                /* width */
.long 480                /* height */
.long 32                 /* bits per pixel */

.section .text
.global _start
.type _start, @function

_start:
    cli
    mov $stack_top, %esp
    push %ebx
    call kernel_main

hang:
    hlt
    jmp hang

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:
