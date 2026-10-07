#include <stdint.h>
#include "kernel.h"

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
    io_inb(ATA_CONTROL);
    io_inb(ATA_CONTROL);
    io_inb(ATA_CONTROL);
    io_inb(ATA_CONTROL);
}

static int ata_wait_not_busy(void)
{
    uint32_t timeout = ATA_TIMEOUT;

    while (timeout--)
    {
        uint8_t status = io_inb(ATA_STATUS);

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
        uint8_t status = io_inb(ATA_STATUS);

        if (status & ATA_STATUS_ERR)
            return 0;

        if (status & ATA_STATUS_DF)
            return 0;

        if (status & ATA_STATUS_DRQ)
            return 1;
    }

    return 0;
}

int ata_read_sector(uint32_t lba, uint8_t *buffer)
{
    int i;

    if (!ata_wait_not_busy())
        return 0;

    io_outb(ATA_DRIVE,
         0xE0 | ((lba >> 24) & 0x0F));

    ata_delay();

    io_outb(ATA_SECCOUNT, 1);
    io_outb(ATA_LBA_LOW,  (uint8_t)(lba));
    io_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    io_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

    io_outb(ATA_COMMAND, ATA_CMD_READ);

    if (!ata_wait_drq())
        return 0;

    for (i = 0; i < 256; i++)
    {
        uint16_t value = io_inw(ATA_DATA);

        buffer[i * 2]     = (uint8_t)(value & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(value >> 8);
    }

    return 1;
}

int ata_write_sector(uint32_t lba, const uint8_t *buffer)
{
    int i;

    if (!ata_wait_not_busy())
        return 0;

    io_outb(ATA_DRIVE,
         0xE0 | ((lba >> 24) & 0x0F));

    ata_delay();

    io_outb(ATA_SECCOUNT, 1);
    io_outb(ATA_LBA_LOW,  (uint8_t)(lba));
    io_outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    io_outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));

    io_outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (!ata_wait_drq())
        return 0;

    for (i = 0; i < 256; i++)
    {
        uint16_t value =
            (uint16_t)buffer[i * 2] |
            ((uint16_t)buffer[i * 2 + 1] << 8);

        io_outw(ATA_DATA, value);
    }

    io_outb(ATA_COMMAND, ATA_CMD_FLUSH);

    return ata_wait_not_busy();
}


int ata_init(void)
{
    uint8_t test_sector[512];
    disk_ready = ata_read_sector(0, test_sector);
    return disk_ready;
}

int ata_is_ready(void)
{
    return disk_ready;
}
