#ifndef ONIX_GPT_H
#define ONIX_GPT_H

#include <onix/types.h>
#include <onix/ide.h>

#define GPT_SIGNATURE "EFI PART"
#define GPT_ENTRY_SIZE 128
#define GPT_MAX_ENTRIES 8

typedef struct gpt_header_t
{
    char signature[8];
    u32 revision;
    u32 header_size;
    u32 crc32;
    u32 reserved;
    u64 my_lba;
    u64 backup_lba;
    u64 first_usable;
    u64 last_usable;
    u8 disk_guid[16];
    u64 partition_lba;
    u32 partition_count;
    u32 partition_entry_size;
    u32 partition_crc32;
} _packed gpt_header_t;

typedef struct gpt_entry_t
{
    u8 type_guid[16];
    u8 unique_guid[16];
    u64 first_lba;
    u64 last_lba;
    u64 attributes;
    u16 name[36];
} _packed gpt_entry_t;

typedef struct gpt_part_info_t
{
    u32 index;
    u64 first_lba;
    u64 last_lba;
    u32 sector_count;
    char name[72];
} gpt_part_info_t;

typedef int (*gpt_block_read_t)(void *disk, void *buf, u8 count, idx_t lba);

bool gpt_validate_header(gpt_header_t *header);
int gpt_read_partitions(void *disk, gpt_block_read_t read, gpt_part_info_t *parts, int max_parts);

#endif
