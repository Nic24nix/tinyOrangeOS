#include <stdint.h>

/* =========================================================
   ORANGE OS
   Kernel v0.4.1 - Multi-User System with Leaf Editor & Nano
   ========================================================= */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t*)0xB8000)
#define MAX_BUFFER 2048

static int cursor_row = 0;
static int cursor_col = 0;

/* =========================================================
   PORT I/O & VGA HARDWARE CURSOR
   ========================================================= */

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void update_vga_cursor(int row, int col) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void)
{
    if (cursor_row >= VGA_HEIGHT) {
        for (int y = 0; y < VGA_HEIGHT - 1; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
            }
        }
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ((uint16_t)0x07 << 8) | ' ';
        }
        cursor_row = VGA_HEIGHT - 1;
    }
}

static void putchar(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        scroll();
        update_vga_cursor(cursor_row, cursor_col);
        return;
    }

    VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = ((uint16_t)0x07 << 8) | (uint8_t)c;
    cursor_col++;

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        scroll();
    }
    update_vga_cursor(cursor_row, cursor_col);
}

static char braille_to_ascii(uint32_t codepoint)
{
    if (codepoint < 0x2800 || codepoint > 0x28FF) return '?';

    uint8_t dots = (uint8_t)(codepoint - 0x2800);
    if (dots == 0) return ' ';

    int count = 0;
    for (int i = 0; i < 8; i++) {
        if ((dots >> i) & 1) count++;
    }

    if (count <= 2) return '.';
    if (count <= 4) return ':';
    if (count <= 6) return '*';
    return '#';
}

static void print(const char *str)
{
    while (*str) {
        uint8_t c = (uint8_t)*str;

        if (c < 0x80) {
            putchar(c);
            str++;
        }
        else if ((c & 0xF0) == 0xE0) {
            uint8_t c2 = (uint8_t)*(str + 1);
            uint8_t c3 = (uint8_t)*(str + 2);

            if (c2 && c3) {
                uint32_t codepoint = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                if (codepoint >= 0x2800 && codepoint <= 0x28FF) {
                    putchar(braille_to_ascii(codepoint));
                } else {
                    putchar('?');
                }
                str += 3;
            } else {
                str++;
            }
        }
        else if ((c & 0xE0) == 0xC0) {
            str += 2;
            putchar('?');
        }
        else {
            str++;
        }
    }
}

static void print_int(uint32_t val)
{
    if (val == 0) {
        putchar('0');
        return;
    }
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    while (val > 0 && i >= 0) {
        buf[i--] = '0' + (val % 10);
        val /= 10;
    }
    print(&buf[i + 1]);
}

static void clear_screen(void)
{
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = ((uint16_t)0x07 << 8) | ' ';
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    update_vga_cursor(0, 0);
}

/* =========================================================
   STRINGS UTILS
   ========================================================= */

static int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int strncmp(const char *a, const char *b, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static uint32_t strlen(const char *str)
{
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

static void strcpy(char *dest, const char *src)
{
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

static void strcat(char *dest, const char *src)
{
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

/* =========================================================
   ATA DRIVER
   ========================================================= */

static void ata_wait_bsy(void) { while (inb(0x1F7) & 0x80); }
static void ata_wait_drq(void) { while (!(inb(0x1F7) & 0x08)); }

static int ata_read_sector(uint32_t lba, uint8_t *buffer)
{
    ata_wait_bsy();
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F7, 0x20);

    ata_wait_bsy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(0x1F0);
        buffer[i * 2] = data & 0xFF;
        buffer[i * 2 + 1] = data >> 8;
    }
    return 1;
}

static int ata_write_sector(uint32_t lba, const uint8_t *buffer)
{
    ata_wait_bsy();
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F7, 0x30);

    ata_wait_bsy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | ((uint16_t)buffer[i * 2 + 1] << 8);
        outw(0x1F0, data);
    }

    outb(0x1F7, 0xE7);
    ata_wait_bsy();
    return 1;
}

/* =========================================================
   KEYBOARD DRIVER
   ========================================================= */

