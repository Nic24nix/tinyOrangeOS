#include <stdint.h>

/* =========================================================
   ORANGEOS v0.4.0
   Kernel + SliceFS + Leaf
   ========================================================= */

/* =========================================================
   VGA
   ========================================================= */

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint8_t *)0xB8000)

static volatile uint8_t *vga = VGA_MEMORY;

static int cursor_row = 0;
static int cursor_col = 0;

static int cursor_row_hw = 0;
static int cursor_col_hw = 0;

static uint8_t current_color = 0x07;

/* =========================================================
   PORT I/O
   ========================================================= */

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile ("inb %1, %0"
                      : "=a"(value)
                      : "Nd"(port));

    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t value;

    __asm__ volatile ("inw %1, %0"
                      : "=a"(value)
                      : "Nd"(port));

    return value;
}

/* =========================================================
   BASIC MEMORY FUNCTIONS
   ========================================================= */

static void kmemset(void *ptr, uint8_t value, uint32_t size)
{
    uint8_t *p = (uint8_t *)ptr;
    uint32_t i;

    for (i = 0; i < size; i++)
        p[i] = value;
}

static void kmemcpy(void *dst, const void *src, uint32_t size)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    uint32_t i;

    for (i = 0; i < size; i++)
        d[i] = s[i];
}

/* =========================================================
   STRING FUNCTIONS
   ========================================================= */

static int kstrlen(const char *str)
{
    int len = 0;

    if (!str)
        return 0;

    while (str[len] != '\0')
        len++;

    return len;
}

static int kstrcmp(const char *a, const char *b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];

        i++;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];
}

static int kstrncmp(const char *a, const char *b, int n)
{
    int i;

    for (i = 0; i < n; i++)
    {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];

        if (a[i] == '\0')
            return 0;
    }

    return 0;
}

static void kstrcpy(char *dst, const char *src)
{
    int i = 0;

    while (src[i] != '\0')
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void kstrcat(char *dst, const char *src)
{
    int pos = kstrlen(dst);
    int i = 0;

    while (src[i] != '\0')
    {
        dst[pos++] = src[i++];
    }

    dst[pos] = '\0';
}

/* =========================================================
   BOUNDED STRINGS
   ========================================================= */

static int kcopy_bounded(char *dst, const char *src, int max)
{
    int len;

    if (!dst || !src || max <= 0)
        return 0;

    len = kstrlen(src);

    if (len >= max)
    {
        dst[0] = '\0';
        return 0;
    }

    kstrcpy(dst, src);
    return 1;
}

/* =========================================================
   VGA CURSOR
   ========================================================= */

static void update_cursor(void)
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

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));

    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
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

