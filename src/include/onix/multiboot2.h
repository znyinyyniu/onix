#ifndef ONIX_MULTIBOOT2
#define ONIX_MULTIBOOT2

#include <onix/types.h>

// 进入内核时 eax 寄存器的值
#define MULTIBOOT2_MAGIC 0x36d76289

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS 4
#define MULTIBOOT_MEMORY_BADRAM 5

// multiboot tag
typedef struct multi_tag_t
{
    u32 type;
    u32 size;
} multi_tag_t;

// multiboot mmap entry
typedef struct multi_mmap_entry_t
{
    u64 addr;
    u64 len;
    u32 type;
    u32 zero;
} multi_mmap_entry_t;

// multiboot mmap tag
typedef struct multi_tag_mmap_t
{
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
    multi_mmap_entry_t entries[0];
} multi_tag_mmap_t;

// multiboot framebuffer tag（GRUB 在 MBI 中回传）
typedef struct multi_tag_framebuffer_t
{
    u32 type;
    u32 size;
    u64 addr;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bpp;
    u8 fb_type;
    u8 reserved[3];
} _packed multi_tag_framebuffer_t;

#endif
