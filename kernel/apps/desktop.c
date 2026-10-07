#include <stdint.h>
#include "../kernel.h"
#include "../fs/slicefs.h"
#include "../timezone.h"

#define W 320
#define H 200
#define FB_INFO_FLAGS 0
#define FB_INFO_ADDR  88
#define FB_INFO_PITCH 96
#define FB_INFO_WIDTH 100
#define FB_INFO_HEIGHT 104
#define FB_INFO_BPP   108
#define FB_INFO_TYPE  109

enum { C_BG=0x26354A, C_PANEL=0x202936, C_WHITE=0xFFFFFF, C_TEXT=0xD8E0EA,
       C_MUTED=0x8492A6, C_ORANGE=0xF28C28, C_WIN=0x10151D,
       C_TITLE=0xD96C19, C_BLACK=0x090C11, C_BLUE=0x285C8E,
       C_GREEN=0x6DBB75, C_CYAN=0x75C9D1 };

static volatile uint8_t *fb;
static uint32_t pitch, screen_w, screen_h;
static uint8_t bpp, fb_type;
static uint8_t red_pos, green_pos, blue_pos;
static int mouse_x=160, mouse_y=100;
static int shown_mouse_x=-1, shown_mouse_y=-1;
static int window=0; /* 0 desktop, 1 terminal, 2 files, 3 help, 4 leaf, 5 terminal-only */
static int boot_console;
static int start_open, file_preview_open;
static int win_x=10, win_y=8, dragging;
static int drag_dx, drag_dy;
static char lines[128][50];
static int line_count=1, term_col;
static int term_view_offset;
static char input[128];
static int input_len;
static int term_row=0;
static uint8_t last_clock_second=255;
static uint32_t clock_poll_count;
static uint32_t backbuffer[W*H];
static char fm_names[16][FS_NAME_LENGTH];
static uint8_t fm_types[16];
static int fm_count, fm_selected;
static char fm_preview[FS_CONTENT_LENGTH];
static int file_create_mode, file_create_len;
static char file_create_name[FS_NAME_LENGTH];
static char fm_message[48];
static char gui_leaf_buffer[FS_CONTENT_LENGTH];
static char gui_leaf_path[FS_NAME_LENGTH];
static char gui_leaf_status[48];
static int gui_leaf_length, gui_leaf_cursor, gui_leaf_dirty;
static int gui_leaf_scroll;

/* Compact 5x7 font. Lowercase is rendered as uppercase in this early GUI. */
static const uint8_t font[36][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
static const uint8_t lower_font[26][7] = {
 {0,0,14,1,15,17,15},{16,16,30,17,17,17,30},{0,0,14,16,16,17,14},
 {1,1,15,17,17,19,13},{0,0,14,17,31,16,14},{6,9,8,28,8,8,8},
 {0,0,15,17,17,15,1},{16,16,30,17,17,17,17},{4,0,12,4,4,4,14},
 {2,0,6,2,2,18,12},{16,16,18,20,24,20,18},{12,4,4,4,4,4,14},
 {0,0,26,21,21,21,21},{0,0,30,17,17,17,17},{0,0,14,17,17,17,14},
 {0,0,30,17,17,30,16},{0,0,15,17,17,15,1},{0,0,22,25,16,16,16},
 {0,0,15,16,14,1,30},{8,8,28,8,8,9,6},{0,0,17,17,17,19,13},
 {0,0,17,17,17,10,4},{0,0,17,17,21,21,10},{0,0,17,10,4,10,17},
 {0,0,17,17,17,15,1},{0,0,31,2,4,8,31}
};

static uint32_t read_u32(uint32_t p) { return *(volatile uint32_t *)p; }

static int equals_ignore_case(const char *a,const char *b)
{
    while(*a&&*b){char ca=*a,cb=*b;if(ca>='A'&&ca<='Z')ca+=32;if(cb>='A'&&cb<='Z')cb+=32;if(ca!=cb)return 0;a++;b++;}
    return *a==*b;
}

static void pixel(int x,int y,uint32_t color)
{
    int pos;
    static const uint32_t palette[16]={
        0x090C11,0x26354A,0x2878B8,0x45B8C4,0xD84848,0x9D5FD0,0xF28C28,0xD8E0EA,
        0x202936,0xD96C19,0x6DBB75,0x75C9D1,0xE87878,0xB990E8,0xFFD166,0xFFFFFF
    };
    if(color<16) color=palette[color];
    if (!fb || x<0 || y<0 || x>=W || y>=H) return;
    pos=y*W+x;
    backbuffer[pos]=color;
}

static void framebuffer_pixel(int x,int y,uint32_t color)
{
    volatile uint8_t *p=fb+(uint32_t)y*pitch+(uint32_t)x*4;
    uint32_t r=(color>>16)&255,g=(color>>8)&255,b=color&255;
    uint32_t c=(r<<red_pos)|(g<<green_pos)|(b<<blue_pos);
    p[0]=(uint8_t)c;p[1]=(uint8_t)(c>>8);p[2]=(uint8_t)(c>>16);p[3]=(uint8_t)(c>>24);
}

static int cursor_pixel(int x,int y,int cx,int cy)
{
    int dx=x-cx,dy=y-cy;
    return (dx==0 && dy>=0 && dy<8) || (dx==dy && dx>=0 && dx<5);
}

static void rect(int x,int y,int w,int h,uint32_t c);
static void text(int x,int y,const char *s,uint32_t fg,uint32_t bg);

static void present_cursor_region(int cx,int cy,int oldx,int oldy)
{
    int left=cx,top=cy,right=cx+5,bottom=cy+8,x,y;
    if(oldx>=0){if(oldx<left)left=oldx;if(oldy<top)top=oldy;if(oldx+5>right)right=oldx+5;if(oldy+8>bottom)bottom=oldy+8;}
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>W)right=W;
    if(bottom>H)bottom=H;
    for(y=top;y<bottom;y++)for(x=left;x<right;x++){
        uint32_t c=backbuffer[y*W+x];
        if(cursor_pixel(x,y,cx,cy))c=0xFFFFFF;
        framebuffer_pixel(x*2,y*2+40,c);framebuffer_pixel(x*2+1,y*2+40,c);
        framebuffer_pixel(x*2,y*2+41,c);framebuffer_pixel(x*2+1,y*2+41,c);
    }
    shown_mouse_x=cx;shown_mouse_y=cy;
}

