#include <stdint.h>
#include "../kernel.h"

/* =========================================================
   BASIC MEMORY FUNCTIONS
   ========================================================= */

void kmemset(void *ptr, uint8_t value, uint32_t size)
{
    uint8_t *p = (uint8_t *)ptr;
    uint32_t i;

    for (i = 0; i < size; i++)
        p[i] = value;
}

void kmemcpy(void *dst, const void *src, uint32_t size)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    uint32_t i;

    for (i = 0; i < size; i++)
        d[i] = s[i];
}

