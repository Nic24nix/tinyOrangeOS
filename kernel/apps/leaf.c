#include <stdint.h>
#include "../kernel.h"
#include "../fs/slicefs.h"

static int leaf_columns(void)
{
    return console_graphics_active() ? 49 : VGA_WIDTH;
}

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

            while (c >= leaf_columns())
            {
                c -= leaf_columns();
                r++;
            }
        }
        else
        {
            c++;

            if (c >= leaf_columns())
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

    if (c >= leaf_columns())
        c = leaf_columns() - 1;

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

            while (col >= leaf_columns())
            {
                col -= leaf_columns();
                line++;
            }
        }
        else
        {
            col++;

            if (col >= leaf_columns())
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

    if (console_graphics_active())
    {
        char view[15][50];
        int line=0, column=0, i;
        for(row=0;row<15;row++){
            for(col=0;col<49;col++)view[row][col]=' ';
            view[row][49]='\0';
        }
        for(i=0;i<length&&line<15;i++){
            char ch=buffer[i];
            if(ch=='\n'){line++;column=0;continue;}
            if(ch=='\t'){
                int spaces=4;
                while(spaces--&&line<15){view[line][column++]=' ';if(column>=49){column=0;line++;}}
                continue;
            }
            view[line][column++]=(ch>=32&&ch<=126)?ch:'?';
            if(column>=49){column=0;line++;}
        }
        leaf_get_cursor_xy(buffer,cursor_pos,&row,&col);
        if(row<15&&col<49)view[row][col]='_';
        print_string_at(0,0," tinyLeaf - OrangeOS Text Editor",0x0F);
        print_string_at(1,0," Ctrl+S Save   Ctrl+Q Exit",0x07);
        for(i=0;i<15;i++)print_string_at(i+2,0,view[i],0x07);
        print_string_at(17,0,"Editing in graphical terminal",0x07);
        if(row>14)row=14;
        print_string_at(row+2,col,"",0x0F);
        return;
    }

    /*
     * Limpar área de edição:
     * linhas 2..22
     */
    for (row = 2; row < 23; row++)
    {
        for (col = 0; col < leaf_columns(); col++)
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
                if (col >= leaf_columns())
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

        if (col >= leaf_columns())
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

        if(!console_graphics_active()){
            cursor_row_hw = cursor_r;
            cursor_col_hw = cursor_c;
            update_cursor();
        }
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