static const char keyboard_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char keyboard_shift_map[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void read_line_input(char *buffer, uint32_t max_len, int mask_input)
{
    uint32_t len = 0;
    int shift = 0;
    for (uint32_t i = 0; i < max_len; i++) buffer[i] = 0;

    while (1) {
        uint8_t status = inb(0x64);
        if (!(status & 1)) continue;

        uint8_t scancode = inb(0x60);

        if (scancode == 0x2A || scancode == 0x36) { shift = 1; continue; }
        if (scancode == 0xAA || scancode == 0xB6) { shift = 0; continue; }
        if (scancode & 0x80 || scancode >= 128) continue;

        char c = shift ? keyboard_shift_map[scancode] : keyboard_map[scancode];

        if (c == '\b') {
            if (len > 0) {
                len--;
                buffer[len] = '\0';
                if (cursor_col > 0) {
                    cursor_col--;
                    VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = ((uint16_t)0x07 << 8) | ' ';
                    update_vga_cursor(cursor_row, cursor_col);
                }
            }
            continue;
        }

        if (c == '\n') {
            putchar('\n');
            buffer[len] = '\0';
            return;
        }

        if (c != 0 && len < max_len - 1) {
            buffer[len++] = c;
            if (mask_input) {
                putchar('*');
            } else {
                putchar(c);
            }
        }
    }
}

/* =========================================================
   SLICEFS & PATH RESOLUTION
   ========================================================= */

#define FS_MAGIC 0x4E49434F
#define FS_VERSION 5
#define FS_SUPERBLOCK_SECTOR 1
#define FS_MAX_FILES 64
#define FS_NAME_LENGTH 64
#define FS_CONTENT_LENGTH 512
#define FS_TABLE_START 2
#define FS_TABLE_SECTORS 71
#define FS_DATA_START (FS_TABLE_START + FS_TABLE_SECTORS)

#define FS_TYPE_FREE 0
#define FS_TYPE_FILE 1
#define FS_TYPE_DIR  2

#define MAX_USERS 8
#define USER_NAME_LEN 16
#define USER_PASS_LEN 16

typedef struct {
    uint8_t used;
    char username[USER_NAME_LEN];
    char password[USER_PASS_LEN];
} User;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_sectors;
    uint32_t max_files;
    uint32_t data_start;
    uint32_t user_count;
    User users[MAX_USERS];
} Superblock;

typedef struct {
    uint8_t used;
    uint8_t type;
    uint16_t reserved;
    char path[FS_NAME_LENGTH];
    char parent_path[FS_NAME_LENGTH];
    uint32_t size;
    uint32_t start_sector;
    char content[FS_CONTENT_LENGTH];
} FSFile;

static Superblock superblock;
static FSFile file_table[FS_MAX_FILES];
static char current_directory[FS_NAME_LENGTH] = "/";
static char current_user[USER_NAME_LEN] = "root";

static void fs_save_superblock(void)
{
    uint8_t buffer[512] = {0};
    uint8_t *raw = (uint8_t*)&superblock;
    for (uint32_t i = 0; i < sizeof(Superblock) && i < 512; i++) buffer[i] = raw[i];
    ata_write_sector(FS_SUPERBLOCK_SECTOR, buffer);
}

static void fs_load_superblock(void)
{
    uint8_t buffer[512];
    ata_read_sector(FS_SUPERBLOCK_SECTOR, buffer);
    uint8_t *raw = (uint8_t*)&superblock;
    for (uint32_t i = 0; i < sizeof(Superblock) && i < 512; i++) raw[i] = buffer[i];
}

static void fs_save_table(void)
{
    uint8_t *raw = (uint8_t*)file_table;
    uint32_t total_bytes = sizeof(file_table);
    uint32_t sectors = (total_bytes + 511) / 512;

    for (uint32_t sector = 0; sector < sectors; sector++) {
        uint8_t buffer[512] = {0};
        uint32_t offset = sector * 512;
        for (uint32_t i = 0; i < 512; i++) {
            if (offset + i < total_bytes) buffer[i] = raw[offset + i];
        }
        ata_write_sector(FS_TABLE_START + sector, buffer);
    }
}

static void fs_load_table(void)
{
    uint8_t *raw = (uint8_t*)file_table;
    uint32_t total_bytes = sizeof(file_table);
    uint32_t sectors = (total_bytes + 511) / 512;

    for (uint32_t sector = 0; sector < sectors; sector++) {
        uint8_t buffer[512];
        ata_read_sector(FS_TABLE_START + sector, buffer);
        uint32_t offset = sector * 512;
        for (uint32_t i = 0; i < 512; i++) {
            if (offset + i < total_bytes) raw[offset + i] = buffer[i];
        }
    }
}

static void build_absolute_path(const char *input, char *out_path)
{
    if (input[0] == '/') {
        strcpy(out_path, input);
    } else {
        strcpy(out_path, current_directory);
        if (strcmp(current_directory, "/") != 0) {
            strcat(out_path, "/");
        }
        strcat(out_path, input);
    }

    uint32_t len = strlen(out_path);
    if (len > 1 && out_path[len - 1] == '/') {
        out_path[len - 1] = '\0';
    }
}

static int fs_find_path(const char *abs_path)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!file_table[i].used) continue;
        if (strcmp(file_table[i].path, abs_path) == 0) return i;
    }
    return -1;
}

static void fs_format(void)
{
    print("\nA formatar SliceFS...\n");
    for (int i = 0; i < FS_MAX_FILES; i++) {
        file_table[i].used = 0;
        file_table[i].type = FS_TYPE_FREE;
        file_table[i].size = 0;
        file_table[i].start_sector = 0;
        for (int j = 0; j < FS_NAME_LENGTH; j++) {
            file_table[i].path[j] = 0;
            file_table[i].parent_path[j] = 0;
        }
        for (int j = 0; j < FS_CONTENT_LENGTH; j++) file_table[i].content[j] = 0;
    }

    superblock.magic = FS_MAGIC;
    superblock.version = FS_VERSION;
    superblock.total_sectors = 32768;
    superblock.max_files = FS_MAX_FILES;
    superblock.data_start = FS_DATA_START;
    superblock.user_count = 1;

    for (int i = 0; i < MAX_USERS; i++) superblock.users[i].used = 0;

    superblock.users[0].used = 1;
    strcpy(superblock.users[0].username, "root");
    strcpy(superblock.users[0].password, "root");

    file_table[0].used = 1;
    file_table[0].type = FS_TYPE_DIR;
    strcpy(file_table[0].path, "/");
    strcpy(file_table[0].parent_path, "");

    file_table[1].used = 1;
    file_table[1].type = FS_TYPE_DIR;
    strcpy(file_table[1].path, "/home");
    strcpy(file_table[1].parent_path, "/");

    fs_save_superblock();
    fs_save_table();
    strcpy(current_directory, "/");
    strcpy(current_user, "root");
    print("SliceFS formatado com sucesso.\n");
}

