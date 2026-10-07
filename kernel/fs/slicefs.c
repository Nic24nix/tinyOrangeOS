#include <stdint.h>
#include "../kernel.h"
#include "slicefs.h"

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

char current_directory[FS_NAME_LENGTH] = "/";
char current_user[USERNAME_LENGTH] = "root";

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

int fs_directory_exists(const char *path)
{
    int index = fs_find(path);

    if (index < 0)
        return 0;

    return file_table[index].type == FS_TYPE_DIR;
}

int build_absolute_path(
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

int fs_save_superblock(void)
{
    uint8_t sector[512];

    if (!ata_is_ready())
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

    if (!ata_is_ready())
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

int fs_save_table(void)
{
    uint8_t sector[512];
    uint32_t total_bytes =
        sizeof(FSFile) * FS_MAX_FILES;

    uint32_t sector_index;

    if (!ata_is_ready())
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

    if (!ata_is_ready())
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

int fs_format(void)
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

    if (!ata_is_ready())
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

void fs_init(void)
{
    if (!ata_is_ready())
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

int fs_touch(const char *input)
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

int fs_mkdir(const char *input)
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

void fs_ls(void)
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

int fs_list_directory(char names[][FS_NAME_LENGTH], uint8_t *types, int max_entries)
{
    int i, count = 0;
    if (!names || !types || max_entries <= 0)
        return 0;
    for (i = 0; i < FS_MAX_FILES && count < max_entries; i++)
    {
        int len, start = 0, j;
        if (file_table[i].type == FS_TYPE_FREE ||
            kstrcmp(file_table[i].parent, current_directory) != 0)
            continue;
        len = kstrlen(file_table[i].name);
        for (j = 0; j < len; j++)
            if (file_table[i].name[j] == '/') start = j + 1;
        kcopy_bounded(names[count], file_table[i].name + start, FS_NAME_LENGTH);
        types[count] = file_table[i].type;
        count++;
    }
    return count;
}

/* =========================================================
   PWD
   ========================================================= */

void fs_pwd(void)
{
    print(current_directory);
    print("\n");
}

/* =========================================================
   CD
   ========================================================= */

int fs_cd(const char *input)
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

int fs_write_absolute(
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

int fs_read_absolute(const char *path, char *buffer, int capacity)
{
    int index = fs_find(path);
    int length;
    int i;

    if (index < 0 || file_table[index].type != FS_TYPE_FILE ||
        !buffer || capacity <= 0)
        return -1;

    length = (int)file_table[index].size;
    if (length >= capacity)
        length = capacity - 1;

    for (i = 0; i < length; i++)
        buffer[i] = file_table[index].content[i];
    buffer[length] = '\0';
    return length;
}

int fs_write(
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

int fs_cat(const char *input)
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

int fs_rm(const char *input)
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

int find_user(const char *username)
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

int useradd(
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

int login_user(
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
   LOGIN AT BOOT
   ========================================================= */

void fs_boot_login_prompt(void)
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
