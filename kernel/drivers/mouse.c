#include <stdint.h>
#include "../kernel.h"

#define PS2_STATUS 0x64
#define PS2_DATA   0x60

static int mouse_x = 160;
static int mouse_y = 100;
static uint8_t packet[4];
static uint8_t packet_index;
static uint8_t old_buttons;
static uint8_t packet_size=3;

static int mouse_wait_input(void)
{
    int timeout = 100000;
    while (timeout--)
        if (!(io_inb(PS2_STATUS) & 2)) return 1;
    return 0;
}

static int mouse_read_aux(uint8_t *value)
{
    int timeout = 100000;
    while (timeout--)
    {
        uint8_t status=io_inb(PS2_STATUS);
        if((status&1)&&(status&0x20)){*value=io_inb(PS2_DATA);return 1;}
    }
    return 0;
}

static int mouse_command(uint8_t command)
{
    uint8_t ack;
    int attempt;
    for(attempt=0;attempt<3;attempt++)
    {
        if(!mouse_wait_input())return 0;
        io_outb(PS2_STATUS,0xD4);
        if(!mouse_wait_input())return 0;
        io_outb(PS2_DATA,command);
        if(!mouse_read_aux(&ack))return 0;
        if(ack==0xFA)return 1;
        if(ack!=0xFE)return 0;
    }
    return 0;
}

void mouse_init(void)
{
    uint8_t id=0;
    if(!mouse_wait_input())return;
    io_outb(PS2_STATUS, 0xA8); /* enable auxiliary device */
    (void)mouse_command(0xF6);  /* defaults */
    /* IntelliMouse wheel negotiation: sample rates 200, 100, 80. */
    (void)mouse_command(0xF3); (void)mouse_command(200);
    (void)mouse_command(0xF3); (void)mouse_command(100);
    (void)mouse_command(0xF3); (void)mouse_command(80);
    if(mouse_command(0xF2) && mouse_read_aux(&id) && id==3)packet_size=4;
    (void)mouse_command(0xF4);  /* stream mode */
    packet_index = 0;
    old_buttons = 0;
}

int mouse_poll(int *x, int *y, uint8_t *buttons, int *wheel)
{
    uint8_t status;
    int dx, dy;

    status = io_inb(PS2_STATUS);
    if (!(status & 1) || !(status & 0x20))
        return 0;

    {
        uint8_t byte=io_inb(PS2_DATA);
        if(packet_index==0 && !(byte&0x08))
            return 0;
        packet[packet_index++] = byte;
    }
    if (packet_index < packet_size)
        return 0;
    packet_index = 0;

    /* Discard overflow packets; bit 3 marks packet synchronization. */
    if (!(packet[0] & 0x08) || (packet[0] & 0xC0))
        return 0;
    dx = (int)(int8_t)packet[1];
    dy = (int)(int8_t)packet[2];
    mouse_x += dx;
    mouse_y -= dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= 320) mouse_x = 319;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= 200) mouse_y = 199;

    *x = mouse_x;
    *y = mouse_y;
    *buttons = packet[0] & 7;
    if(wheel)*wheel=0;
    if(packet_size==4 && wheel){
        int delta=(int)(packet[3]&0x0F);
        if(delta&8)delta-=16;
        *wheel=delta;
    }
    {
        int changed = *buttons != old_buttons;
        old_buttons = *buttons;
        return changed ? 2 : ((wheel && *wheel) ? 3 : 1);
    }
}
