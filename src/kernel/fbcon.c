#include <onix/fbcon.h>
#include <onix/multiboot2.h>
#include <onix/types.h>
#include <onix/memory.h>
#include <onix/printk.h>

static u32 fb_width;
static u32 fb_height;
static u32 fb_pitch;
static u32 fb_bpp;
static u32 fb_type;
static u32 fb_addr;
static u32 fb_cols;
static u32 fb_rows;
static u32 fb_cur_x;
static u32 fb_cur_y;
static bool fb_parsed;
static bool fb_ready;

extern const u8 font8x8_basic[128][8];

static void fb_put_pixel(u32 x, u32 y, u32 color)
{
    if (x >= fb_width || y >= fb_height)
        return;

    u8 *p = (u8 *)(fb_addr + y * fb_pitch + x * (fb_bpp / 8));
    if (fb_bpp == 32)
    {
        *(u32 *)p = color;
    }
    else if (fb_bpp == 24)
    {
        p[0] = (color >> 16) & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = color & 0xFF;
    }
    else if (fb_bpp == 8)
    {
        *p = color & 0xFF;
    }
}

static void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color)
{
    for (u32 row = 0; row < h; row++)
    {
        for (u32 col = 0; col < w; col++)
        {
            fb_put_pixel(x + col, y + row, color);
        }
    }
}

static void fb_scroll()
{
    u32 row_bytes = fb_pitch;
    u8 *base = (u8 *)fb_addr;
    u32 copy_rows = fb_rows - 1;
    for (u32 row = 0; row < copy_rows; row++)
    {
        u8 *dst = base + row * row_bytes;
        u8 *src = base + (row + 1) * row_bytes;
        for (u32 i = 0; i < row_bytes; i++)
            dst[i] = src[i];
    }
    fb_fill_rect(0, (fb_rows - 1) * 8, fb_width, 8, 0);
}

static void fb_putchar(char ch)
{
    if (ch == '\n')
    {
        fb_cur_x = 0;
        fb_cur_y++;
        if (fb_cur_y >= fb_rows)
        {
            fb_scroll();
            fb_cur_y = fb_rows - 1;
        }
        return;
    }

    u8 c = (u8)ch;
    if (c >= 128)
        c = '?';

    const u8 *glyph = font8x8_basic[c];
    u32 color = 0xFFFFFF;
    u32 bg = 0x000000;

    for (u32 row = 0; row < 8; row++)
    {
        u8 bits = glyph[row];
        for (u32 col = 0; col < 8; col++)
        {
            u32 px = fb_cur_x * 8 + col;
            u32 py = fb_cur_y * 8 + row;
            fb_put_pixel(px, py, (bits & (1 << (7 - col))) ? color : bg);
        }
    }

    fb_cur_x++;
    if (fb_cur_x >= fb_cols)
    {
        fb_cur_x = 0;
        fb_cur_y++;
        if (fb_cur_y >= fb_rows)
        {
            fb_scroll();
            fb_cur_y = fb_rows - 1;
        }
    }
}

void fbcon_parse(multi_tag_framebuffer_t *tag)
{
    fb_parsed = false;
    fb_ready = false;

    if (tag->fb_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
        return;

    fb_addr = (u32)tag->addr;
    fb_pitch = tag->pitch;
    fb_width = tag->width;
    fb_height = tag->height;
    fb_bpp = tag->bpp;
    fb_type = tag->fb_type;

    if (fb_addr == 0 || fb_width == 0 || fb_height == 0 || fb_pitch == 0)
        return;
    if (fb_bpp != 8 && fb_bpp != 24 && fb_bpp != 32)
        return;

    fb_cols = fb_width / 8;
    fb_rows = fb_height / 8;
    if (fb_cols == 0 || fb_rows == 0)
        return;

    fb_cur_x = 0;
    fb_cur_y = 0;
    fb_parsed = true;
}

void fbcon_map()
{
    if (!fb_parsed)
        return;

    u32 size = fb_pitch * fb_height;
    map_mmio_range(fb_addr, size);
}

void fbcon_activate()
{
    if (!fb_parsed)
        return;

    fb_fill_rect(0, 0, fb_width, fb_height, 0);
    fb_ready = true;
    printk("FB %ux%u bpp %u pitch %u addr 0x%x\n",
           fb_width, fb_height, fb_bpp, fb_pitch, fb_addr);
}

bool fbcon_ready()
{
    return fb_ready;
}

void fbcon_write(const char *buf, int len)
{
    if (!fb_ready)
        return;

    for (int i = 0; i < len; i++)
        fb_putchar(buf[i]);
}