static void present_region(int left,int top,int right,int bottom)
{
    int x,y;
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>W)right=W;
    if(bottom>H)bottom=H;
    for(y=top;y<bottom;y++)for(x=left;x<right;x++){
        uint32_t c=backbuffer[y*W+x];
        if(cursor_pixel(x,y,mouse_x,mouse_y))c=0xFFFFFF;
        framebuffer_pixel(x*2,y*2+40,c);framebuffer_pixel(x*2+1,y*2+40,c);
        framebuffer_pixel(x*2,y*2+41,c);framebuffer_pixel(x*2+1,y*2+41,c);
    }
}

static void update_clock(int force)
{
    DateTime dt;
    char value[17];
    int year;
    if(!force){
        clock_poll_count++;
        if(clock_poll_count<120000)return;
        clock_poll_count=0;
    }
    timezone_get_datetime(&dt);
    if(!force&&last_clock_second==dt.second)return;
    last_clock_second=dt.second;
    value[0]='0'+dt.hour/10;value[1]='0'+dt.hour%10;value[2]=':';
    value[3]='0'+dt.minute/10;value[4]='0'+dt.minute%10;value[5]=' ';
    value[6]='0'+dt.day/10;value[7]='0'+dt.day%10;value[8]='/';
    value[9]='0'+dt.month/10;value[10]='0'+dt.month%10;value[11]='/';
    year=dt.year;value[15]='0'+year%10;year/=10;value[14]='0'+year%10;year/=10;
    value[13]='0'+year%10;year/=10;value[12]='0'+year%10;value[16]=0;
    rect(207,184,113,16,8);
    text(211,189,value,15,8);
    if(!force)present_region(207,184,320,200);
}

static void present_scene(void)
{
    int x,y;
    for(y=0;y<H;y++)for(x=0;x<W;x++){
        uint32_t c=backbuffer[y*W+x];
        framebuffer_pixel(x*2,y*2+40,c);framebuffer_pixel(x*2+1,y*2+40,c);
        framebuffer_pixel(x*2,y*2+41,c);framebuffer_pixel(x*2+1,y*2+41,c);
    }
    present_cursor_region(mouse_x,mouse_y,-1,-1);
}

static void rect(int x,int y,int w,int h,uint32_t c)
{ int a,b; for(b=y;b<y+h;b++) for(a=x;a<x+w;a++) pixel(a,b,c); }

