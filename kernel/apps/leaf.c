#include <stdint.h>
#include "../kernel.h"
#include "../fs/slicefs.h"

/* =========================================================
   LEAF CURSOR POSITION
   ========================================================= */

static void leaf_get_cursor_xy(
    const char *buffer,
    int pos,
    int *row,
    int *col
)
{
    int r = 0;
    int c = 0;
    int i;

    for (i = 0; i < pos; i++)
    {
        char ch = buffer[i];

        if (ch == '\n')
        {
            r++;
            c = 0;
        }
        else if (ch == '\t')
        {
            c += 4;

            while (c >= VGA_WIDTH)
            {
                c -= VGA_WIDTH;
                r++;
            }
        }
        else
        {
            c++;

            if (c >= VGA_WIDTH)
            {
                c = 0;
                r++;
            }
        }
    }

    if (r < 0)
        r = 0;

    if (c < 0)
        c = 0;

    if (r > 20)
        r = 20;

    if (c >= VGA_WIDTH)
        c = VGA_WIDTH - 1;

    *row = r;
    *col = c;
}

/* =========================================================
   LEAF FIND POSITION
   ========================================================= */

static int leaf_find_position(
    const char *buffer,
    int length,
    int target_line,
    int target_col
)
{
    int line = 0;
    int col = 0;
    int i;

    if (target_line < 0)
        target_line = 0;

    if (target_col < 0)
        target_col = 0;

    for (i = 0; i < length; i++)
    {
        char ch = buffer[i];

        if (line == target_line &&
            col >= target_col)
        {
            return i;
        }

        if (ch == '\n')
        {
            if (line == target_line)
                return i + 1;

            line++;
            col = 0;
        }
        else if (ch == '\t')
        {
            col += 4;

            while (col >= VGA_WIDTH)
            {
                col -= VGA_WIDTH;
                line++;
            }
        }
        else
        {
            col++;

            if (col >= VGA_WIDTH)
            {
                col = 0;
                line++;
            }
        }
    }

    return length;
}

/* =========================================================
   LEAF REDRAW
   ========================================================= */

static void redraw_leaf_text(
    const char *buffer,
    int cursor_pos
)
{
    int row;
    int col;
    int i;
    int length;

    length = kstrlen(buffer);

    /*
     * Limpar área de edição:
     * linhas 2..22
     */
    for (row = 2; row < 23; row++)
    {
        for (col = 0; col < VGA_WIDTH; col++)
        {
            int pos =
                (row * VGA_WIDTH + col) * 2;

            vga[pos] = ' ';
            vga[pos + 1] = 0x07;
        }
    }

    row = 2;
    col = 0;

    for (i = 0; i < length; i++)
    {
        char ch = buffer[i];

        if (row >= 23)
            break;

        if (ch == '\n')
        {
            row++;
            col = 0;
            continue;
        }

        if (ch == '\t')
        {
            int spaces = 4;

            while (spaces--)
            {
                if (col >= VGA_WIDTH)
                {
                    col = 0;
                    row++;

                    if (row >= 23)
                        break;
                }

                if (row < 23)
                {
                    int pos =
                        (row * VGA_WIDTH + col) * 2;

                    vga[pos] = ' ';
                    vga[pos + 1] = 0x07;

                    col++;
                }
            }

            continue;
        }

        if (ch >= 32 && ch <= 126)
        {
            int pos =
                (row * VGA_WIDTH + col) * 2;

            vga[pos] = (uint8_t)ch;
            vga[pos + 1] = 0x07;
        }

        col++;

        if (col >= VGA_WIDTH)
        {
            col = 0;
            row++;
        }
    }

    /*
     * Cursor.
     */
    {
        int cursor_r;
        int cursor_c;

        leaf_get_cursor_xy(
            buffer,
            cursor_pos,
            &cursor_r,
            &cursor_c
        );

        cursor_r += 2;

        if (cursor_r >= 23)
            cursor_r = 22;

        cursor_row_hw = cursor_r;
        cursor_col_hw = cursor_c;

        update_cursor();
    }
}

/* =========================================================
   LEAF INSERT
   ========================================================= */

static int leaf_insert_char(
    char *buffer,
    int *length,
    int *cursor_pos,
    char ch
)
{
    int i;

    if (*length >= FS_CONTENT_LENGTH - 1)
        return 0;

    for (i = *length;
         i > *cursor_pos;
         i--)
    {
        buffer[i] = buffer[i - 1];
    }

    buffer[*cursor_pos] = ch;

    (*length)++;
    (*cursor_pos)++;

    buffer[*length] = '\0';

    return 1;
}

/* =========================================================
   LEAF BACKSPACE
   ========================================================= */

static int leaf_backspace(
    char *buffer,
    int *length,
    int *cursor_pos
)
{
    int i;

    if (*cursor_pos <= 0)
        return 0;

    for (i = *cursor_pos - 1;
         i < *length;
         i++)
    {
        buffer[i] = buffer[i + 1];
    }

    (*cursor_pos)--;
    (*length)--;

    return 1;
}

/* =========================================================
   LEAF DELETE
   ========================================================= */

static int leaf_delete(
    char *buffer,
    int *length,
    int *cursor_pos
)
{
    int i;

    if (*cursor_pos >= *length)
        return 0;

    for (i = *cursor_pos;
         i < *length;
         i++)
    {
        buffer[i] = buffer[i + 1];
    }

    (*length)--;

    return 1;
}

/* =========================================================
   LEAF HOME
   ========================================================= */

