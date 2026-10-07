#ifndef KERNEL_KERNEL_H
#define KERNEL_KERNEL_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define FS_NAME_LENGTH 64
#define FS_CONTENT_LENGTH 512
#define USERNAME_LENGTH 16
#define PASSWORD_LENGTH 16

void io_outb(uint16_t port, uint8_t value);
uint8_t io_inb(uint16_t port);
void io_outw(uint16_t port, uint16_t value);
uint16_t io_inw(uint16_t port);

void kmemset(void *ptr, uint8_t value, uint32_t size);
void kmemcpy(void *dst, const void *src, uint32_t size);
int kstrlen(const char *str);
int kstrcmp(const char *a, const char *b);
int kstrncmp(const char *a, const char *b, int n);
void kstrcpy(char *dst, const char *src);
void kstrcat(char *dst, const char *src);
int kcopy_bounded(char *dst, const char *src, int max);

void putchar_kernel(char c);
void print(const char *str);
void print_int(uint32_t value);
void clear_screen(void);
void read_line_input(char *buffer, int max_len);
void leaf_editor(const char *filename);
void neofetch(void);
void print_string_at(int row, int col, const char *str, uint8_t color);
char braille_to_ascii(uint8_t dots);

extern volatile uint8_t *vga;
extern int cursor_row;
extern int cursor_col;
extern int cursor_row_hw;
extern int cursor_col_hw;
void update_cursor(void);

extern int shift_pressed;
extern int ctrl_pressed;
char keyboard_get_char(uint8_t scancode, int shifted);
uint8_t keyboard_wait_scancode(void);

int ata_init(void);
int ata_is_ready(void);
int ata_read_sector(uint32_t lba, uint8_t *buffer);
int ata_write_sector(uint32_t lba, const uint8_t *buffer);

void fs_init(void);
void fs_boot_login_prompt(void);
void shell(void);

void kernel_main(void);

#endif
