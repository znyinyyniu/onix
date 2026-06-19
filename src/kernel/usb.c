#include <onix/usb.h>
#include <onix/device.h>
#include <onix/stdio.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static usb_disk_t *usb_disk_list[USB_DISK_NR];
static int usb_disk_count;

static int usb_disk_block_read(void *disk, void *buf, u8 count, idx_t lba)
{
    return usb_storage_read_sectors((usb_disk_t *)disk, buf, count, lba);
}

int usb_disk_read(usb_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
    return usb_storage_read_sectors(disk, buf, count, lba);
}

int usb_disk_write(usb_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
    return usb_storage_write_sectors(disk, buf, count, lba);
}

static int usb_disk_ioctl(usb_disk_t *disk, int cmd, void *args, int flags)
{
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return 0;
    case DEV_CMD_SECTOR_COUNT:
        return disk->total_lba;
    case DEV_CMD_SECTOR_SIZE:
        return disk->sector_size;
    default:
        panic("device command %d can't recognize!!!", cmd);
        break;
    }
}

static int usb_part_ioctl(usb_part_t *part, int cmd, void *args, int flags)
{
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return part->start;
    case DEV_CMD_SECTOR_COUNT:
        return part->count;
    case DEV_CMD_SECTOR_SIZE:
        return part->disk->sector_size;
    default:
        panic("device command %d can't recognize!!!", cmd);
        break;
    }
}

int usb_part_read(usb_part_t *part, void *buf, u8 count, idx_t lba)
{
    return usb_disk_read(part->disk, buf, count, part->start + lba);
}

int usb_part_write(usb_part_t *part, void *buf, u8 count, idx_t lba)
{
    return usb_disk_write(part->disk, buf, count, part->start + lba);
}

void usb_gpt_install(usb_disk_t *disk)
{
    gpt_part_info_t parts[USB_PART_NR];
    int count = gpt_read_partitions(disk, usb_disk_block_read, parts, USB_PART_NR);
    if (!count)
    {
        LOGK("USB disk %s: no GPT partitions\n", disk->name);
        return;
    }

    for (int i = 0; i < count && i < USB_PART_NR; i++)
    {
        usb_part_t *part = &disk->parts[i];
        gpt_part_info_t *info = &parts[i];
        sprintf(part->name, "%sp%d", disk->name, info->index + 1);
        part->disk = disk;
        part->start = (u32)info->first_lba;
        part->count = info->sector_count;
        part->index = info->index;
        LOGK("USB part %s start %d count %d\n", part->name, part->start, part->count);
    }

    if (usb_disk_count < USB_DISK_NR)
        usb_disk_list[usb_disk_count++] = disk;
}

void usb_install(void)
{
    for (int d = 0; d < usb_disk_count; d++)
    {
        usb_disk_t *disk = usb_disk_list[d];
        if (!disk->total_lba)
            continue;

        dev_t pdev = device_install(
            DEV_BLOCK, DEV_USB_DISK, disk, disk->name, 0,
            usb_disk_ioctl, usb_disk_read, usb_disk_write);

        for (int i = 0; i < USB_PART_NR; i++)
        {
            usb_part_t *part = &disk->parts[i];
            if (!part->count)
                continue;
            device_install(
                DEV_BLOCK, DEV_USB_PART, part, part->name, pdev,
                usb_part_ioctl, usb_part_read, usb_part_write);
        }
    }
}