static void fs_init(void)
{
    fs_load_superblock();
    if (superblock.magic != FS_MAGIC || superblock.version != FS_VERSION) {
        fs_format();
        return;
    }
    fs_load_table();
    strcpy(current_directory, "/");
    strcpy(current_user, "root");
    print("SliceFS carregado.\n");
}

static void fs_mkdir(const char *name)
{
    if (!name || name[0] == '\0') return;

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(name, target_path);

    if (fs_find_path(target_path) != -1) {
        print("Erro: O diretorio ja existe.\n");
        return;
    }

    for (int i = 1; i < FS_MAX_FILES; i++) {
        if (!file_table[i].used) {
            file_table[i].used = 1;
            file_table[i].type = FS_TYPE_DIR;
            strcpy(file_table[i].path, target_path);
            strcpy(file_table[i].parent_path, current_directory);
            file_table[i].size = 0;
            fs_save_table();
            print("Diretorio criado: ");
            print(target_path);
            print("\n");
            return;
        }
    }
    print("Erro: Sem espaco no FS.\n");
}

static void fs_ls(void)
{
    print("\nConteudo de ");
    print(current_directory);
    print(":\n");
    int found = 0;

    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!file_table[i].used) continue;

        if (strcmp(file_table[i].parent_path, current_directory) == 0) {
            char *filename = file_table[i].path;
            uint32_t len = strlen(filename);
            for (int k = len - 1; k >= 0; k--) {
                if (filename[k] == '/') {
                    filename = &filename[k + 1];
                    break;
                }
            }

            print("  ");
            print(filename);

            if (file_table[i].type == FS_TYPE_DIR) {
                print("  <DIR>\n");
            } else {
                print("  ");
                print_int(file_table[i].size);
                print(" bytes\n");
            }
            found = 1;
        }
    }

    if (!found) print("  (diretorio vazio)\n");
}

static void fs_cd(const char *name)
{
    if (!name || name[0] == '\0') {
        print("Uso: cd <diretorio>\n");
        return;
    }

    if (strcmp(name, "/") == 0) {
        strcpy(current_directory, "/");
        return;
    }

    if (strcmp(name, "..") == 0) {
        if (strcmp(current_directory, "/") == 0) return;

        int idx = fs_find_path(current_directory);
        if (idx != -1) {
            strcpy(current_directory, file_table[idx].parent_path);
        } else {
            strcpy(current_directory, "/");
        }
        return;
    }

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(name, target_path);

    int index = fs_find_path(target_path);
    if (index == -1) {
        print("Diretorio nao encontrado.\n");
        return;
    }
    if (file_table[index].type != FS_TYPE_DIR) {
        print("Erro: Nao e um diretorio.\n");
        return;
    }

    strcpy(current_directory, target_path);
}

static void fs_touch(const char *name)
{
    if (!name || name[0] == '\0') return;

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(name, target_path);

    if (fs_find_path(target_path) != -1) {
        print("Erro: Ficheiro ja existe.\n");
        return;
    }

    for (int i = 1; i < FS_MAX_FILES; i++) {
        if (!file_table[i].used) {
            file_table[i].used = 1;
            file_table[i].type = FS_TYPE_FILE;
            strcpy(file_table[i].path, target_path);
            strcpy(file_table[i].parent_path, current_directory);
            file_table[i].size = 0;
            for (int j = 0; j < FS_CONTENT_LENGTH; j++) file_table[i].content[j] = 0;
            fs_save_table();
            print("Ficheiro criado: ");
            print(target_path);
            print("\n");
            return;
        }
    }
}

static void fs_write(const char *name, const char *content)
{
    if (!name || !content) return;

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(name, target_path);

    int index = fs_find_path(target_path);
    if (index == -1) {
        print("Ficheiro nao encontrado.\n");
        return;
    }

    uint32_t length = strlen(content);
    for (uint32_t i = 0; i < FS_CONTENT_LENGTH; i++) file_table[index].content[i] = 0;
    for (uint32_t i = 0; i < length && i < FS_CONTENT_LENGTH - 1; i++) file_table[index].content[i] = content[i];

    file_table[index].size = length;
    fs_save_table();
    print("Ficheiro gravado.\n");
}

static void fs_cat(const char *name)
{
    if (!name) return;

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(name, target_path);

    int index = fs_find_path(target_path);
    if (index == -1 || file_table[index].type != FS_TYPE_FILE) {
        print("Ficheiro nao encontrado.\n");
        return;
    }

    print(file_table[index].content);
    print("\n");
}

