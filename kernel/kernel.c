#include <stdint.h>
#include "kernel.h"
#include "timezone.h"

void kernel_main(uint32_t multiboot_info)
{
    timezone_init();
    desktop_prepare(multiboot_info);
    clear_screen();

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
    desktop();

    for (;;)
        __asm__ volatile ("hlt");
}
