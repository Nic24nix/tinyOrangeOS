#include <stdint.h>
#include "kernel.h"
#include "timezone.h"

void kernel_main(void)
{
    clear_screen();
    timezone_init();

    print("\n========================================\n");
    print("             ORANGEOS v0.5.0\n");
    print("========================================\n\n");
    print("Inicializando hardware...\n");

    if (ata_init())
        print("[ OK ] ATA PIO\n");
    else
    {
        print("[ !! ] ATA indisponivel\n");
        print("      filesystem sera apenas memoria.\n");
    }

    print("[ .. ] Inicializando SliceFS...\n");
    fs_init();
    print("[ OK ] SliceFS\n");

    fs_boot_login_prompt();
    print("\nOrangeOS pronto!\n");
    print("Escreve 'help' para ver os comandos.\n");
    shell();

    for (;;)
        __asm__ volatile ("hlt");
}