static uint8_t punctuation_row(char ch,int row)
{
    static const char chars[]="!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    static const uint8_t rows[32][7]={
        {4,4,4,4,4,0,4},{10,10,10,0,0,0,0},{10,31,10,10,31,10,10},
        {4,15,20,14,5,30,4},{24,25,2,4,8,19,3},{12,18,20,8,21,18,13},
        {4,4,8,0,0,0,0},{2,4,8,8,8,4,2},{8,4,2,2,2,4,8},
        {0,10,4,31,4,10,0},{0,4,4,31,4,4,0},{0,0,0,0,4,4,8},
        {0,0,0,31,0,0,0},{0,0,0,0,0,4,0},{1,2,2,4,8,8,16},
        {0,4,0,0,0,4,0},{0,4,0,0,4,0,0},{2,4,8,16,8,4,2},
        {0,0,31,0,31,0,0},{16,8,4,2,4,8,16},{14,17,1,2,4,0,4},
        {14,17,23,21,23,16,14},{14,8,8,8,8,8,14},{1,2,2,4,8,8,16},
        {14,2,2,2,2,2,14},{4,10,17,0,0,0,0},{0,0,0,0,0,0,31},
        {8,4,2,0,0,0,0},{2,4,4,8,4,4,2},{4,4,4,4,4,4,4},
        {8,4,2,2,4,4,8},{0,0,9,22,0,0,0}
    };
    int i;
    for(i=0;i<32;i++)if(chars[i]==ch)return rows[i][row];
    return 0;
}

static void glyph(int x,int y,char ch,uint32_t fg,uint32_t bg)
{
    int r,c,index=-1,lower=0;
    if(ch>='a'&&ch<='z'){index=ch-'a';lower=1;}
    else if(ch>='A'&&ch<='Z') index=ch-'A';
    else if(ch>='0'&&ch<='9') index=26+ch-'0';
    for(r=0;r<7;r++) for(c=0;c<6;c++) {
        uint8_t bits=0;
        if(index>=0 && c<5) bits=lower?lower_font[index][r]:font[index][r];
        else if(index<0 && c<5) bits=punctuation_row(ch,r);
        pixel(x+c,y+r,(bits&(1<<(4-c)))?fg:bg);
    }
}

static void text(int x,int y,const char *s,uint32_t fg,uint32_t bg)
{ while(*s){ if(*s=='\n'){x=4;y+=9;} else glyph(x,y,*s,fg,bg); x+=6; s++; } }

static void term_newline(void)
{
    if(line_count<128) line_count++;
    else { int i; for(i=1;i<128;i++){ int j; for(j=0;j<50;j++) lines[i-1][j]=lines[i][j]; } for(i=0;i<50;i++) lines[127][i]=0; }
    term_view_offset=0;
    term_row=line_count-1; term_col=0;
}

static void draw_boot_console(void);
static void draw_terminal(void);
static void refresh_terminal_display(void);
static void draw_terminal_only(void);

void console_graphics_putchar(char c)
{
    if(c=='\n'){term_newline();if(boot_console)draw_boot_console();else refresh_terminal_display();return;}
    if(c=='\r'){term_col=0;if(boot_console)draw_boot_console();else refresh_terminal_display();return;}
    if(c=='\b'){if(term_col>0){term_col--;lines[term_row][term_col]=0;}if(boot_console)draw_boot_console();else refresh_terminal_display();return;}
    if(c=='\t'){int n;for(n=0;n<4;n++) console_graphics_putchar(' ');return;}
    if(c<32||c>126)c='?';
    if(term_col>=49) term_newline();
    lines[term_row][term_col++]=c;
    lines[term_row][term_col]=0;
    if(boot_console)draw_boot_console();
    else refresh_terminal_display();
}

void console_graphics_clear(void)
{ int r,c; for(r=0;r<128;r++)for(c=0;c<50;c++)lines[r][c]=0;line_count=1;term_row=term_col=term_view_offset=0;if(boot_console)draw_boot_console();else refresh_terminal_display(); }

static void draw_terminal(void)
{
    int i,start=line_count-19-term_view_offset;
    if(start<0)start=0;
    rect(win_x+2,win_y+3,300,180,0);
    rect(win_x,win_y,300,178,7);
    rect(win_x+1,win_y+1,298,176,0);
    rect(win_x+2,win_y+2,296,13,9);
    text(win_x+6,win_y+5,"Orange Terminal",15,9);
    rect(win_x+284,win_y+3,12,11,4); text(win_x+287,win_y+5,"X",15,4);
    rect(win_x+3,win_y+17,294,157,0);
    for(i=0;i<19&&start+i<line_count;i++) text(win_x+6,win_y+20+i*8,lines[start+i],10,0);
    if(term_view_offset==0&&term_row>=start&&term_row<start+19&&term_col<49){ char cur[2]={'_',0}; text(win_x+6+term_col*6,win_y+20+(term_row-start)*8,cur,15,0); }
}

static void refresh_terminal_display(void)
{
    if(window==1&&fb){draw_terminal();present_region(win_x,win_y,win_x+300,win_y+178);}
    else if(window==5&&fb)draw_terminal_only();
}

static void draw_terminal_only(void)
{
    int i,start=line_count-20-term_view_offset;
    if(start<0)start=0;
    rect(0,0,W,H,C_BLACK);
    for(i=0;i<20&&start+i<line_count;i++)text(8,8+i*9,lines[start+i],C_TEXT,C_BLACK);
    if(term_view_offset==0&&term_row>=start&&term_row<start+20&&term_col<49){char cursor[2]={'_',0};text(8+term_col*6,8+(term_row-start)*9,cursor,C_WHITE,C_BLACK);}
    present_scene();
}