static void putchar_kernel(char c)
{
    int pos;

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

static char braille_to_ascii(uint8_t dots)
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

static void print(const char *str)
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

static void print_int(uint32_t value)
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

static void clear_screen(void)
{
    int row;
    int col;

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

/* =========================================================
   ATA PIO
   ========================================================= */

#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_CONTROL     0x3F6

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_FLUSH   0xE7

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DF   0x20

#define ATA_TIMEOUT     1000000

static int disk_ready = 0;

static void ata_delay(void)
{
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
}

static int ata_wait_not_busy(void)
{
    uint32_t timeout = ATA_TIMEOUT;

    while (timeout--)
    {
        uint8_t status = inb(ATA_STATUS);

        if (!(status & ATA_STATUS_BSY))
        {
            if (status & (ATA_STATUS_ERR | ATA_STATUS_DF))
                return 0;

            return 1;
        }
    }

    return 0;
}

static int ata_wait_drq(void)
{
    uint32_t timeout = ATA_TIMEOUT;

    while (timeout--)
    {
        uint8_t status = inb(ATA_STATUS);

        if (status & ATA_STATUS_ERR)
            return 0;

        if (status & ATA_STATUS_DF)
            return 0;

        if (status & ATA_STATUS_DRQ)
            return 1;
    }

    return 0;
}

static int ata_read_sector(uint32_t lba, uint8_t *buffer)
{
    int i;

    if (!ata_wait_not_busy())
        return 0;

    outb(ATA_DRIVE,
         0xE0 | ((lba >> 24) & 0x0F));

    ata_delay();

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

    outb(ATA_COMMAND, ATA_CMD_READ);

    if (!ata_wait_drq())
        return 0;

    for (i = 0; i < 256; i++)
    {
        uint16_t value = inw(ATA_DATA);

        buffer[i * 2]     = (uint8_t)(value & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(value >> 8);
    }

    return 1;
}

static int ata_write_sector(uint32_t lba, const uint8_t *buffer)
{
    int i;

    if (!ata_wait_not_busy())
        return 0;

    outb(ATA_DRIVE,
         0xE0 | ((lba >> 24) & 0x0F));

    ata_delay();

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (!ata_wait_drq())
        return 0;

    for (i = 0; i < 256; i++)
    {
        uint16_t value =
            (uint16_t)buffer[i * 2] |
            ((uint16_t)buffer[i * 2 + 1] << 8);

        outw(ATA_DATA, value);
    }

    outb(ATA_COMMAND, ATA_CMD_FLUSH);

    return ata_wait_not_busy();
}

/* =========================================================
   SLICEFS
   ========================================================= */

#define FS_MAGIC        0x4E49434F
#define FS_VERSION      6

#define FS_SUPERBLOCK   1
#define FS_TABLE_START  2

#define FS_MAX_FILES    64
#define FS_NAME_LENGTH  64
#define FS_CONTENT_LENGTH 512

#define FS_TYPE_FREE    0
#define FS_TYPE_FILE    1
#define FS_TYPE_DIR     2

#define MAX_USERS       8
#define USERNAME_LENGTH 16
#define PASSWORD_LENGTH 16

typedef struct
{
    uint8_t used;
    char username[USERNAME_LENGTH];
    char password[PASSWORD_LENGTH];
} User;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t total_sectors;
    uint32_t max_files;
    uint32_t table_start;
    uint32_t data_start;

    uint32_t user_count;
    User users[MAX_USERS];
} Superblock;

typedef struct
{
    uint8_t type;
    uint8_t reserved;
    uint16_t permissions;

    char name[FS_NAME_LENGTH];
    char parent[FS_NAME_LENGTH];

    uint32_t size;
    uint32_t start_sector;

    char content[FS_CONTENT_LENGTH];
} FSFile;

static FSFile file_table[FS_MAX_FILES];
static Superblock superblock;

static char current_directory[FS_NAME_LENGTH] = "/";
static char current_user[USERNAME_LENGTH] = "root";

enum
{
    FS_TABLE_SECTORS =
        (sizeof(FSFile) * FS_MAX_FILES + 511) / 512
};

#define FS_DATA_START (FS_TABLE_START + FS_TABLE_SECTORS)
#define FS_TOTAL_SECTORS 32768

static int fs_ready = 0;

/* =========================================================
   SLICEFS HELPERS
   ========================================================= */

static int fs_find(const char *path)
{
    int i;

    for (i = 0; i < FS_MAX_FILES; i++)
    {
        if (file_table[i].type != FS_TYPE_FREE)
        {
            if (kstrcmp(file_table[i].name, path) == 0)
                return i;
        }
    }

    return -1;
}

static int fs_find_free(void)
{
    int i;

    for (i = 0; i < FS_MAX_FILES; i++)
    {
        if (file_table[i].type == FS_TYPE_FREE)
            return i;
    }

    return -1;
}

static int fs_directory_exists(const char *path)
{
    int index = fs_find(path);

    if (index < 0)
        return 0;

    return file_table[index].type == FS_TYPE_DIR;
}

static int build_absolute_path(
    const char *input,
    char *output
)
{
    int len;

    if (!input || !output)
        return 0;

    if (input[0] == '\0')
        return 0;

    if (input[0] == '/')
    {
        return kcopy_bounded(
            output,
            input,
            FS_NAME_LENGTH
        );
    }

    len = kstrlen(current_directory);

    if (len == 1 &&
        current_directory[0] == '/')
    {
        if (1 + kstrlen(input) >= FS_NAME_LENGTH)
            return 0;

        output[0] = '/';
        kstrcpy(output + 1, input);
        return 1;
    }

    if (len + 1 + kstrlen(input) >= FS_NAME_LENGTH)
        return 0;

    kstrcpy(output, current_directory);
    kstrcat(output, "/");
    kstrcat(output, input);

    return 1;
}

static int get_parent_path(
    const char *path,
    char *parent
)
{
    int len;
    int i;

    if (!path || path[0] != '/')
        return 0;

    len = kstrlen(path);

    if (len <= 1)
        return 0;

    i = len - 1;

    while (i > 0 && path[i] != '/')
        i--;

    if (i == 0)
    {
        parent[0] = '/';
        parent[1] = '\0';
        return 1;
    }

    if (i >= FS_NAME_LENGTH)
        return 0;

    for (int j = 0; j < i; j++)
        parent[j] = path[j];

    parent[i] = '\0';

    return 1;
}

static int fs_save_superblock(void)
{
    uint8_t sector[512];

    if (!disk_ready)
        return 0;

    kmemset(sector, 0, 512);

    kmemcpy(
        sector,
        &superblock,
        sizeof(Superblock)
    );

    return ata_write_sector(
        FS_SUPERBLOCK,
        sector
    );
}

static int fs_load_superblock(void)
{
    uint8_t sector[512];

    if (!disk_ready)
        return 0;

    if (!ata_read_sector(
            FS_SUPERBLOCK,
            sector))
        return 0;

    kmemset(
        &superblock,
        0,
        sizeof(Superblock)
    );

    kmemcpy(
        &superblock,
        sector,
        sizeof(Superblock)
    );

    return 1;
}

static int fs_save_table(void)
{
    uint8_t sector[512];
    uint32_t total_bytes =
        sizeof(FSFile) * FS_MAX_FILES;

    uint32_t sector_index;

    if (!disk_ready)
        return 0;

    for (sector_index = 0;
         sector_index < FS_TABLE_SECTORS;
         sector_index++)
    {
        uint32_t offset =
            sector_index * 512;

        uint32_t remaining =
            total_bytes - offset;

        uint32_t copy_size =
            remaining > 512 ? 512 : remaining;

        kmemset(sector, 0, 512);

        kmemcpy(
            sector,
            ((uint8_t *)file_table) + offset,
            copy_size
        );

        if (!ata_write_sector(
                FS_TABLE_START + sector_index,
                sector))
            return 0;
    }

    return 1;
}

static int fs_load_table(void)
{
    uint8_t sector[512];
    uint32_t total_bytes =
        sizeof(FSFile) * FS_MAX_FILES;

    uint32_t sector_index;

    if (!disk_ready)
        return 0;

    kmemset(
        file_table,
        0,
        sizeof(file_table)
    );

    for (sector_index = 0;
         sector_index < FS_TABLE_SECTORS;
         sector_index++)
    {
        uint32_t offset =
            sector_index * 512;

        uint32_t remaining =
            total_bytes - offset;

        uint32_t copy_size =
            remaining > 512 ? 512 : remaining;

        if (!ata_read_sector(
                FS_TABLE_START + sector_index,
                sector))
            return 0;

        kmemcpy(
            ((uint8_t *)file_table) + offset,
            sector,
            copy_size
        );
    }

    return 1;
}

/* =========================================================
   FORMAT
   ========================================================= */

static void fs_create_root_entries(void)
{
    kmemset(
        file_table,
        0,
        sizeof(file_table)
    );

    /*
     * /
     */
    file_table[0].type = FS_TYPE_DIR;
    file_table[0].permissions = 0x755;

    kstrcpy(
        file_table[0].name,
        "/"
    );

    kstrcpy(
        file_table[0].parent,
        ""
    );

    /*
     * /home
     */
    file_table[1].type = FS_TYPE_DIR;
    file_table[1].permissions = 0x755;

    kstrcpy(
        file_table[1].name,
        "/home"
    );

    kstrcpy(
        file_table[1].parent,
        "/"
    );
}

static int fs_format(void)
{
    kmemset(
        &superblock,
        0,
        sizeof(Superblock)
    );

    superblock.magic = FS_MAGIC;
    superblock.version = FS_VERSION;
    superblock.total_sectors = FS_TOTAL_SECTORS;
    superblock.max_files = FS_MAX_FILES;
    superblock.table_start = FS_TABLE_START;
    superblock.data_start = FS_DATA_START;

    superblock.user_count = 1;

    superblock.users[0].used = 1;

    kstrcpy(
        superblock.users[0].username,
        "root"
    );

    kstrcpy(
        superblock.users[0].password,
        "orange"
    );

    kstrcpy(
        current_user,
        "root"
    );

    kstrcpy(
        current_directory,
        "/"
    );

    fs_create_root_entries();

    fs_ready = 1;

    if (!disk_ready)
        return 1;

    if (!fs_save_superblock())
        return 0;

    if (!fs_save_table())
        return 0;

    return 1;
}

/* =========================================================
   FS INIT
   ========================================================= */

static void fs_init(void)
{
    if (!disk_ready)
    {
        print(
            "SliceFS: disco ATA indisponivel.\n"
        );

        fs_format();
        return;
    }

    if (!fs_load_superblock())
    {
        print(
            "SliceFS: superbloco invalido.\n"
        );

        if (!fs_format())
        {
            print(
                "Erro ao formatar SliceFS.\n"
            );
        }

        return;
    }

    if (superblock.magic != FS_MAGIC ||
        superblock.version != FS_VERSION ||
        superblock.max_files != FS_MAX_FILES ||
        superblock.table_start != FS_TABLE_START ||
        superblock.data_start != FS_DATA_START ||
        superblock.user_count > MAX_USERS)
    {
        print(
            "SliceFS: filesystem antigo/invalido.\n"
        );

        print(
            "A criar novo filesystem...\n"
        );

        if (!fs_format())
        {
            print(
                "Erro ao criar filesystem.\n"
            );
        }

        return;
    }

    if (!fs_load_table())
    {
        print(
            "Erro ao carregar tabela SliceFS.\n"
        );

        fs_format();
        return;
    }

    fs_ready = 1;

    print(
        "SliceFS carregado com sucesso.\n"
    );
}

/* =========================================================
   TOUCH
   ========================================================= */

static int fs_touch(const char *input)
{
    char path[FS_NAME_LENGTH];
    char parent[FS_NAME_LENGTH];

    int index;

    if (!build_absolute_path(input, path))
    {
        print("Erro: nome demasiado longo.\n");
        return 0;
    }

    if (kstrcmp(path, "/") == 0)
    {
        print("Erro: nome invalido.\n");
        return 0;
    }

    if (fs_find(path) >= 0)
    {
        return 0;
    }

    if (!get_parent_path(path, parent))
    {
        print("Erro: caminho invalido.\n");
        return 0;
    }

    if (!fs_directory_exists(parent))
    {
        print("Erro: diretorio pai nao existe.\n");
        return 0;
    }

    index = fs_find_free();

    if (index < 0)
    {
        print("Erro: SliceFS cheio.\n");
        return 0;
    }

    kmemset(
        &file_table[index],
        0,
        sizeof(FSFile)
    );

    file_table[index].type = FS_TYPE_FILE;
    file_table[index].permissions = 0x644;

    kstrcpy(
        file_table[index].name,
        path
    );

    kstrcpy(
        file_table[index].parent,
        parent
    );

    file_table[index].size = 0;
    file_table[index].start_sector = FS_DATA_START;

    file_table[index].content[0] = '\0';

    fs_save_table();

    return 1;
}

/* =========================================================
   MKDIR
   ========================================================= */

static int fs_mkdir(const char *input)
{
    char path[FS_NAME_LENGTH];
    char parent[FS_NAME_LENGTH];

    int index;

    if (!build_absolute_path(input, path))
    {
        print("Erro: nome demasiado longo.\n");
        return 0;
    }

    if (kstrcmp(path, "/") == 0)
    {
        print("Erro: diretorio invalido.\n");
        return 0;
    }

    if (fs_find(path) >= 0)
    {
        print("Erro: ja existe.\n");
        return 0;
    }

    if (!get_parent_path(path, parent))
    {
        print("Erro: caminho invalido.\n");
        return 0;
    }

    if (!fs_directory_exists(parent))
    {
        print("Erro: diretorio pai nao existe.\n");
        return 0;
    }

    index = fs_find_free();

    if (index < 0)
    {
        print("Erro: SliceFS cheio.\n");
        return 0;
    }

    kmemset(
        &file_table[index],
        0,
        sizeof(FSFile)
    );

    file_table[index].type = FS_TYPE_DIR;
    file_table[index].permissions = 0x755;

    kstrcpy(
        file_table[index].name,
        path
    );

    kstrcpy(
        file_table[index].parent,
        parent
    );

    fs_save_table();

    return 1;
}

/* =========================================================
   LS
   ========================================================= */

static void fs_ls(void)
{
    int i;
    int found = 0;

    for (i = 0; i < FS_MAX_FILES; i++)
    {
        if (file_table[i].type == FS_TYPE_FREE)
            continue;

        if (kstrcmp(
                file_table[i].parent,
                current_directory) == 0)
        {
            int len =
                kstrlen(file_table[i].name);

            int start = 0;
            int j;

            for (j = 0; j < len; j++)
            {
                if (file_table[i].name[j] == '/')
                    start = j + 1;
            }

            if (file_table[i].type == FS_TYPE_DIR)
            {
                print("[DIR]  ");
            }
            else
            {
                print("[FILE] ");
            }

            print(
                file_table[i].name + start
            );

            print("\n");

            found = 1;
        }
    }

    if (!found)
        print("(vazio)\n");
}

/* =========================================================
   PWD
   ========================================================= */

static void fs_pwd(void)
{
    print(current_directory);
    print("\n");
}

/* =========================================================
   CD
   ========================================================= */

static int fs_cd(const char *input)
{
    char path[FS_NAME_LENGTH];

    if (kstrcmp(input, ".") == 0)
        return 1;

    if (kstrcmp(input, "..") == 0)
    {
        if (kstrcmp(current_directory, "/") == 0)
            return 1;

        if (!get_parent_path(
                current_directory,
                path))
        {
            kstrcpy(
                current_directory,
                "/"
            );
        }
        else
        {
            kstrcpy(
                current_directory,
                path
            );
        }

        return 1;
    }

    if (!build_absolute_path(input, path))
    {
        print("Erro: caminho demasiado longo.\n");
        return 0;
    }

    if (!fs_directory_exists(path))
    {
        print("Erro: diretorio nao encontrado.\n");
        return 0;
    }

    kstrcpy(
        current_directory,
        path
    );

    return 1;
}

/* =========================================================
   WRITE
   ========================================================= */

static int fs_write_absolute(
    const char *path,
    const char *content
)
{
    int index;
    int len;
    int stored;

    index = fs_find(path);

    if (index < 0)
    {
        if (!fs_touch(path))
            return 0;

        index = fs_find(path);

        if (index < 0)
            return 0;
    }

    if (file_table[index].type != FS_TYPE_FILE)
    {
        print("Erro: nao e ficheiro.\n");
        return 0;
    }

    len = kstrlen(content);

    stored = len;

    if (stored >= FS_CONTENT_LENGTH)
        stored = FS_CONTENT_LENGTH - 1;

    kmemset(
        file_table[index].content,
        0,
        FS_CONTENT_LENGTH
    );

    for (int i = 0; i < stored; i++)
        file_table[index].content[i] =
            content[i];

    file_table[index].content[stored] = '\0';
    file_table[index].size = stored;

    fs_save_table();

    if (len >= FS_CONTENT_LENGTH)
    {
        print(
            "Aviso: texto truncado para 511 bytes.\n"
        );
    }

    return 1;
}

static int fs_write(
    const char *input,
    const char *content
)
{
    char path[FS_NAME_LENGTH];

    if (!build_absolute_path(input, path))
    {
        print("Erro: caminho demasiado longo.\n");
        return 0;
    }

    return fs_write_absolute(
        path,
        content
    );
}

/* =========================================================
   CAT
   ========================================================= */

static int fs_cat(const char *input)
{
    char path[FS_NAME_LENGTH];
    int index;

    if (!build_absolute_path(input, path))
    {
        print("Erro: caminho demasiado longo.\n");
        return 0;
    }

    index = fs_find(path);

    if (index < 0)
    {
        print("Erro: ficheiro nao encontrado.\n");
        return 0;
    }

    if (file_table[index].type != FS_TYPE_FILE)
    {
        print("Erro: nao e ficheiro.\n");
        return 0;
    }

    print(
        file_table[index].content
    );

    print("\n");

    return 1;
}

/* =========================================================
   RM
   ========================================================= */

static int fs_rm(const char *input)
{
    char path[FS_NAME_LENGTH];

    int index;
    int i;

    if (!build_absolute_path(input, path))
    {
        print("Erro: caminho demasiado longo.\n");
        return 0;
    }

    if (kstrcmp(path, "/") == 0 ||
        kstrcmp(path, "/home") == 0)
    {
        print("Erro: diretorio protegido.\n");
        return 0;
    }

    index = fs_find(path);

    if (index < 0)
    {
        print("Erro: nao encontrado.\n");
        return 0;
    }

    if (file_table[index].type == FS_TYPE_DIR)
    {
        for (i = 0; i < FS_MAX_FILES; i++)
        {
            if (file_table[i].type != FS_TYPE_FREE &&
                kstrcmp(
                    file_table[i].parent,
                    path
                ) == 0)
            {
                print(
                    "Erro: diretorio nao esta vazio.\n"
                );

                return 0;
            }
        }
    }

    kmemset(
        &file_table[index],
        0,
        sizeof(FSFile)
    );

    fs_save_table();

    return 1;
}

/* =========================================================
   USER MANAGEMENT
   ========================================================= */

static int find_user(const char *username)
{
    int i;

    for (i = 0; i < MAX_USERS; i++)
    {
        if (superblock.users[i].used)
        {
            if (kstrcmp(
                    superblock.users[i].username,
                    username) == 0)
            {
                return i;
            }
        }
    }

    return -1;
}

static int useradd(
    const char *username,
    const char *password
)
{
    int index;
    int i;

    if (kstrcmp(current_user, "root") != 0)
    {
        print(
            "Erro: apenas root pode criar utilizadores.\n"
        );

        return 0;
    }

    if (kstrlen(username) == 0 ||
        kstrlen(username) >= USERNAME_LENGTH)
    {
        print("Erro: username invalido.\n");
        return 0;
    }

    if (kstrlen(password) == 0 ||
        kstrlen(password) >= PASSWORD_LENGTH)
    {
        print("Erro: password invalida.\n");
        return 0;
    }

    if (find_user(username) >= 0)
    {
        print("Erro: utilizador ja existe.\n");
        return 0;
    }

    index = -1;

    for (i = 0; i < MAX_USERS; i++)
    {
        if (!superblock.users[i].used)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
    {
        print("Erro: limite de utilizadores atingido.\n");
        return 0;
    }

    superblock.users[index].used = 1;

    kstrcpy(
        superblock.users[index].username,
        username
    );

    kstrcpy(
        superblock.users[index].password,
        password
    );

    superblock.user_count++;

    fs_save_superblock();

    /*
     * Criar home do utilizador.
     */
    {
        char old_dir[FS_NAME_LENGTH];

        kstrcpy(
            old_dir,
            current_directory
        );

        kstrcpy(
            current_directory,
            "/home"
        );

        fs_mkdir(username);

        kstrcpy(
            current_directory,
            old_dir
        );
    }

    print("Utilizador criado.\n");

    return 1;
}

/* =========================================================
   LOGIN
   ========================================================= */

static int login_user(
    const char *username,
    const char *password
)
{
    int index;

    index = find_user(username);

    if (index < 0)
    {
        print("Erro: utilizador nao existe.\n");
        return 0;
    }

    if (kstrcmp(
            superblock.users[index].password,
            password) != 0)
    {
        print("Erro: password incorreta.\n");
        return 0;
    }

    kstrcpy(
        current_user,
        username
    );

    if (kstrcmp(username, "root") == 0)
    {
        kstrcpy(
            current_directory,
            "/"
        );
    }
    else
    {
        char home[FS_NAME_LENGTH];

        kstrcpy(home, "/home/");
        kstrcat(home, username);

        if (fs_directory_exists(home))
        {
            kstrcpy(
                current_directory,
                home
            );
        }
        else
        {
            kstrcpy(
                current_directory,
                "/home"
            );
        }
    }

    return 1;
}

/* =========================================================
   KEYBOARD PROTOTYPE
   ========================================================= */

static void read_line_input(
    char *buffer,
    int max_len
);

/* =========================================================
   LOGIN AT BOOT
   ========================================================= */

static void boot_login_prompt(void)
{
    char username[USERNAME_LENGTH];
    char password[PASSWORD_LENGTH];

    int attempts;

    /*
     * Se só existir root, entrar diretamente.
     */
    if (superblock.user_count <= 1)
    {
        kstrcpy(
            current_user,
            "root"
        );

        kstrcpy(
            current_directory,
            "/"
        );

        return;
    }

    clear_screen();

    print(
        "========================================\n"
    );

    print(
        "             ORANGEOS LOGIN\n"
    );

    print(
        "========================================\n\n"
    );

    for (attempts = 0; attempts < 3; attempts++)
    {
        print("Username: ");

        read_line_input(
            username,
            USERNAME_LENGTH
        );

        print("Password: ");

        read_line_input(
            password,
            PASSWORD_LENGTH
        );

        if (login_user(
                username,
                password))
        {
            print(
                "\nLogin efetuado!\n"
            );

            return;
        }

        print(
            "\nLogin falhou.\n\n"
        );
    }

    /*
     * Fallback para root.
     */
    kstrcpy(
        current_user,
        "root"
    );

    kstrcpy(
        current_directory,
        "/"
    );
}

/* =========================================================
   KEYBOARD
   ========================================================= */

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

static int shift_pressed = 0;
static int ctrl_pressed = 0;

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

static char keyboard_get_char(
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

static void read_line_input(
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

        if (!(inb(KEYBOARD_STATUS_PORT) & 1))
            continue;

        scancode = inb(KEYBOARD_DATA_PORT);

        /*
         * Extended key prefix.
         */
        if (scancode == 0xE0)
        {
            inb(KEYBOARD_DATA_PORT);
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

/* =========================================================
   LEAF SCREEN OUTPUT
   ========================================================= */

static void print_string_at(
    int row,
    int col,
    const char *str,
    uint8_t color
)
{
    int i = 0;

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

static void leaf_editor(
    const char *filename
)
{
    char buffer[FS_CONTENT_LENGTH];
    char target_path[FS_NAME_LENGTH];

    int length = 0;
    int cursor_pos = 0;
    int running = 1;

    int index;

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

    index = fs_find(target_path);

    if (index >= 0 &&
        file_table[index].type == FS_TYPE_FILE)
    {
        int copy_len =
            file_table[index].size;

        if (copy_len >= FS_CONTENT_LENGTH)
            copy_len = FS_CONTENT_LENGTH - 1;

        for (int i = 0;
             i < copy_len;
             i++)
        {
            buffer[i] =
                file_table[index].content[i];
        }

        buffer[copy_len] = '\0';
        length = copy_len;
    }
    else
    {
        buffer[0] = '\0';
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

        if (!(inb(KEYBOARD_STATUS_PORT) & 1))
            continue;

        scancode =
            inb(KEYBOARD_DATA_PORT);

        /*
         * Extended keyboard.
         */
        if (scancode == 0xE0)
        {
            uint8_t key =
                inb(KEYBOARD_DATA_PORT);

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
static void neofetch_braille_line(
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

static void neofetch(void)
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
        "OrangeOS v0.4.0",
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

/* =========================================================
   SHUTDOWN
   ========================================================= */

static void shutdown_system(void)
{
    fs_save_superblock();
    fs_save_table();

    print(
        "\nA desligar OrangeOS...\n"
    );

    /*
     * QEMU / PIIX4.
     */
    outw(0x604, 0x2000);

    /*
     * Bochs.
     */
    outw(0xB004, 0x2000);

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

/* =========================================================
   SHELL
   ========================================================= */

static void shell_prompt(void)
{
    print("\n");

    print(current_user);
    print("@orangeos:");

    print(current_directory);

    print("$ ");
}

static void shell_execute(char *command)
{
    if (kstrcmp(command, "") == 0)
        return;

    /*
     * help
     */
    if (kstrcmp(command, "help") == 0)
    {
        print(
            "\n"
            "Comandos:\n"
            "  help\n"
            "  clear\n"
            "  neofetch\n"
            "  uname\n"
            "  ls\n"
            "  pwd\n"
            "  cd <dir>\n"
            "  mkdir <dir>\n"
            "  touch <file>\n"
            "  write <file> <texto>\n"
            "  cat <file>\n"
            "  rm <file>\n"
            "  leaf <file>\n"
            "  format\n"
            "  echo <texto>\n"
            "  user\n"
            "  user <nome>\n"
            "  useradd <nome> <password>\n"
            "  shutdown\n"
            "  bye\n"
            "\n"
        );

        return;
    }

    /*
     * clear
     */
    if (kstrcmp(command, "clear") == 0)
    {
        clear_screen();
        return;
    }

    /*
     * neofetch
     */
    if (kstrcmp(command, "neofetch") == 0)
    {
        neofetch();
        return;
    }

    /*
     * uname
     */
    if (kstrcmp(command, "uname") == 0)
    {
        print(
            "OrangeOS i386\n"
        );

        return;
    }

    /*
     * ls
     */
    if (kstrcmp(command, "ls") == 0)
    {
        fs_ls();
        return;
    }

    /*
     * pwd
     */
    if (kstrcmp(command, "pwd") == 0)
    {
        fs_pwd();
        return;
    }

    /*
     * shutdown
     */
    if (kstrcmp(command, "shutdown") == 0 ||
        kstrcmp(command, "bye") == 0)
    {
        shutdown_system();
        return;
    }

    /*
     * format
     */
    if (kstrcmp(command, "format") == 0)
    {
        if (kstrcmp(
                current_user,
                "root") != 0)
        {
            print(
                "Erro: apenas root pode formatar.\n"
            );

            return;
        }

        if (fs_format())
        {
            print(
                "SliceFS formatado com sucesso.\n"
            );
        }
        else
        {
            print(
                "Erro ao formatar SliceFS.\n"
            );
        }

        return;
    }

    /*
     * user
     */
    if (kstrcmp(command, "user") == 0)
    {
        print("Utilizador: ");
        print(current_user);
        print("\nDiretorio: ");
        print(current_directory);
        print("\n");

        return;
    }

    /*
     * cd
     */
    if (kstrncmp(command, "cd ", 3) == 0)
    {
        fs_cd(command + 3);
        return;
    }

    /*
     * mkdir
     */
    if (kstrncmp(command, "mkdir ", 6) == 0)
    {
        if (fs_mkdir(command + 6))
            print("Diretorio criado.\n");

        return;
    }

    /*
     * touch
     */
    if (kstrncmp(command, "touch ", 6) == 0)
    {
        if (fs_touch(command + 6))
            print("Ficheiro criado.\n");

        return;
    }

    /*
     * cat
     */
    if (kstrncmp(command, "cat ", 4) == 0)
    {
        fs_cat(command + 4);
        return;
    }

    /*
     * rm
     */
    if (kstrncmp(command, "rm ", 3) == 0)
    {
        if (fs_rm(command + 3))
            print("Removido.\n");

        return;
    }

    /*
     * leaf
     */
    if (kstrncmp(command, "leaf ", 5) == 0)
    {
        leaf_editor(command + 5);
        return;
    }

    /*
     * write
     */
    if (kstrncmp(command, "write ", 6) == 0)
    {
        char filename[FS_NAME_LENGTH];
        char content[FS_CONTENT_LENGTH];

        int i = 6;
        int j = 0;

        while (
            command[i] != '\0' &&
            command[i] != ' ' &&
            j < FS_NAME_LENGTH - 1)
        {
            filename[j++] =
                command[i++];
        }

        filename[j] = '\0';

        while (command[i] == ' ')
            i++;

        kcopy_bounded(
            content,
            command + i,
            FS_CONTENT_LENGTH
        );

        if (fs_write(
                filename,
                content))
        {
            print("Escrito.\n");
        }

        return;
    }

    /*
     * echo
     */
    if (kstrncmp(command, "echo ", 5) == 0)
    {
        print(command + 5);
        print("\n");
        return;
    }

    /*
     * useradd
     */
    if (kstrncmp(command, "useradd ", 8) == 0)
    {
        char username[USERNAME_LENGTH];
        char password[PASSWORD_LENGTH];

        int i = 8;
        int j = 0;

        while (
            command[i] != '\0' &&
            command[i] != ' ' &&
            j < USERNAME_LENGTH - 1)
        {
            username[j++] =
                command[i++];
        }

        username[j] = '\0';

        while (command[i] == ' ')
            i++;

        j = 0;

        while (
            command[i] != '\0' &&
            command[i] != ' ' &&
            j < PASSWORD_LENGTH - 1)
        {
            password[j++] =
                command[i++];
        }

        password[j] = '\0';

        useradd(
            username,
            password
        );

        return;
    }

    /*
     * user <nome>
     */
    if (kstrncmp(command, "user ", 5) == 0)
    {
        char username[USERNAME_LENGTH];

        kcopy_bounded(
            username,
            command + 5,
            USERNAME_LENGTH
        );

        if (kstrcmp(
                current_user,
                "root") == 0)
        {
            if (login_user(
                    username,
                    ""))
            {
                /*
                 * Root pode trocar sem password.
                 * login_user normalmente exigiria
                 * password, portanto procurar
                 * diretamente.
                 */
            }
            else
            {
                int index =
                    find_user(username);

                if (index >= 0)
                {
                    kstrcpy(
                        current_user,
                        username
                    );

                    if (kstrcmp(
                            username,
                            "root") == 0)
                    {
                        kstrcpy(
                            current_directory,
                            "/"
                        );
                    }
                    else
                    {
                        char home[FS_NAME_LENGTH];

                        kstrcpy(
                            home,
                            "/home/"
                        );

                        kstrcat(
                            home,
                            username
                        );

                        if (fs_directory_exists(home))
                        {
                            kstrcpy(
                                current_directory,
                                home
                            );
                        }
                    }

                    print(
                        "Utilizador alterado.\n"
                    );
                }
                else
                {
                    print(
                        "Utilizador nao encontrado.\n"
                    );
                }
            }
        }
        else
        {
            print(
                "Erro: apenas root pode trocar utilizadores.\n"
            );
        }

        return;
    }

    print("Comando desconhecido. Use 'help'.\n");
}

/* =========================================================
   SHELL
   ========================================================= */

static void shell(void)
{
    char command[128];

    while (1)
    {
        shell_prompt();

        read_line_input(
            command,
            sizeof(command)
        );

        shell_execute(command);
    }
}

/* =========================================================
   KERNEL MAIN
   ========================================================= */

void kernel_main(void)
{
    uint8_t test_sector[512];

    clear_screen();

    print(
        "\n"
        "========================================\n"
        "             ORANGEOS v0.4.0\n"
        "========================================\n\n"
    );

    print(
        "Inicializando hardware...\n"
    );

    /*
     * Teste ATA.
     */
    if (ata_read_sector(
            0,
            test_sector))
    {
        disk_ready = 1;

        print(
            "[ OK ] ATA PIO\n"
        );
    }
    else
    {
        disk_ready = 0;

        print(
            "[ !! ] ATA indisponivel\n"
        );
        print(
            "      filesystem sera apenas memoria.\n"
        );
    }

    /*
     * SliceFS.
     */
    print(
        "[ .. ] Inicializando SliceFS...\n"
    );

    fs_init();

    print(
        "[ OK ] SliceFS\n"
    );

    /*
     * Login.
     */
    boot_login_prompt();

    print(
        "\n"
        "OrangeOS pronto!\n"
    );

    print(
        "Escreve 'help' para ver os comandos.\n"
    );

    /*
     * Shell.
     */
    shell();

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