static void fs_rm(const char *name)
{
    if (!name) return;

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(name, target_path);

    int index = fs_find_path(target_path);
    if (index <= 0) {
        print("Item invalido ou nao encontrado.\n");
        return;
    }

    file_table[index].used = 0;
    file_table[index].type = FS_TYPE_FREE;
    fs_save_table();
    print("Removido com sucesso.\n");
}

/* =========================================================
   LEAF EDITOR INTEGRADO COM SLICEFS
   ========================================================= */

static void print_string_at(const char* str, int row, int col, uint8_t color) {
    int pos = row * VGA_WIDTH + col;
    for (int i = 0; str[i] != '\0'; i++) {
        VGA_MEMORY[pos + i] = ((uint16_t)color << 8) | str[i];
    }
}

static void redraw_leaf_text(const char* file_buffer, int len) {
    for (int i = 0; i < VGA_WIDTH * 21; i++) {
        VGA_MEMORY[2 * VGA_WIDTH + i] = (0x07 << 8) | ' ';
    }
    for (int i = 0; i < len; i++) {
        int r = 2 + (i / VGA_WIDTH);
        int c = i % VGA_WIDTH;
        if (r <= 22) {
            VGA_MEMORY[r * VGA_WIDTH + c] = (0x07 << 8) | file_buffer[i];
        }
    }
}

void leaf_editor(const char* filename) {
    if (!filename || filename[0] == '\0') {
        print("Uso: leaf <ficheiro>\n");
        return;
    }

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(filename, target_path);

    int file_idx = fs_find_path(target_path);

    if (file_idx == -1) {
        fs_touch(filename);
        file_idx = fs_find_path(target_path);
        if (file_idx == -1) {
            print("Erro ao abrir/criar ficheiro.\n");
            return;
        }
    }

    char file_buffer[MAX_BUFFER];
    int len = file_table[file_idx].size;
    if (len >= MAX_BUFFER) len = MAX_BUFFER - 1;

    for (int i = 0; i < len; i++) {
        file_buffer[i] = file_table[file_idx].content[i];
    }
    file_buffer[len] = '\0';

    int cursor_pos = len;
    int cursor_c = cursor_pos % VGA_WIDTH;
    int cursor_r = 2 + (cursor_pos / VGA_WIDTH);
    int is_extended = 0;
    int ctrl_pressed = 0;

    clear_screen();

    print_string_at(" Leaf Editor  Ficheiro: ", 0, 0, 0x70);
    print_string_at(target_path, 0, 24, 0x70);
    for (int i = 24 + strlen(target_path); i < VGA_WIDTH; i++) {
        VGA_MEMORY[i] = (0x70 << 8) | ' ';
    }

    print_string_at(" Pronto.", 23, 0, 0x07);
    print_string_at("^S Guardar    ^Q Sair", 24, 0, 0x70);
    for (int i = 21; i < VGA_WIDTH; i++) {
        VGA_MEMORY[24 * VGA_WIDTH + i] = (0x70 << 8) | ' ';
    }

    redraw_leaf_text(file_buffer, len);
    update_vga_cursor(cursor_r, cursor_c);

    while (1) {
        if (!(inb(0x64) & 1)) continue;

        uint8_t scancode = inb(0x60);

        if (scancode == 0xE0) {
            is_extended = 1;
            continue;
        }

        if (scancode == 0x1D) { ctrl_pressed = 1; is_extended = 0; continue; }
        if (scancode == 0x9D) { ctrl_pressed = 0; is_extended = 0; continue; }

        if (is_extended) {
            is_extended = 0;
            if (scancode & 0x80) continue;

            if (scancode == 0x48) {
                if (cursor_pos >= VGA_WIDTH) cursor_pos -= VGA_WIDTH;
            } else if (scancode == 0x50) {
                if (cursor_pos + VGA_WIDTH <= len) cursor_pos += VGA_WIDTH;
                else cursor_pos = len;
            } else if (scancode == 0x4B) {
                if (cursor_pos > 0) cursor_pos--;
            } else if (scancode == 0x4D) {
                if (cursor_pos < len) cursor_pos++;
            }

            cursor_c = cursor_pos % VGA_WIDTH;
            cursor_r = 2 + (cursor_pos / VGA_WIDTH);
            if (cursor_r > 22) cursor_r = 22;

            update_vga_cursor(cursor_r, cursor_c);
            continue;
        }

        if (scancode & 0x80) continue;

        if (ctrl_pressed) {
            if (scancode == 0x1F) {
                fs_write(target_path, file_buffer);
                print_string_at(" Ficheiro Guardado!  ", 23, 0, 0x0F);
                continue;
            }
            if (scancode == 0x10) {
                break;
            }
        }

        if (scancode == 0x0E) {
            if (cursor_pos > 0) {
                for (int i = cursor_pos - 1; i < len - 1; i++) {
                    file_buffer[i] = file_buffer[i + 1];
                }
                len--;
                cursor_pos--;
                file_buffer[len] = '\0';
                redraw_leaf_text(file_buffer, len);
                cursor_c = cursor_pos % VGA_WIDTH;
                cursor_r = 2 + (cursor_pos / VGA_WIDTH);
                update_vga_cursor(cursor_r, cursor_c);
            }
            continue;
        }

        if (scancode < 128) {
            char c = keyboard_map[scancode];
            if (c != 0 && len < MAX_BUFFER - 1) {
                for (int i = len; i > cursor_pos; i--) {
                    file_buffer[i] = file_buffer[i - 1];
                }
                file_buffer[cursor_pos] = c;
                len++;
                cursor_pos++;
                file_buffer[len] = '\0';
                redraw_leaf_text(file_buffer, len);
                cursor_c = cursor_pos % VGA_WIDTH;
                cursor_r = 2 + (cursor_pos / VGA_WIDTH);
                update_vga_cursor(cursor_r, cursor_c);
            }
        }
    }

    clear_screen();
}