void console_graphics_string_at(int row,int col,const char *str,uint8_t color)
{
    (void)color;
    if(row<0||row>=128||col<0||col>=49||!str)return;
    while(*str&&col<49&&*str!='\n')lines[row][col++]=(*str>=32&&*str<=126)?*str:'?';
    if(col<50)lines[row][col]=0;
    if(row>=line_count)line_count=row+1;
    term_row=row;term_col=col;
    refresh_terminal_display();
}

static void refresh_files(void)
{
    fm_count=fs_list_directory(fm_names,fm_types,16);
    if(fm_count==0)fm_selected=0;
    else if(fm_selected<0||fm_selected>=fm_count)fm_selected=0;
}

static void draw_files(void)
{
    int i;
    rect(win_x+2,win_y+3,300,180,0); rect(win_x,win_y,300,178,7);
    rect(win_x+1,win_y+1,298,176,0); rect(win_x+2,win_y+2,296,13,9);
    text(win_x+6,win_y+5,"SliceFS Files",15,9);
    rect(win_x+284,win_y+3,12,11,4); text(win_x+287,win_y+5,"X",15,4);
    rect(win_x+3,win_y+17,294,157,0);
    rect(win_x+3,win_y+17,294,15,8);
    rect(win_x+5,win_y+18,22,13,6);text(win_x+9,win_y+21,"UP",15,6);
    text(win_x+32,win_y+21,current_directory,15,8);
    rect(win_x+3,win_y+33,294,16,0);
    rect(win_x+5,win_y+34,76,14,3);text(win_x+9,win_y+38,"+ FOLDER",15,3);
    rect(win_x+84,win_y+34,68,14,3);text(win_x+89,win_y+38,"+ TXT",15,3);
    if(fm_message[0])text(win_x+158,win_y+38,fm_message,14,0);
    if(file_create_mode){
        rect(win_x+28,win_y+58,244,50,8);rect(win_x+30,win_y+60,240,46,0);
        text(win_x+37,win_y+66,file_create_mode==1?"NEW FOLDER NAME":"NEW TXT FILE NAME",15,0);
        rect(win_x+36,win_y+79,224,13,8);text(win_x+40,win_y+82,file_create_name,15,8);
        text(win_x+38,win_y+96,"ENTER: CREATE   ESC: CANCEL",7,0);
        return;
    }
    if(file_preview_open){
        text(win_x+7,win_y+55,"FILE PREVIEW  (ESC to return)",10,0);
        for(i=0;i<14;i++){
            char row[50];int j=0,pos=i*48;
            while(j<48&&fm_preview[pos+j]&&fm_preview[pos+j]!='\n'&&fm_preview[pos+j]!='\r'){row[j]=fm_preview[pos+j];j++;}
            row[j]=0;text(win_x+7,win_y+67+i*8,row,15,0);
        }
    }else{
        for(i=0;i<fm_count&&i<13;i++){
            int j=0;char shown[48];
            while(j<46&&fm_names[i][j]){shown[j]=fm_names[i][j];j++;}
            shown[j]=0;
            if(i==fm_selected)rect(win_x+5,win_y+51+i*9,286,9,3);
            text(win_x+8,win_y+52+i*9,fm_types[i]==2?"[DIR]":"[FILE]",fm_types[i]==2?14:10,i==fm_selected?3:0);
            text(win_x+47,win_y+52+i*9,shown,15,i==fm_selected?3:0);
        }
        if(!fm_count)text(win_x+8,win_y+57,"This folder is empty.",8,0);
    }
}

static void draw_start_menu(void)
{
    if(!start_open)return;
    rect(5,54,150,129,0);rect(3,52,150,129,7);rect(4,53,148,127,8);
    rect(4,53,148,24,6);text(10,57,"TINY ORANGE OS",15,6);
    text(10,68,current_user,14,6);
    rect(7,81,142,17,8);text(13,86,">_  TERMINAL",15,8);
    rect(7,100,142,17,8);text(13,105,"DIR  FILES",15,8);
    rect(7,119,142,17,8);text(13,124,"?  HELP",15,8);
    rect(7,138,142,17,8);text(13,143,"TERMINAL ONLY",15,8);
    rect(7,157,142,17,4);text(13,162,"POWER OFF",15,4);
}

