#include <onix/gpt.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/memory.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

bool gpt_validate_header(gpt_header_t *header)
{
    if (memcmp(header->signature, GPT_SIGNATURE, 8) != 0)
        return false;
    if (header->header_size < 92)
        return false;
    if (header->partition_entry_size < sizeof(gpt_entry_t))
        return false;
    if (header->partition_count == 0)
        return false;
    return true;
}

static void gpt_copy_name(gpt_part_info_t *part, gpt_entry_t *entry)
{
    int j = 0;
    for (int i = 0; i < 36 && j < (int)sizeof(part->name) - 1; i++)
    {
        u16 c = entry->name[i];
        if (!c)
            break;
        if (c < 0x80)
            part->name[j++] = (char)c;
    }
    part->name[j] = 0;
}

int gpt_read_partitions(void *disk, gpt_block_read_t read, gpt_part_info_t *parts, int max_parts)
{
    u8 *sector = (u8 *)alloc_kpage(1);
    if (read(disk, sector, 1, 1) != EOK)
    {
        free_kpage((u32)sector, 1);
        return 0;
    }

    gpt_header_t *header = (gpt_header_t *)sector;
    if (!gpt_validate_header(header))
    {
        free_kpage((u32)sector, 1);
        return 0;
    }

    u32 count = header->partition_count;
    u32 entry_size = header->partition_entry_size;
    u64 entry_lba = header->partition_lba;
    if (count > GPT_MAX_ENTRIES)
        count = GPT_MAX_ENTRIES;

    u32 table_bytes = count * entry_size;
    u32 table_sectors = (table_bytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
    u8 *table = (u8 *)alloc_kpage((table_sectors + 4095) / 4096);
    if (!table)
    {
        free_kpage((u32)sector, 1);
        return 0;
    }

    for (u32 s = 0; s < table_sectors; s++)
    {
        if (read(disk, table + s * SECTOR_SIZE, 1, entry_lba + s) != EOK)
        {
            free_kpage((u32)table, (table_sectors + 4095) / 4096);
            free_kpage((u32)sector, 1);
            return 0;
        }
    }

    int found = 0;
    for (u32 i = 0; i < count && found < max_parts; i++)
    {
        gpt_entry_t *entry = (gpt_entry_t *)(table + i * entry_size);
        bool empty = true;
        for (int b = 0; b < 16; b++)
        {
            if (entry->type_guid[b])
            {
                empty = false;
                break;
            }
        }
        if (empty)
            continue;

        gpt_part_info_t *part = &parts[found];
        part->index = i;
        part->first_lba = entry->first_lba;
        part->last_lba = entry->last_lba;
        part->sector_count = (u32)(entry->last_lba - entry->first_lba + 1);
        gpt_copy_name(part, entry);

        LOGK("GPT part %d name %s start %d count %d\n",
             part->index, part->name, (u32)part->first_lba, part->sector_count);
        found++;
    }

    free_kpage((u32)table, (table_sectors + 4095) / 4096);
    free_kpage((u32)sector, 1);
    return found;
}