/* =========================================================
   TINYLEAF EDITOR REAL (INTEGRADO COM SLICEFS + AUTO-SCROLL)
   ========================================================= */

static void redraw_nano_screen(const char* filename, const char* file_buffer, int len, int scroll_offset) {
    // Limpa a tela inteira com fundo padrão
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = (0x07 << 8) | ' ';
        }
    }

    // 1. Barra Superior (Inverse video: Cyan background, text -> 0x1F)
    char header[VGA_WIDTH + 1];
    for(int i = 0; i < VGA_WIDTH; i++) header[i] = ' ';
    header[VGA_WIDTH] = '\0';
    
    const char *title = "   tinyLeaf 1.0                               File: ";
    int idx = 0;
    while(title[idx] && idx < VGA_WIDTH) { header[idx] = title[idx]; idx++; }
    int fidx = 0;
    while(filename[fidx] && idx < VGA_WIDTH - 1) { header[idx++] = filename[fidx++]; }
    
    print_string_at(header, 0, 0, 0x1F);

    // 2. Área de Edição (Linhas 1 a 21)
    int current_row = 0;
    int current_col = 0;
    int line_start_idx = 0;

    for (int i = 0; i <= len; i++) {
        if (i == len || file_buffer[i] == '\n' || current_col >= VGA_WIDTH) {
            if (current_row >= scroll_offset && current_row < scroll_offset + 21) {
                int screen_r = 1 + (current_row - scroll_offset);
                for (int c = 0; c < current_col && c < VGA_WIDTH; c++) {
                    char ch = file_buffer[line_start_idx + c];
                    if (ch != '\n') {
                        VGA_MEMORY[screen_r * VGA_WIDTH + c] = (0x07 << 8) | ch;
                    }
                }
            }
            current_row++;
            if (i < len && file_buffer[i] == '\n') {
                line_start_idx = i + 1;
            } else {
                line_start_idx = i;
            }
            current_col = 0;
            if (i == len) break;
        }
        if (file_buffer[i] != '\n') {
            current_col++;
        }
    }

    // 3. Barras Inferiores de Atalhos
    print_string_at("^G Get Help  ^O WriteOut  ^R Read File ^Y Prev Page ^K Cut Text  ^C Cur Pos", 23, 0, 0x70);
    print_string_at("^X Exit      ^J Justify   ^W Where Is  ^V Next Page ^U UnCut Text^T To Spell", 24, 0, 0x70);
}

