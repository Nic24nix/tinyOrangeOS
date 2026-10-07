#include <stdint.h>
#include "../kernel.h"

/* =========================================================
   PORT I/O
   ========================================================= */

void io_outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t io_inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile ("inb %1, %0"
                      : "=a"(value)
                      : "Nd"(port));

    return value;
}

void io_outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

uint16_t io_inw(uint16_t port)
{
    uint16_t value;

    __asm__ volatile ("inw %1, %0"
                      : "=a"(value)
                      : "Nd"(port));

    return value;
}