static void draw_help(void)
{
    rect(win_x+2,win_y+3,300,180,0);rect(win_x,win_y,300,178,7);
    rect(win_x+1,win_y+1,298,176,0);rect(win_x+2,win_y+2,296,13,9);
    text(win_x+6,win_y+5,"OrangeOS Help",15,9);
    rect(win_x+284,win_y+3,12,11,4);text(win_x+287,win_y+5,"X",15,4);
    text(win_x+10,win_y+28,"Open apps from the Start menu.",10,0);
    text(win_x+10,win_y+42,"ESC closes this window.",10,0);
    text(win_x+10,win_y+72,"FILES: click a folder to enter it.",15,0);
    text(win_x+10,win_y+84,"Click a file to preview its contents.",15,0);
}

static void render(void)
{
    int x,y;
    if(window==5){draw_terminal_only();return;}
    rect(0,0,W,H,1);
    /* Soft two-tone wallpaper */
    for(y=0;y<184;y++) for(x=0;x<W;x++) if(y>110) pixel(x,y,9);
    text(12,10,"TINY ORANGE OS",15,1);
    text(105,80,"Welcome to tinyOrangeOS",15,1);
    text(105,92,"Open applications from Start.",7,1);
    rect(0,184,W,16,8); rect(3,187,50,11,6); text(8,189,"START",15,6);
    update_clock(1);
    if(window==1)draw_terminal(); else if(window==2)draw_files(); else if(window==3)draw_help();
    draw_start_menu();
    present_scene();
}

static void draw_boot_console(void)
{
    int i,start=line_count-19;
    if(start<0)start=0;
    rect(0,0,W,H,C_BG);
    rect(0,0,W,18,C_PANEL);
    text(8,5,"TINYORANGEOS STARTUP",C_WHITE,C_PANEL);
    for(i=0;i<19&&start+i<line_count;i++) text(8,25+i*9,lines[start+i],C_TEXT,C_BG);
    if(term_row>=start&&term_row<start+19&&term_col<49) rect(8+term_col*6,25+(term_row-start)*9,5,7,C_ORANGE);
    present_scene();
}

static void open_term(void)
{
    window=1; win_x=10;win_y=8; console_graphics_clear(); input_len=0;input[0]=0;
    print("OrangeOS graphical terminal\nType help for commands; exit returns to desktop.\n");
    print(current_user);print("@orangeos:");print(current_directory);print("$ ");
}

static void open_files(void)
{ 
    window=2; win_x=10;win_y=8;file_preview_open=0;file_create_mode=0;fm_message[0]=0;fm_selected=0;refresh_files();
}

static void create_file_item(void)
{
    int ok=0;
    file_create_name[file_create_len]=0;
    if(file_create_len>0){
        if(file_create_mode==1)ok=fs_mkdir(file_create_name);
        else{
            int n=file_create_len;
            if(n<4||file_create_name[n-4]!='.'||
               (file_create_name[n-3]!='t'&&file_create_name[n-3]!='T')||
               (file_create_name[n-2]!='x'&&file_create_name[n-2]!='X')||
               (file_create_name[n-1]!='t'&&file_create_name[n-1]!='T')){
                if(n+4<FS_NAME_LENGTH){file_create_name[n++]='.';file_create_name[n++]='t';file_create_name[n++]='x';file_create_name[n++]='t';file_create_name[n]=0;}
            }
            ok=fs_touch(file_create_name);
        }
    }
    if(ok)kstrcpy(fm_message,file_create_mode==1?"Folder added":"TXT added");
    else kstrcpy(fm_message,"Could not create");
    file_create_mode=0;file_create_len=0;file_create_name[0]=0;refresh_files();
}

static void activate_file_row(int row)
{
    char path[FS_NAME_LENGTH];
    if(row<0||row>=fm_count)return;
    fm_selected=row;
    if(fm_types[row]==2){if(fs_cd(fm_names[row])){file_preview_open=0;refresh_files();}}
    else if(build_absolute_path(fm_names[row],path)){
        if(fs_read_absolute(path,fm_preview,sizeof(fm_preview))>=0)file_preview_open=1;
    }
}

static void start_action(int action)
{
    char command[]="shutdown";
    start_open=0;
    if(action==0)open_term();
    else if(action==1)open_files();
    else if(action==2){window=3;win_x=10;win_y=8;}
    else if(action==3){window=5;console_graphics_clear();input_len=0;input[0]=0;
        print("OrangeOS fullscreen terminal. Type 'desktop' to return.\n");
        print(current_user);print("@orangeos:");print(current_directory);print("$ ");}
    else if(action==4)shell_execute_command(command);
}

static void gui_leaf_position(int pos,int *line,int *col)
{
    int i,l=0,c=0;
    if(pos>gui_leaf_length)pos=gui_leaf_length;
    for(i=0;i<pos;i++){
        if(gui_leaf_buffer[i]=='\n'){l++;c=0;}
        else if(++c>=48){l++;c=0;}
    }
    *line=l;*col=c;
}

