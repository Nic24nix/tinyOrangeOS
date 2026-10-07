#include <stdint.h>
#include "../kernel.h"

/* =========================================================
   KEYBOARD
   ========================================================= */

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

int shift_pressed = 0;
int ctrl_pressed = 0;

static const char keyboard_map[128] =
{
    0,
    27,
    '1','2','3','4','5','6','7','8','9','0',
    '-','=',
    '\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p',
    '[',']',
    '\n',
    0,
    'a','s','d','f','g','h','j','k','l',
    ';','\'','`',
    0,
    '\\',
    'z','x','c','v','b','n','m',
    ',','.','/',
    0,
    '*',
    0,
    ' '
};

static const char keyboard_shift_map[128] =
{
    0,
    27,
    '!','@','#','$','%','^','&','*','(',')',
    '_','+',
    '\b',
    '\t',
    'Q','W','E','R','T','Y','U','I','O','P',
    '{','}',
    '\n',
    0,
    'A','S','D','F','G','H','J','K','L',
    ':','"','~',
    0,
    '|',
    'Z','X','C','V','B','N','M',
    '<','>','?',
    0,
    '*',
    0,
    ' '
};

char keyboard_get_char(
    uint8_t scancode,
    int shifted
)
{
    if (scancode >= 128)
        return 0;

    if (shifted)
        return keyboard_shift_map[scancode];

    return keyboard_map[scancode];
}

/* =========================================================
   READ LINE
   ========================================================= */

void read_line_input(
    char *buffer,
    int max_len
)
{
    int length = 0;

    if (max_len <= 0)
        return;

    buffer[0] = '\0';

    while (1)
    {
        uint8_t scancode;

        if (!(io_inb(KEYBOARD_STATUS_PORT) & 1))
            continue;

        scancode = io_inb(KEYBOARD_DATA_PORT);

        /*
         * Extended key prefix.
         */
        if (scancode == 0xE0)
        {
            io_inb(KEYBOARD_DATA_PORT);
            continue;
        }

        /*
         * Shift.
         */
        if (scancode == 0x2A ||
            scancode == 0x36)
        {
            shift_pressed = 1;
            continue;
        }

        if (scancode == 0xAA ||
            scancode == 0xB6)
        {
            shift_pressed = 0;
            continue;
        }

        /*
         * Ctrl.
         */
        if (scancode == 0x1D)
        {
            ctrl_pressed = 1;
            continue;
        }

        if (scancode == 0x9D)
        {
            ctrl_pressed = 0;
            continue;
        }

        /*
         * Ignore releases.
         */
        if (scancode & 0x80)
            continue;

        /*
         * Enter.
         */
        if (scancode == 0x1C)
        {
            putchar_kernel('\n');
            buffer[length] = '\0';
            return;
        }

        /*
         * Backspace.
         */
        if (scancode == 0x0E)
        {
            if (length > 0)
            {
                length--;
                buffer[length] = '\0';

                if (cursor_col > 0)
                {
                    cursor_col--;
                    putchar_kernel(' ');
                    cursor_col--;
                }
            }

            continue;
        }

        {
            char c = keyboard_get_char(
                scancode,
                shift_pressed
            );

            if (c >= 32 && c <= 126)
            {
                if (length < max_len - 1)
                {
                    buffer[length++] = c;
                    buffer[length] = '\0';

                    putchar_kernel(c);
                }
            }
        }
    }
}


uint8_t keyboard_wait_scancode(void)
{
    while (!(io_inb(KEYBOARD_STATUS_PORT) & 1))
        ;
    return io_inb(KEYBOARD_DATA_PORT);
}
