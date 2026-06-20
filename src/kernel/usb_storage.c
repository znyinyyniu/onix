#include <onix/usb.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>
#include <onix/stdio.h>
#include <onix/net/types.h>


#define CBW_SIGNATURE 0x43425355
#define CSW_SIGNATURE 0x53425355

#define SCSI_READ_CAPACITY10 0x25
#define SCSI_READ10 0x28
#define SCSI_WRITE10 0x2A

typedef struct bot_cbw_t
{
    u32 signature;
    u32 tag;
    u32 data_len;
    u8 flags;
    u8 lun;
    u8 cb_len;
    u8 cb[16];
} _packed bot_cbw_t;

typedef struct bot_csw_t
{
    u32 signature;
    u32 tag;
    u32 residue;
    u8 status;
} _packed bot_csw_t;

typedef struct read_capacity10_t
{
    u32 lba;
    u32 block_len;
} _packed read_capacity10_t;

static usb_disk_t usb_disks[USB_DISK_NR];
static u32 bot_tag = 1;
static bot_cbw_t bot_cbw_buf;
static bot_csw_t bot_csw_buf;
static read_capacity10_t bot_cap_buf;

static err_t bot_command(xhci_device_t *xdev, u8 *cb, u8 cb_len, void *data, u32 data_len, bool in)
{
    bot_cbw_t *cbw = &bot_cbw_buf;
    memset(cbw, 0, sizeof(*cbw));
    cbw->signature = CBW_SIGNATURE;
    cbw->tag = bot_tag;
    cbw->data_len = data_len;
    cbw->flags = in ? 0x80 : 0;
    cbw->lun = 0;
    cbw->cb_len = cb_len;
    memcpy(cbw->cb, cb, cb_len);

    if (xhci_bulk_out(xdev, cbw, sizeof(*cbw)) < EOK)
    {
        LOGK("BOT CBW bulk out failed\n");
        return -EIO;
    }

    if (data_len > 0)
    {
        if (in)
        {
            if (xhci_bulk_in(xdev, data, data_len) < EOK)
            {
                LOGK("BOT data bulk in failed\n");
                return -EIO;
            }
        }
        else
        {
            if (xhci_bulk_out(xdev, data, data_len) < EOK)
            {
                LOGK("BOT data bulk out failed\n");
                return -EIO;
            }
        }
    }

    bot_csw_t *csw = &bot_csw_buf;
    if (xhci_bulk_in(xdev, csw, sizeof(*csw)) < EOK)
    {
        LOGK("BOT CSW bulk in failed\n");
        return -EIO;
    }

    if (csw->signature != CSW_SIGNATURE || csw->tag != bot_tag || csw->status != 0)
    {
        LOGK("BOT CSW error sig 0x%x tag %d status %d\n", csw->signature, csw->tag, csw->status);
        return -EIO;
    }

    bot_tag++;
    return EOK;
}

static err_t usb_storage_read_capacity(usb_disk_t *disk)
{
    u8 cb[16];
    read_capacity10_t *cap = &bot_cap_buf;

    memset(cb, 0, sizeof(cb));
    cb[0] = SCSI_READ_CAPACITY10;

    if (bot_command(disk->xdev, cb, 10, cap, sizeof(*cap), true) < EOK)
        return -EIO;

    u32 blocks = ntohl(cap->lba) + 1;
    u32 block_len = ntohl(cap->block_len);

    if (block_len != SECTOR_SIZE)
    {
        LOGK("USB disk sector size %d unsupported\n", block_len);
        return -EIO;
    }

    disk->total_lba = blocks;
    disk->sector_size = block_len;
    LOGK("USB disk capacity %d sectors\n", disk->total_lba);
    return EOK;
}

err_t usb_storage_read_sectors(usb_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    u8 cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = SCSI_READ10;
    cb[2] = (lba >> 24) & 0xFF;
    cb[3] = (lba >> 16) & 0xFF;
    cb[4] = (lba >> 8) & 0xFF;
    cb[5] = lba & 0xFF;
    cb[7] = (count >> 8) & 0xFF;
    cb[8] = count & 0xFF;

    return bot_command(disk->xdev, cb, 10, buf, count * disk->sector_size, true);
}

err_t usb_storage_write_sectors(usb_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    u8 cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = SCSI_WRITE10;
    cb[2] = (lba >> 24) & 0xFF;
    cb[3] = (lba >> 16) & 0xFF;
    cb[4] = (lba >> 8) & 0xFF;
    cb[5] = lba & 0xFF;
    cb[7] = (count >> 8) & 0xFF;
    cb[8] = count & 0xFF;

    return bot_command(disk->xdev, cb, 10, buf, count * disk->sector_size, false);
}

void usb_storage_init(void)
{
    LOGK("USB storage init...\n");

    int count = xhci_device_count();
    if (!count)
    {
        LOGK("No USB devices found\n");
        return;
    }

    int disk_idx = 0;
    for (int i = 0; i < count && disk_idx < USB_DISK_NR; i++)
    {
        xhci_device_t *xdev = xhci_get_device(i);
        if (!xdev || !xdev->mass_storage)
            continue;

        usb_disk_t *disk = &usb_disks[disk_idx];
        memset(disk, 0, sizeof(*disk));
        sprintf(disk->name, "usb%d", disk_idx);
        disk->xdev = xdev;
        disk->sector_size = SECTOR_SIZE;

        if (usb_storage_read_capacity(disk) < EOK)
            continue;

        usb_gpt_install(disk);
        disk_idx++;
    }

    usb_install();
    LOGK("USB storage init done, %d disk(s)\n", disk_idx);
}