static int gui_leaf_find(int target_line,int target_col)
{
    int i,l=0,c=0;
    for(i=0;i<gui_leaf_length;i++){
        if(l==target_line&&c>=target_col)return i;
        if(gui_leaf_buffer[i]=='\n'){if(l==target_line)return i+1;l++;c=0;}
        else if(++c>=48){l++;c=0;}
    }
    return gui_leaf_length;
}

static void draw_graphical_leaf(const char *filename)
{
    char view[13][49];
    int i,j,line=0,col=0,cursor_line,cursor_col;
    rect(win_x+2,win_y+3,300,180,0);rect(win_x,win_y,300,178,7);
    rect(win_x+1,win_y+1,298,176,C_BLACK);rect(win_x+2,win_y+2,296,13,C_TITLE);
    text(win_x+6,win_y+5,"tinyLeaf",15,C_TITLE);
    text(win_x+58,win_y+5,filename,15,C_TITLE);
    /* Ctrl+Q closes the editor; no mouse-only close affordance. */
    rect(win_x+3,win_y+17,294,157,C_BLACK);
    text(win_x+7,win_y+21,"CTRL+S SAVE   CTRL+Q CLOSE",10,C_BLACK);
    for(i=0;i<13;i++){for(j=0;j<48;j++)view[i][j]=' ';view[i][48]=0;}
    gui_leaf_position(gui_leaf_cursor,&cursor_line,&cursor_col);
    if(cursor_line<gui_leaf_scroll)gui_leaf_scroll=cursor_line;
    if(cursor_line>=gui_leaf_scroll+13)gui_leaf_scroll=cursor_line-12;
    for(i=0;i<gui_leaf_length;i++){
        char ch=gui_leaf_buffer[i];
        if(ch=='\n'){line++;col=0;continue;}
        if(col>=48){line++;col=0;}
        if(line>=gui_leaf_scroll&&line<gui_leaf_scroll+13)
            view[line-gui_leaf_scroll][col]=(ch>=32&&ch<=126)?ch:'?';
        col++;
    }
    if(cursor_line>=gui_leaf_scroll&&cursor_line<gui_leaf_scroll+13&&cursor_col<48)
        view[cursor_line-gui_leaf_scroll][cursor_col]='_';
    for(i=0;i<13;i++)text(win_x+7,win_y+34+i*9,view[i],C_TEXT,C_BLACK);
    if(gui_leaf_status[0])text(win_x+7,win_y+155,gui_leaf_status,gui_leaf_dirty?14:10,C_BLACK);
    present_region(win_x,win_y,win_x+300,win_y+178);
}

static void gui_leaf_insert(char ch)
{
    int i;
    if(gui_leaf_length>=FS_CONTENT_LENGTH-1)return;
    for(i=gui_leaf_length;i>gui_leaf_cursor;i--)gui_leaf_buffer[i]=gui_leaf_buffer[i-1];
    gui_leaf_buffer[gui_leaf_cursor++]=ch;
    gui_leaf_length++;gui_leaf_buffer[gui_leaf_length]=0;gui_leaf_dirty=1;
}