static int leaf_home(
    const char *buffer,
    int cursor_pos
)
{
    int i = cursor_pos - 1;

    while (i >= 0 &&
           buffer[i] != '\n')
    {
        i--;
    }

    return i + 1;
}

/* =========================================================
   LEAF END
   ========================================================= */

static int leaf_end(
    const char *buffer,
    int length,
    int cursor_pos
)
{
    int i = cursor_pos;

    while (i < length &&
           buffer[i] != '\n')
    {
        i++;
    }

    return i;
}

/* =========================================================
   LEAF EDITOR
   ========================================================= */

void leaf_editor(
    const char *filename
)
{
    char buffer[FS_CONTENT_LENGTH];
    char target_path[FS_NAME_LENGTH];

    int length = 0;
    int cursor_pos = 0;
    int running = 1;

    /*
     * Criar ficheiro se necessário.
     */
    fs_touch(filename);

    if (!build_absolute_path(
            filename,
            target_path))
    {
        print(
            "Erro: caminho demasiado longo.\n"
        );

        return;
    }

    length = fs_read_absolute(
        target_path,
        buffer,
        sizeof(buffer)
    );

    if (length < 0)
    {
        buffer[0] = '\0';
        length = 0;
    }

    clear_screen();

    print_string_at(
        0,
        0,
        " LEAF - OrangeOS Text Editor",
        0x0F
    );

    print_string_at(
        1,
        0,
        " Ctrl+S: Guardar   Ctrl+Q: Sair",
        0x07
    );

    redraw_leaf_text(
        buffer,
        cursor_pos
    );

    while (running)
    {
        uint8_t scancode;

        scancode = keyboard_wait_scancode();

        /*
         * Extended keyboard.
         */
        if (scancode == 0xE0)
        {
            uint8_t key = keyboard_wait_scancode();

            /*
             * LEFT
             */
            if (key == 0x4B)
            {
                if (cursor_pos > 0)
                    cursor_pos--;
            }

            /*
             * RIGHT
             */
            else if (key == 0x4D)
            {
                if (cursor_pos < length)
                    cursor_pos++;
            }

            /*
             * UP
             */
            else if (key == 0x48)
            {
                int line;
                int col;

                leaf_get_cursor_xy(
                    buffer,
                    cursor_pos,
                    &line,
                    &col
                );

                if (line > 0)
                {
                    cursor_pos =
                        leaf_find_position(
                            buffer,
                            length,
                            line - 1,
                            col
                        );
                }
            }

            /*
             * DOWN
             */
            else if (key == 0x50)
            {
                int line;
                int col;

                leaf_get_cursor_xy(
                    buffer,
                    cursor_pos,
                    &line,
                    &col
                );

                cursor_pos =
                    leaf_find_position(
                        buffer,
                        length,
                        line + 1,
                        col
                    );
            }

            /*
             * DELETE
             */
            else if (key == 0x53)
            {
                leaf_delete(
                    buffer,
                    &length,
                    &cursor_pos
                );
            }

            /*
             * HOME
             */
            else if (key == 0x47)
            {
                cursor_pos =
                    leaf_home(
                        buffer,
                        cursor_pos
                    );
            }

            /*
             * END
             */
            else if (key == 0x4F)
            {
                cursor_pos =
                    leaf_end(
                        buffer,
                        length,
                        cursor_pos
                    );
            }

            redraw_leaf_text(
                buffer,
                cursor_pos
            );

            continue;
        }

        /*
         * SHIFT.
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
         * CTRL.
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
         * Releases.
         */
        if (scancode & 0x80)
            continue;

        /*
         * Ctrl+S.
         */
        if (ctrl_pressed &&
            scancode == 0x1F)
        {
            if (fs_write_absolute(
                    target_path,
                    buffer))
            {
                print_string_at(
                    23,
                    0,
                    "Guardado!                       ",
                    0x2F
                );
            }
            else
            {
                print_string_at(
                    23,
                    0,
                    "Erro ao guardar!                ",
                    0x4F
                );
            }

            redraw_leaf_text(
                buffer,
                cursor_pos
            );

            continue;
        }

        /*
         * Ctrl+Q.
         */
        if (ctrl_pressed &&
            scancode == 0x10)
        {
            running = 0;
            continue;
        }

        /*
         * ENTER.
         */
        if (scancode == 0x1C)
        {
            leaf_insert_char(
                buffer,
                &length,
                &cursor_pos,
                '\n'
            );

            redraw_leaf_text(
                buffer,
                cursor_pos
            );

            continue;
        }

        /*
         * BACKSPACE.
         */
        if (scancode == 0x0E)
        {
            leaf_backspace(
                buffer,
                &length,
                &cursor_pos
            );

            redraw_leaf_text(
                buffer,
                cursor_pos
            );

            continue;
        }

        /*
         * TAB.
         */
        if (scancode == 0x0F)
        {
            leaf_insert_char(
                buffer,
                &length,
                &cursor_pos,
                '\t'
            );

            redraw_leaf_text(
                buffer,
                cursor_pos
            );

            continue;
        }

        /*
         * Tecla normal.
         */
        {
            char c =
                keyboard_get_char(
                    scancode,
                    shift_pressed
                );

            if (c >= 32 &&
                c <= 126)
            {
                leaf_insert_char(
                    buffer,
                    &length,
                    &cursor_pos,
                    c
                );

                redraw_leaf_text(
                    buffer,
                    cursor_pos
                );
            }
        }
    }

    clear_screen();
}