void nano_editor(const char* filename) {
    if (!filename || filename[0] == '\0') {
        print("Uso: tinyleaf <ficheiro>\n");
        return;
    }

    char target_path[FS_NAME_LENGTH];
    build_absolute_path(filename, target_path);

    int file_idx = fs_find_path(target_path);
    if (file_idx == -1) {
        fs_touch(filename);
        file_idx = fs_find_path(target_path);
        if (file_idx == -1) {
            print("Erro ao abrir/criar ficheiro.\n");
            return;
        }
    }

    char file_buffer[MAX_BUFFER];
    int len = file_table[file_idx].size;
    if (len >= MAX_BUFFER) len = MAX_BUFFER - 1;

    for (int i = 0; i < len; i++) {
        file_buffer[i] = file_table[file_idx].content[i];
    }
    file_buffer[len] = '\0';

    int cursor_pos = 0;
    int scroll_offset = 0;
    int is_extended = 0;
    int ctrl_pressed = 0;

    clear_screen();
    redraw_nano_screen(target_path, file_buffer, len, scroll_offset);
    update_vga_cursor(1, 0);

    while (1) {
        if (!(inb(0x64) & 1)) continue;
        uint8_t scancode = inb(0x60);

        if (scancode == 0xE0) {
            is_extended = 1;
            continue;
        }

        if (scancode == 0x1D) { ctrl_pressed = 1; is_extended = 0; continue; }
        if (scancode == 0x9D) { ctrl_pressed = 0; is_extended = 0; continue; }

        if (is_extended) {
            is_extended = 0;
            if (!(scancode & 0x80)) {
                if (scancode == 0x4B) { // Esquerda
                    if (cursor_pos > 0) cursor_pos--;
                } else if (scancode == 0x4D) { // Direita
                    if (cursor_pos < len) cursor_pos++;
                } else if (scancode == 0x48 || scancode == 0x50) { // Cima ou Baixo
                    int cur_vrow = 0;
                    int cur_vcol = 0;
                    int temp_row = 0;
                    int temp_col = 0;
                    
                    for (int i = 0; i < cursor_pos; i++) {
                        if (file_buffer[i] == '\n') {
                            temp_row++;
                            temp_col = 0;
                        } else {
                            temp_col++;
                            if (temp_col >= VGA_WIDTH) {
                                temp_col = 0;
                                temp_row++;
                            }
                        }
                    }
                    cur_vrow = temp_row;
                    cur_vcol = temp_col;

                    int target_vrow = cur_vrow;
                    if (scancode == 0x48) {
                        if (target_vrow > 0) target_vrow--;
                    } else {
                        target_vrow++;
                    }

                    int r = 0;
                    int c = 0;
                    int best_pos = len;
                    int found = 0;

                    for (int i = 0; i <= len; i++) {
                        if (r == target_vrow && c == cur_vcol) {
                            best_pos = i;
                            found = 1;
                            break;
                        }
                        if (i == len) {
                            best_pos = i;
                            break;
                        }

                        if (file_buffer[i] == '\n') {
                            if (r == target_vrow) {
                                best_pos = i;
                                found = 1;
                                break;
                            }
                            r++;
                            c = 0;
                        } else {
                            c++;
                            if (c >= VGA_WIDTH) {
                                if (r == target_vrow) {
                                    best_pos = i;
                                    found = 1;
                                    break;
                                }
                                c = 0;
                                r++;
                            }
                        }
                    }
                    if (!found && r == target_vrow) {
                        best_pos = len;
                    }
                    cursor_pos = best_pos;
                }
            }
        }
        else if (scancode & 0x80) {
            continue;
        }

        // Atalhos de Controle
        else if (ctrl_pressed) {
            if (scancode == 0x2D) { // Ctrl + X -> Sair
                break;
            }
            else if (scancode == 0x18) { // Ctrl + O -> Gravar
                fs_write(target_path, file_buffer);
                print_string_at("[ Wrote file saved successfully ]", 23, 0, 0x3F);
                continue;
            }
            else if (scancode == 0x22) { // Ctrl + G -> Get Help
                print_string_at("[ Help: Use Ctrl+X to exit, Ctrl+O to save ]", 23, 0, 0x3F);
                continue;
            }
            else if (scancode == 0x13) { // Ctrl + R
                continue;
            }
            else if (scancode == 0x15) { // Ctrl + Y -> Prev Page
                if (scroll_offset > 0) scroll_offset -= 5;
                continue;
            }
            else if (scancode == 0x2F) { // Ctrl + V -> Next Page
                scroll_offset += 5;
                continue;
            }
            else if (scancode == 0x11) { // Ctrl + W
                continue;
            }
            else if (scancode == 0x25) { // Ctrl + K
                continue;
            }
            else if (scancode == 0x2E) { // Ctrl + C
                continue;
            }
            else if (scancode == 0x24) { // Ctrl + J
                continue;
            }
            else if (scancode == 0x16) { // Ctrl + U
                continue;
            }
            else if (scancode == 0x14) { // Ctrl + T
                continue;
            }
        }

        // Backspace
        else if (scancode == 0x0E) {
            if (cursor_pos > 0) {
                for (int i = cursor_pos - 1; i < len - 1; i++) {
                    file_buffer[i] = file_buffer[i + 1];
                }
                len--;
                cursor_pos--;
                file_buffer[len] = '\0';
            }
        }
        // Tecla Enter
        else if (scancode == 0x1C) {
            if (len < MAX_BUFFER - 1) {
                for (int i = len; i > cursor_pos; i--) {
                    file_buffer[i] = file_buffer[i - 1];
                }
                file_buffer[cursor_pos] = '\n';
                len++;
                cursor_pos++;
                file_buffer[len] = '\0';
            }
        }
        // Digitação normal
        else if (scancode < 128) {
            char c = keyboard_map[scancode];
            if (c != 0 && len < MAX_BUFFER - 1) {
                for (int i = len; i > cursor_pos; i--) {
                    file_buffer[i] = file_buffer[i - 1];
                }
                file_buffer[cursor_pos] = c;
                len++;
                cursor_pos++;
                file_buffer[len] = '\0';
            }
        }

        // CÁLCULO DA LINHA VISUAL E AUTO-SCROLL
        int absolute_vrow = 0;
        int c_vis = 0;
        for(int i = 0; i < cursor_pos; i++) {
            if(file_buffer[i] == '\n') {
                absolute_vrow++;
                c_vis = 0;
            } else {
                c_vis++;
                if(c_vis >= VGA_WIDTH) {
                    c_vis = 0;
                    absolute_vrow++;
                }
            }
        }

        if (absolute_vrow < scroll_offset) {
            scroll_offset = absolute_vrow;
        } else if (absolute_vrow >= scroll_offset + 21) {
            scroll_offset = absolute_vrow - 20;
        }

        redraw_nano_screen(target_path, file_buffer, len, scroll_offset);
        
        int r_vis = 1 + (absolute_vrow - scroll_offset);
        if (r_vis > 21) r_vis = 21;
        if (r_vis < 1) r_vis = 1;
        update_vga_cursor(r_vis, c_vis);
    }
    clear_screen();
}
 