void graphical_leaf_editor(const char *filename)
{
    uint8_t sc;
    int target_line,target_col,return_window=window;
    if(!build_absolute_path(filename,gui_leaf_path)){print("Invalid leaf path.\n");return;}
    (void)fs_touch(filename);
    gui_leaf_length=fs_read_absolute(gui_leaf_path,gui_leaf_buffer,sizeof(gui_leaf_buffer));
    if(gui_leaf_length<0){gui_leaf_length=0;gui_leaf_buffer[0]=0;}
    gui_leaf_cursor=gui_leaf_length;gui_leaf_scroll=0;gui_leaf_dirty=0;
    kstrcpy(gui_leaf_status,"Ready");window=4;
    draw_graphical_leaf(filename);
    for(;;){
        sc=keyboard_wait_scancode();
        if(sc==0x2A||sc==0x36){shift_pressed=1;continue;}
        if(sc==0xAA||sc==0xB6){shift_pressed=0;continue;}
        if(sc==0x1D){ctrl_pressed=1;continue;}
        if(sc==0x9D){ctrl_pressed=0;continue;}
        if(sc&0x80)continue;
        if(ctrl_pressed&&sc==0x1F){
            if(fs_write_absolute(gui_leaf_path,gui_leaf_buffer)){gui_leaf_dirty=0;kstrcpy(gui_leaf_status,"Saved");}
            else kstrcpy(gui_leaf_status,"Save failed");
            draw_graphical_leaf(filename);continue;
        }
        if(ctrl_pressed&&sc==0x10)break;
        if(sc==0xE0){
            sc=keyboard_wait_scancode();if(sc&0x80)continue;
            if(sc==0x4B){if(gui_leaf_cursor>0)gui_leaf_cursor--;}
            else if(sc==0x4D){if(gui_leaf_cursor<gui_leaf_length)gui_leaf_cursor++;}
            else if(sc==0x47){int i=gui_leaf_cursor-1;while(i>=0&&gui_leaf_buffer[i]!='\n')i--;gui_leaf_cursor=i+1;}
            else if(sc==0x4F){int i=gui_leaf_cursor;while(i<gui_leaf_length&&gui_leaf_buffer[i]!='\n')i++;gui_leaf_cursor=i;}
            else if(sc==0x48||sc==0x50){gui_leaf_position(gui_leaf_cursor,&target_line,&target_col);if(sc==0x48&&target_line>0)target_line--;else if(sc==0x50)target_line++;gui_leaf_cursor=gui_leaf_find(target_line,target_col);}
            else if(sc==0x53&&gui_leaf_cursor<gui_leaf_length){int i;for(i=gui_leaf_cursor;i<gui_leaf_length;i++)gui_leaf_buffer[i]=gui_leaf_buffer[i+1];gui_leaf_length--;gui_leaf_dirty=1;}
            draw_graphical_leaf(filename);continue;
        }
        if(sc==0x1C)gui_leaf_insert('\n');
        else if(sc==0x0E&&gui_leaf_cursor>0){int i;for(i=gui_leaf_cursor-1;i<gui_leaf_length;i++)gui_leaf_buffer[i]=gui_leaf_buffer[i+1];gui_leaf_cursor--;gui_leaf_length--;gui_leaf_dirty=1;}
        else if(sc==0x0F)gui_leaf_insert('\t');
        else {char ch=keyboard_get_char(sc,shift_pressed);if(ch>=32&&ch<=126)gui_leaf_insert(ch);}
        draw_graphical_leaf(filename);
    }
    ctrl_pressed=0;shift_pressed=0;window=return_window;render();
}

static void run_command(void)
{
    int i;
    console_graphics_putchar('\n');
    if(equals_ignore_case(input,"desktop")){
        if(window==1)print("Voce ja esta em desktop.\n");
        else if(window==5)window=0;
        input_len=0;input[0]=0;return;
    }
    if(equals_ignore_case(input,"exit")&&window==1){window=0;input_len=0;return;}
    if(equals_ignore_case(input,"exit")&&window==5)print("Use 'desktop' to return to the desktop.\n");
    else if(equals_ignore_case(input,"clear")) console_graphics_clear();
    else shell_execute_command(input);
    input_len=0;input[0]=0;
    if(window==1||window==5){print(current_user);print("@orangeos:");print(current_directory);print("$ ");}
    for(i=0;i<128;i++) lines[i][49]=0;
}

static int key_input(uint8_t sc)
{
    char c;
    if(sc&0x80)return 0;
    if(sc==0x01){
        if(window==2&&file_create_mode){file_create_mode=0;file_create_len=0;file_create_name[0]=0;return 1;}
        if(window==2&&file_preview_open)file_preview_open=0;
        else if(window!=5)window=0;
        start_open=0;return 1;
    }
    if(window==2){
        if(file_create_mode){
            if(sc==0x1C){create_file_item();return 1;}
            if(sc==0x0E){if(file_create_len){file_create_len--;file_create_name[file_create_len]=0;}return 1;}
            c=keyboard_get_char(sc,shift_pressed);
            if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'){
                if(file_create_len<FS_NAME_LENGTH-5){file_create_name[file_create_len++]=c;file_create_name[file_create_len]=0;return 1;}
            }
            return 0;
        }
        if(file_preview_open&&sc==0x0E){file_preview_open=0;return 1;}
        if(!file_preview_open&&sc==0x48){if(fm_selected>0)fm_selected--;return 1;}
        if(!file_preview_open&&sc==0x50){if(fm_selected+1<fm_count)fm_selected++;return 1;}
        if(!file_preview_open&&sc==0x1C){activate_file_row(fm_selected);return 1;}
        if(!file_preview_open&&sc==0x0E){fs_cd("..");refresh_files();return 1;}
        return 0;
    }
    if(window!=1&&window!=5)return 0;
    if(sc==0x1C){run_command();return 1;}
    if(sc==0x0E){if(input_len){input_len--;input[input_len]=0;console_graphics_putchar('\b');return 1;}return 0;}
    c=keyboard_get_char(sc,shift_pressed);
    if(c>=32&&c<=126&&input_len<126){input[input_len++]=c;input[input_len]=0;console_graphics_putchar(c);return 1;}
    return 0;
}

