#include <stdint.h>
#include "../kernel.h"
#include "../fs/slicefs.h"
#include "../timezone.h"

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
    io_outw(0x604, 0x2000);

    /*
     * Bochs.
     */
    io_outw(0xB004, 0x2000);

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

static char command_lower(char c)
{
    if(c>='A'&&c<='Z')return (char)(c-'A'+'a');
    return c;
}

static int command_equals(const char *a,const char *b)
{
    while(*a&&*b&&command_lower(*a)==command_lower(*b)){a++;b++;}
    return *a==*b;
}

static int command_starts(const char *a,const char *prefix,int n)
{
    int i;
    for(i=0;i<n;i++)if(!a[i]||command_lower(a[i])!=command_lower(prefix[i]))return 0;
    return 1;
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

void shell_execute_command(char *command)
{
    if (command_equals(command, ""))
        return;

    /*
     * help
     */
    if (command_equals(command, "help"))
    {
        print(
            "\n"
            "Comandos:\n"
            "  help\n"
            "  clear\n"
            "  neofetch\n"
            "  uname\n"
            "  date\n"
            "  date setup\n"
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
    if (command_equals(command, "clear"))
    {
        clear_screen();
        return;
    }

    /*
     * neofetch
     */
    if (command_equals(command, "neofetch"))
    {
        neofetch();
        return;
    }

    /*
     * uname
     */
    if (command_equals(command, "uname"))
    {
        print(
            "OrangeOS 0.5.0 i386\n"
        );

        return;
    }

    /*
     * date setup
     */
    if (command_equals(command, "date setup"))
    {
        print(
            "\n"
            "========================================\n"
            "          CONFIGURACAO DE FUSO\n"
            "========================================\n\n"
            "1. UTC\n"
            "2. Europe/Lisbon\n"
            "3. Europe/London\n"
            "4. Europe/Madrid\n"
            "5. America/New_York\n"
            "6. America/Sao_Paulo\n"
            "\n"
            "Escolha: "
        );

        char choice[4];

        read_line_input(
            choice,
            sizeof(choice)
        );

        if (kstrcmp(choice, "1") == 0)
        {
            timezone_set(TZ_UTC);
            print("Fuso definido: UTC\n");
        }
        else if (kstrcmp(choice, "2") == 0)
        {
            timezone_set(TZ_LISBON);
            print("Fuso definido: Europe/Lisbon\n");
        }
        else if (kstrcmp(choice, "3") == 0)
        {
            timezone_set(TZ_LONDON);
            print("Fuso definido: Europe/London\n");
        }
        else if (kstrcmp(choice, "4") == 0)
        {
            timezone_set(TZ_MADRID);
            print("Fuso definido: Europe/Madrid\n");
        }
        else if (kstrcmp(choice, "5") == 0)
        {
            timezone_set(TZ_NEW_YORK);
            print("Fuso definido: America/New_York\n");
        }
        else if (kstrcmp(choice, "6") == 0)
        {
            timezone_set(TZ_SAO_PAULO);
            print("Fuso definido: America/Sao_Paulo\n");
        }
        else
        {
            print("Opcao invalida.\n");
        }

        return;
    }

    /*
     * date
     */
    if (command_equals(command, "date"))
    {
        DateTime dt;

        timezone_get_datetime(&dt);

        print("\n");

        switch (dt.weekday)
        {
            case 0: print("Sunday, "); break;
            case 1: print("Monday, "); break;
            case 2: print("Tuesday, "); break;
            case 3: print("Wednesday, "); break;
            case 4: print("Thursday, "); break;
            case 5: print("Friday, "); break;
            case 6: print("Saturday, "); break;
            default: print("Unknown, "); break;
        }

        if (dt.day < 10)
            putchar_kernel('0');

        print_int(dt.day);
        print(" ");

        switch (dt.month)
        {
            case 1:  print("January"); break;
            case 2:  print("February"); break;
            case 3:  print("March"); break;
            case 4:  print("April"); break;
            case 5:  print("May"); break;
            case 6:  print("June"); break;
            case 7:  print("July"); break;
            case 8:  print("August"); break;
            case 9:  print("September"); break;
            case 10: print("October"); break;
            case 11: print("November"); break;
            case 12: print("December"); break;
            default: print("Unknown"); break;
        }

        print(" ");
        print_int(dt.year);
        print("\n");

        print("Time: ");

        if (dt.hour < 10)
            putchar_kernel('0');

        print_int(dt.hour);

        putchar_kernel(':');

        if (dt.minute < 10)
            putchar_kernel('0');

        print_int(dt.minute);

        putchar_kernel(':');

        if (dt.second < 10)
            putchar_kernel('0');

        print_int(dt.second);

        print("\n");

        print("Timezone: ");
        print(timezone_get_name());
        print("\n");

        print("UTC offset: ");

        {
            int offset =
                timezone_get_offset_minutes();

            if (offset >= 0)
            {
                putchar_kernel('+');
            }
            else
            {
                putchar_kernel('-');
                offset = -offset;
            }

            if ((offset / 60) < 10)
                putchar_kernel('0');

            print_int(offset / 60);

            putchar_kernel(':');

            if ((offset % 60) < 10)
                putchar_kernel('0');

            print_int(offset % 60);
        }

        print("\nDST: ");

        if (timezone_is_dst())
            print("active\n");
        else
            print("inactive\n");

        print("\n");

        return;
    }

    /*
     * ls
     */
    if (command_equals(command, "ls"))
    {
        fs_ls();
        return;
    }

    /*
     * pwd
     */
    if (command_equals(command, "pwd"))
    {
        fs_pwd();
        return;
    }

    /*
     * shutdown
     */
    if (command_equals(command, "shutdown") ||
        command_equals(command, "bye"))
    {
        shutdown_system();
        return;
    }

    /*
     * format
     */
    if (command_equals(command, "format"))
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
    if (command_equals(command, "user"))
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
    if (command_starts(command, "cd ", 3))
    {
        fs_cd(command + 3);
        return;
    }

    /*
     * mkdir
     */
    if (command_starts(command, "mkdir ", 6))
    {
        if (fs_mkdir(command + 6))
            print("Diretorio criado.\n");

        return;
    }

    /*
     * touch
     */
    if (command_starts(command, "touch ", 6))
    {
        if (fs_touch(command + 6))
            print("Ficheiro criado.\n");

        return;
    }

    /*
     * cat
     */
    if (command_starts(command, "cat ", 4))
    {
        fs_cat(command + 4);
        return;
    }

    /*
     * rm
     */
    if (command_starts(command, "rm ", 3))
    {
        if (fs_rm(command + 3))
            print("Removido.\n");

        return;
    }

    /*
     * leaf
     */
    if (command_starts(command, "leaf ", 5))
    {
        if (console_graphics_active())
            graphical_leaf_editor(command + 5);
        else
            leaf_editor(command + 5);
        return;
    }

    /*
     * write
     */
    if (command_starts(command, "write ", 6))
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
    if (command_starts(command, "echo ", 5))
    {
        print(command + 5);
        print("\n");

        return;
    }

    /*
     * useradd
     */
    if (command_starts(command, "useradd ", 8))
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
    if (command_starts(command, "user ", 5))
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

    print(
        "Comando desconhecido. Use 'help'.\n"
    );
}

/* =========================================================
   SHELL
   ========================================================= */

void shell(void)
{
    char command[128];

    while (1)
    {
        shell_prompt();

        read_line_input(
            command,
            sizeof(command)
        );

        if (command_equals(command, "exit") ||
            command_equals(command, "desktop"))
            return;

        shell_execute_command(command);
    }
}
