#include <stdint.h>
#include "../kernel.h"

/* =========================================================
   NEOFETCH
   ========================================================= */

/*
 * Escreve uma linha de Braille no VGA.
 *
 * O VGA nao consegue mostrar Unicode diretamente,
 * portanto cada caractere Braille e convertido
 * para um caractere ASCII simples.
 */
void neofetch_braille_line(
    int row,
    int col,
    const char *str
)
{
    int i = 0;

    if (row < 0 || row >= VGA_HEIGHT)
        return;

    while (str[i] != '\0' &&
           col < VGA_WIDTH)
    {
        uint8_t c = (uint8_t)str[i];

        /*
         * Braille UTF-8:
         *
         * U+2800..U+28FF
         * E2 A0 80 .. E2 A3 BF
         */
        if ((c & 0xF0) == 0xE0)
        {
            uint8_t c2 = (uint8_t)str[i + 1];
            uint8_t c3 = (uint8_t)str[i + 2];

            if ((c2 & 0xC0) == 0x80 &&
                (c3 & 0xC0) == 0x80)
            {
                if (c == 0xE2 &&
                    c2 >= 0xA0 &&
                    c2 <= 0xA3)
                {
                    uint8_t dots;

                    /*
                     * Para U+2800..U+28FF,
                     * o terceiro byte contem
                     * diretamente os 6 bits inferiores.
                     */
                    dots = c3 & 0x3F;

                    /*
                     * Os bits adicionais do segundo
                     * byte sao necessarios para obter
                     * os 8 bits completos do Braille.
                     */
                    if (c2 == 0xA1)
                        dots |= 0x40;

                    else if (c2 == 0xA2)
                        dots |= 0x80;

                    else if (c2 == 0xA3)
                        dots |= 0xC0;

                    {
                        int pos =
                            (row * VGA_WIDTH + col) * 2;

                        vga[pos] =
                            (uint8_t)braille_to_ascii(dots);

                        vga[pos + 1] = 0x07;
                    }

                    col++;
                    i += 3;
                    continue;
                }
            }

            i += 3;
            continue;
        }

        /*
         * UTF-8 de 2 bytes.
         */
        if ((c & 0xE0) == 0xC0)
        {
            i += 2;
            continue;
        }

        /*
         * ASCII normal.
         */
        if (c >= 32 && c <= 126)
        {
            int pos =
                (row * VGA_WIDTH + col) * 2;

            vga[pos] = c;
            vga[pos + 1] = 0x07;

            col++;
        }

        i++;
    }
}

void neofetch(void)
{
    clear_screen();

    /*
     * Titulo.
     */
    print_string_at(
        0,
        28,
        "OrangeOS neofetch",
        0x0F
    );

    /*
     * Logo:
     *
     * 15 linhas, colocado a partir da
     * linha 5 para ficar centrado.
     */
    neofetch_braille_line(
        5,
        1,
        "⠀⠀⠀⠀⣀⣀⣀⣀⣀⠀⠀⠀⢀⣠⡴⢶⡾⣆⠀⠀⠀⠀⠀⠀"
    );

    neofetch_braille_line(
        6,
        1,
        "⠀⠐⣾⠛⠋⠉⠛⠋⠙⢻⣦⣠⡟⠁⠀⢸⢠⡿⠀⠀⠀⠀⠀⠀"
    );

    neofetch_braille_line(
        7,
        1,
        "⠀⠀⠹⣦⡀⠀⠀⠀⠀⠀⢹⣿⣓⣠⣴⣵⠟⠁⠀⠀⠀⠀⠀⠀"
    );

    neofetch_braille_line(
        8,
        1,
        "⠀⠀⠀⠘⠻⣦⣤⣤⣤⣤⣼⠾⠻⠿⠿⣷⣤⡀⠀⠀⠀⠀⠀⠀"
    );

    neofetch_braille_line(
        9,
        1,
        "⠀⠀⠀⣠⡾⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠉⠪⣽⢷⣄⠀⠀⠀⠀"
    );

    neofetch_braille_line(
        10,
        1,
        "⠀⢠⡾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢣⡙⢷⡄⠀⠀"
    );

    neofetch_braille_line(
        11,
        1,
        "⢠⡟⠁⢰⣰⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢱⡈⢿⡄⠀"
    );

    neofetch_braille_line(
        12,
        1,
        "⣾⠃⠀⠀⠀⣀⡢⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⠘⣿⠀"
    );

    neofetch_braille_line(
        13,
        1,
        "⣿⠀⠸⠜⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀⢿⡶"
    );

    neofetch_braille_line(
        14,
        1,
        "⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀⣿⠀"
    );

    neofetch_braille_line(
        15,
        1,
        "⢿⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡄⢠⡿⠀"
    );

    neofetch_braille_line(
        16,
        1,
        "⠈⢿⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⢁⣾⠃⠀"
    );

    neofetch_braille_line(
        17,
        1,
        "⠀⠈⢻⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⣰⡟⠁⠀⠀"
    );

    neofetch_braille_line(
        18,
        1,
        "⠀⠀⠀⠙⠷⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣐⣥⠟⠋⠀⠀⠀⠀"
    );

    neofetch_braille_line(
        19,
        1,
        "⠀⠀⠀⠀⠀⠈⠉⠛⠶⠶⠦⠤⠶⣶⠾⠛⠋⠁⠀⠀⠀⠀⠀⠀"
    );

    /*
     * Informacoes:
     *
     * colocadas a direita do logo
     * e centradas verticalmente.
     */
    print_string_at(
        8,
        57,
        "OrangeOS v0.5.0",
        0x0F
    );

    print_string_at(
        9,
        57,
        "----------------",
        0x07
    );

    print_string_at(
        10,
        57,
        "Kernel: i386",
        0x07
    );

    print_string_at(
        11,
        57,
        "Shell: OrangeShell",
        0x07
    );

    print_string_at(
        12,
        57,
        "FS: SliceFS",
        0x07
    );

    print_string_at(
        13,
        57,
        "Editor: Leaf",
        0x07
    );

    print_string_at(
        14,
        57,
        "VGA: 80x25",
        0x07
    );

    /*
     * Cursor fora da area principal.
     */
    cursor_row_hw = 24;
    cursor_col_hw = 0;

    update_cursor();
}