void desktop_prepare(uint32_t info)
{
    uint32_t flags,addr;
    if(!info){print("No Multiboot information; cannot start framebuffer desktop.\n");for(;;)__asm__ volatile("hlt");}
    flags=read_u32(info+FB_INFO_FLAGS);
    if(!(flags&(1u<<12))){print("GRUB did not provide a framebuffer.\n");for(;;)__asm__ volatile("hlt");}
    addr=read_u32(info+FB_INFO_ADDR);
    fb=(volatile uint8_t *)addr; pitch=read_u32(info+FB_INFO_PITCH);
    screen_w=read_u32(info+FB_INFO_WIDTH);screen_h=read_u32(info+FB_INFO_HEIGHT);
    bpp=*(volatile uint8_t *)(info+FB_INFO_BPP);fb_type=*(volatile uint8_t *)(info+FB_INFO_TYPE);
    red_pos=*(volatile uint8_t *)(info+110);green_pos=*(volatile uint8_t *)(info+112);blue_pos=*(volatile uint8_t *)(info+114);
    if(fb_type!=1 || bpp!=32 || screen_w<640 || screen_h<480){print("Unsupported framebuffer; expected RGB 640x480 VBE.\n");for(;;)__asm__ volatile("hlt");}
    console_set_graphics(1);boot_console=1;console_graphics_clear();draw_boot_console();
}

void desktop(void)
{
    int mx=160,my=100,event,was_down=0;
    uint8_t buttons=0;
    boot_console=0;
    mouse_init();render();
    for(;;){
        update_clock(0);
        {int wheel=0;event=mouse_poll(&mx,&my,&buttons,&wheel);
        if(wheel&&(window==1||window==5)){
            int over=(window==5)||(mx>=win_x&&mx<win_x+300&&my>=win_y+17&&my<win_y+174);
            if(over){term_view_offset-=wheel;if(term_view_offset<0)term_view_offset=0;if(term_view_offset>line_count-1)term_view_offset=line_count-1;refresh_terminal_display();}
        }}
        if(event){
            int scene_dirty=0;
            if((buttons&1)&&!was_down){
                was_down=1;
                if(start_open){
                    if(mx>=7&&mx<149&&my>=81&&my<99)start_action(0);
                    else if(mx>=7&&mx<149&&my>=100&&my<118)start_action(1);
                    else if(mx>=7&&mx<149&&my>=119&&my<137)start_action(2);
                    else if(mx>=7&&mx<149&&my>=138&&my<156)start_action(3);
                    else if(mx>=7&&mx<149&&my>=157&&my<176)start_action(4);
                    else start_open=0;
                    scene_dirty=1;
                }else if(mx<53&&my>=184){start_open=1;scene_dirty=1;}
                else if(window && mx>=win_x+280&&mx<win_x+300&&my>=win_y+2&&my<win_y+16){window=0;scene_dirty=1;}
                else if(window==2&&!file_create_mode&&!file_preview_open&&my>=win_y+17&&my<win_y+33&&mx>=win_x+3&&mx<win_x+28){fs_cd("..");refresh_files();scene_dirty=1;}
                else if(window==2&&!file_create_mode&&my>=win_y+34&&my<win_y+50&&mx>=win_x+5&&mx<win_x+82){file_create_mode=1;file_create_len=0;file_create_name[0]=0;fm_message[0]=0;scene_dirty=1;}
                else if(window==2&&!file_create_mode&&my>=win_y+34&&my<win_y+50&&mx>=win_x+84&&mx<win_x+153){file_create_mode=2;file_create_len=0;file_create_name[0]=0;fm_message[0]=0;scene_dirty=1;}
                else if(window==2&&!file_create_mode&&!file_preview_open&&mx>=win_x+5&&mx<win_x+295&&my>=win_y+51&&my<win_y+170){int row=(my-win_y-51)/9;if(row<fm_count){activate_file_row(row);scene_dirty=1;}}
                else if(window && my>=win_y+2&&my<win_y+16&&mx>=win_x&&mx<win_x+280){dragging=1;drag_dx=mx-win_x;drag_dy=my-win_y;}
            }
            if(!(buttons&1)){was_down=0;dragging=0;}
            if(dragging){int nx=mx-drag_dx,ny=my-drag_dy;if(nx<0)nx=0;if(nx>20)nx=20;if(ny<0)ny=0;if(ny>20)ny=20;if(nx!=win_x||ny!=win_y){win_x=nx;win_y=ny;scene_dirty=1;}}
            if(scene_dirty)render();
            else if(mx!=shown_mouse_x||my!=shown_mouse_y)present_cursor_region(mx,my,shown_mouse_x,shown_mouse_y);
        }
        {uint8_t status=io_inb(0x64);if((status&1)&&!(status&0x20)){uint8_t sc=io_inb(0x60);int changed=0;if(sc==0x2A||sc==0x36)shift_pressed=1;else if(sc==0xAA||sc==0xB6)shift_pressed=0;else changed=key_input(sc);if(changed)render();}}
    }
}