/* =========================================================
   USER MANAGEMENT
   ========================================================= */

static int find_user(const char *username)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (superblock.users[i].used && strcmp(superblock.users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

static void useradd(void)
{
    if (strcmp(current_user, "root") != 0) {
        print("Apenas o root pode adicionar utilizadores.\n");
        return;
    }

    char username[USER_NAME_LEN];
    char password[USER_PASS_LEN];

    print("\nNome do novo utilizador: ");
    read_line_input(username, USER_NAME_LEN, 0);

    if (strlen(username) == 0 || find_user(username) != -1) {
        print("Nome invalido ou ja existente.\n");
        return;
    }

    print("Palavra-passe: ");
    read_line_input(password, USER_PASS_LEN, 1);

    for (int i = 0; i < MAX_USERS; i++) {
        if (!superblock.users[i].used) {
            superblock.users[i].used = 1;
            strcpy(superblock.users[i].username, username);
            strcpy(superblock.users[i].password, password);
            superblock.user_count++;
            fs_save_superblock();

            char user_home[FS_NAME_LENGTH] = "/home/";
            strcat(user_home, username);
            
            char old_dir[FS_NAME_LENGTH];
            strcpy(old_dir, current_directory);
            strcpy(current_directory, "/home");
            fs_mkdir(username);
            strcpy(current_directory, old_dir);

            print("Utilizador '");
            print(username);
            print("' criado em /home/");
            print(username);
            print("!\n");
            return;
        }
    }
}

static void login_user(const char *target_user)
{
    if (strcmp(target_user, "/") == 0) {
        strcpy(current_user, "root");
        strcpy(current_directory, "/");
        print("Sessao alterada para root.\n");
        return;
    }

    int idx = find_user(target_user);
    if (idx == -1) {
        print("Utilizador nao encontrado.\n");
        return;
    }

    if (strcmp(current_user, "root") != 0) {
        char pass[USER_PASS_LEN];
        print("Palavra-passe para ");
        print(target_user);
        print(": ");
        read_line_input(pass, USER_PASS_LEN, 1);

        if (strcmp(pass, superblock.users[idx].password) != 0) {
            print("Palavra-passe incorreta.\n");
            return;
        }
    }

    strcpy(current_user, target_user);
    if (strcmp(target_user, "root") == 0) {
        strcpy(current_directory, "/");
    } else {
        strcpy(current_directory, "/home/");
        strcat(current_directory, target_user);
    }

    print("Sessao iniciada como ");
    print(current_user);
    print("\n");
}

static void boot_login_prompt(void)
{
    if (superblock.user_count <= 1) return;

    print("\n--- LOGIN DE ARRANQUE ---\n");
    while (1) {
        char username[USER_NAME_LEN];
        char password[USER_PASS_LEN];

        print("Utilizador: ");
        read_line_input(username, USER_NAME_LEN, 0);

        int idx = find_user(username);
        if (idx != -1) {
            print("Palavra-passe: ");
            read_line_input(password, USER_PASS_LEN, 1);

            if (strcmp(password, superblock.users[idx].password) == 0) {
                strcpy(current_user, username);
                if (strcmp(username, "root") == 0) {
                    strcpy(current_directory, "/");
                } else {
                    strcpy(current_directory, "/home/");
                    strcat(current_directory, username);
                }
                print("Bem-vindo, ");
                print(current_user);
                print("!\n\n");
                return;
            }
        }
        print("Login invalido. Tente novamente.\n\n");
    }
}

static void shutdown_system(void)
{
    print("\nA desligar OrangeOS...\n");
    fs_save_superblock();
    fs_save_table();
    outw(0x604, 0x2000);
    while (1) { __asm__ volatile("cli; hlt"); }
}

static void neofetch(void)
{
    print("\n");
    print("⠀⠀⠀⠀⣀⣀⣀⣀⣀⠀⠀⠀⢀⣠⡴⢶⡾⣆⠀⠀     \n");
    print("⠀⠐⣾⠛⠋⠉⠛⠋⠙⢻⣦⣠⡟⠁⠀⢸⢠⡿⠀⠀ \n");
    print("⠀⠀⠹⣦⡀⠀⠀⠀⠀⠀⢹⣿⣓⣠⣴⣵⠟⠁⠀⠀ \n");
    print("⠀⠀⠀⠘⠻⣦⣤⣤⣤⣤⣼⠾⠻⠿⠿⣷⣤⡀⠀⠀⠀⠀\n");
    print("⠀⠀⠀⣠⡾⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠉⠪⣽⢷⣄⠀\n");
    print("⠀⢠⡾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢣⡙⢷⡄\n");
    print("⢠⡟⠁⢰⣰⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢱⡈⢿⡄ \n");
    print(" ⣾⠃⠀⠀⠀⣀⡢⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⠘⣿ \n");
    print("⣿⠀⠸⠜⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀⢿⡶\n");
    print("⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀⣿\n");
    print("⢿⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡄⢠⡿\n");
    print("⠈⢿⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⢁⣾⠃\n");
    print("⠀⠈⢻⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀ ⡀⣰⡟⠁\n");
    print("⠀⠀⠀⠙⠷⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣐⣥⠟⠋⠀\n");
    print("⠀⠀⠀⠀⠀⠈⠉⠛⠶⠶⠦⠤⠶⣶⠾⠛⠋⠁\n\n");
    print("OS: OrangeOS v0.4.1 (com GNU Nano integrado)\n");
    print("User: ");
    print(current_user);
    print("\nArch: i386 (x86 32-bit)\n");
    print("FS: SliceFS (Hierarchical Paths)\n");
    print("Terminal: VGA Text Mode (80x25)\n\n");
}

/* =========================================================
   SHELL INTERPRETER
   ========================================================= */

static void shell_prompt(void)
{
    if (strcmp(current_user, "root") == 0) {
        print(current_directory);
        print(" > ");
    } else {
        print(current_user);
        print("@orangeos:");

        char user_home[FS_NAME_LENGTH] = "/home/";
        strcat(user_home, current_user);

        if (strncmp(current_directory, user_home, strlen(user_home)) == 0) {
            print("~");
            print(&current_directory[strlen(user_home)]);
        } else {
            print(current_directory);
        }
        print("$ ");
    }
}

static void shell_execute(char *input)
{
    print("\n");

    if (strcmp(input, "help") == 0) {
        print("Comandos do OrangeOS:\n");
        print("  help, clear, neofetch, uname, shutdown\n");
        print("  useradd, user, user <nome>, user /\n");
        print("  ls, pwd, cd <dir>, mkdir <dir>\n");
        print("  touch <file>, write <file> <texto>, cat <file>, rm <item>\n");
        print("  leaf <file> (Editor Leaf)\n");
        print("  nano <file> (Editor GNU Nano Real Integrado)\n");
        print("  format\n");
    }
    else if (strcmp(input, "clear") == 0) {
        clear_screen();
    }
    else if (strcmp(input, "uname") == 0) {
        print("OrangeOS Kernel v0.4.1 i386\n");
    }
    else if (strcmp(input, "useradd") == 0) {
        useradd();
    }
    else if (strcmp(input, "user") == 0) {
        print("Utilizador atual: ");
        print(current_user);
        print("\n");
    }
    else if (strncmp(input, "user ", 5) == 0) {
        login_user(&input[5]);
    }
    else if (strcmp(input, "ls") == 0) {
        fs_ls();
    }
    else if (strcmp(input, "pwd") == 0) {
        print(current_directory);
        print("\n");
    }
    else if (strncmp(input, "cd ", 3) == 0) {
        fs_cd(&input[3]);
    }
    else if (strncmp(input, "mkdir ", 6) == 0) {
        fs_mkdir(&input[6]);
    }
    else if (strncmp(input, "touch ", 6) == 0) {
        fs_touch(&input[6]);
    }
    else if (strncmp(input, "leaf ", 5) == 0) {
        leaf_editor(&input[5]);
    }
    else if (strncmp(input, "nano ", 5) == 0) {
        nano_editor(&input[5]);
    }
    else if (strncmp(input, "write ", 6) == 0) {
        char *args = &input[6];
        char filename[FS_NAME_LENGTH] = {0};
        char content[FS_CONTENT_LENGTH] = {0};

        uint32_t i = 0;
        while (args[i] != ' ' && args[i] != '\0' && i < FS_NAME_LENGTH - 1) {
            filename[i] = args[i];
            i++;
        }

        if (args[i] == ' ') {
            i++;
            uint32_t j = 0;
            while (args[i] != '\0' && j < FS_CONTENT_LENGTH - 1) {
                content[j++] = args[i++];
            }
            fs_write(filename, content);
        } else {
            print("Uso: write <ficheiro> <texto>\n");
        }
    }
    else if (strncmp(input, "cat ", 4) == 0) {
        fs_cat(&input[4]);
    }
    else if (strncmp(input, "rm ", 3) == 0) {
        fs_rm(&input[3]);
    }
    else if (strcmp(input, "format") == 0) {
        fs_format();
    }
    else if (strcmp(input, "neofetch") == 0) {
        neofetch();
    }
    else if (strncmp(input, "echo ", 5) == 0) {
        print(&input[5]);
        print("\n");
    }
    else if (strcmp(input, "shutdown") == 0 || strcmp(input, "bye") == 0) {
        shutdown_system();
    }
    else if (strlen(input) > 0) {
        print("Comando desconhecido: ");
        print(input);
        print("\n");
    }

    shell_prompt();
}

/* =========================================================
   KERNEL ENTRYPOINT
   ========================================================= */

void kernel_main(void)
{
    clear_screen();
    print("A iniciar OrangeOS Kernel v0.4.1...\n");
    print("A inicializar controlador ATA PIO...\n");

    uint8_t test_buffer[512];
    ata_read_sector(0, test_buffer);

    print("ATA PIO carregado.\n");
    print("A carregar SliceFS...\n");
    fs_init();

    print("\n========================================\n");
    print("          ORANGE OS v0.4.1              \n");
    print("========================================\n\n");

    boot_login_prompt();
    shell_prompt();

    char command_buffer[128];

    while (1) {
        read_line_input(command_buffer, 128, 0);
        shell_execute(command_buffer);
    }
}
