#include <stdint.h>
#include "../kernel.h"

#define VGA_MEMORY ((volatile uint8_t *)0xB8000)

volatile uint8_t *vga = VGA_MEMORY;

int cursor_row = 0;
int cursor_col = 0;

int cursor_row_hw = 0;
int cursor_col_hw = 0;

uint8_t current_color = 0x07;
static int graphics_console_enabled = 0;

void console_set_graphics(int enabled)
{
    graphics_console_enabled = enabled;
}

int console_graphics_active(void)
{
    return graphics_console_enabled;
}

/* =========================================================
   VGA CURSOR
   ========================================================= */

void update_cursor(void)
{
    uint16_t position;

    if (cursor_row_hw < 0)
        cursor_row_hw = 0;

    if (cursor_col_hw < 0)
        cursor_col_hw = 0;

    if (cursor_row_hw >= VGA_HEIGHT)
        cursor_row_hw = VGA_HEIGHT - 1;

    if (cursor_col_hw >= VGA_WIDTH)
        cursor_col_hw = VGA_WIDTH - 1;

    position = (uint16_t)
        (cursor_row_hw * VGA_WIDTH + cursor_col_hw);

    io_outb(0x3D4, 0x0F);
    io_outb(0x3D5, (uint8_t)(position & 0xFF));

    io_outb(0x3D4, 0x0E);
    io_outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

/* =========================================================
   VGA SCROLL
   ========================================================= */

static void scroll(void)
{
    int row;
    int col;

    for (row = 1; row < VGA_HEIGHT; row++)
    {
        for (col = 0; col < VGA_WIDTH; col++)
        {
            int src = (row * VGA_WIDTH + col) * 2;
            int dst = ((row - 1) * VGA_WIDTH + col) * 2;

            vga[dst] = vga[src];
            vga[dst + 1] = vga[src + 1];
        }
    }

    for (col = 0; col < VGA_WIDTH; col++)
    {
        int pos = ((VGA_HEIGHT - 1) * VGA_WIDTH + col) * 2;

        vga[pos] = ' ';
        vga[pos + 1] = current_color;
    }

    cursor_row = VGA_HEIGHT - 1;
    cursor_col = 0;
}

/* =========================================================
   VGA OUTPUT
   ========================================================= */

void putchar_kernel(char c)
{
    int pos;

    if (graphics_console_enabled)
    {
        console_graphics_putchar(c);
        return;
    }

    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;

        if (cursor_row >= VGA_HEIGHT)
            scroll();

        cursor_row_hw = cursor_row;
        cursor_col_hw = cursor_col;
        update_cursor();

        return;
    }

    if (c == '\r')
    {
        cursor_col = 0;

        cursor_row_hw = cursor_row;
        cursor_col_hw = cursor_col;
        update_cursor();

        return;
    }

    if (c == '\t')
    {
        cursor_col += 4;

        if (cursor_col >= VGA_WIDTH)
        {
            cursor_col = 0;
            cursor_row++;

            if (cursor_row >= VGA_HEIGHT)
                scroll();
        }

        cursor_row_hw = cursor_row;
        cursor_col_hw = cursor_col;
        update_cursor();

        return;
    }

    pos = (cursor_row * VGA_WIDTH + cursor_col) * 2;

    vga[pos] = (uint8_t)c;
    vga[pos + 1] = current_color;

    cursor_col++;

    if (cursor_col >= VGA_WIDTH)
    {
        cursor_col = 0;
        cursor_row++;

        if (cursor_row >= VGA_HEIGHT)
            scroll();
    }

    cursor_row_hw = cursor_row;
    cursor_col_hw = cursor_col;

    update_cursor();
}

/* =========================================================
   BRAILLE / UTF-8
   ========================================================= */

char braille_to_ascii(uint8_t dots)
{
    int count = 0;
    int i;

    for (i = 0; i < 8; i++)
    {
        if (dots & (1 << i))
            count++;
    }

    if (count >= 6)
        return '#';

    if (count >= 4)
        return '*';

    if (count >= 2)
        return '+';

    if (count == 1)
        return '.';

    return ' ';
}

void print(const char *str)
{
    int i = 0;

    while (str[i] != '\0')
    {
        uint8_t c = (uint8_t)str[i];

        if ((c & 0xF0) == 0xE0)
        {
            uint8_t c2 = (uint8_t)str[i + 1];
            uint8_t c3 = (uint8_t)str[i + 2];

            if ((c2 & 0xC0) == 0x80 &&
                (c3 & 0xC0) == 0x80)
            {
                /*
                 * Braille Unicode:
                 * U+2800..U+28FF
                 */
                if (c == 0xE2 &&
                    c2 == 0xA0)
                {
                    putchar_kernel(
                        braille_to_ascii(c3 & 0x3F)
                    );
                }
                else
                {
                    putchar_kernel('?');
                }

                i += 3;
                continue;
            }
        }

        if ((c & 0xE0) == 0xC0)
        {
            i += 2;
            putchar_kernel('?');
            continue;
        }

        putchar_kernel((char)c);
        i++;
    }
}

/* =========================================================
   INTEGER OUTPUT
   ========================================================= */

void print_int(uint32_t value)
{
    char buffer[12];
    int i = 0;

    if (value == 0)
    {
        putchar_kernel('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
        putchar_kernel(buffer[--i]);
}

/* =========================================================
   CLEAR SCREEN
   ========================================================= */

void clear_screen(void)
{
    int row;
    int col;

    if (graphics_console_enabled)
    {
        console_graphics_clear();
        return;
    }

    for (row = 0; row < VGA_HEIGHT; row++)
    {
        for (col = 0; col < VGA_WIDTH; col++)
        {
            int pos = (row * VGA_WIDTH + col) * 2;

            vga[pos] = ' ';
            vga[pos + 1] = current_color;
        }
    }

    cursor_row = 0;
    cursor_col = 0;

    cursor_row_hw = 0;
    cursor_col_hw = 0;

    update_cursor();
}

void print_string_at(
    int row,
    int col,
    const char *str,
    uint8_t color
)
{
    int i = 0;

    if (graphics_console_enabled)
    {
        console_graphics_string_at(row, col, str, color);
        return;
    }

    if (row < 0 || row >= VGA_HEIGHT)
        return;

    while (
        str[i] != '\0' &&
        col < VGA_WIDTH)
    {
        if (str[i] == '\n')
            break;

        vga[
            (row * VGA_WIDTH + col) * 2
        ] = (uint8_t)str[i];

        vga[
            (row * VGA_WIDTH + col) * 2 + 1
        ] = color;

        col++;
        i++;
    }
}
