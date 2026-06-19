#ifndef ONIX_USB_H
#define ONIX_USB_H

#include <onix/types.h>
#include <onix/ide.h>
#include <onix/gpt.h>
#include <onix/xhci.h>

#define USB_DISK_NR 1
#define USB_PART_NR GPT_MAX_ENTRIES

typedef struct usb_part_t
{
    char name[16];
    struct usb_disk_t *disk;
    u32 start;
    u32 count;
    u32 index;
} usb_part_t;

typedef struct usb_disk_t
{
    char name[16];
    xhci_device_t *xdev;
    u32 total_lba;
    u32 sector_size;
    usb_part_t parts[USB_PART_NR];
} usb_disk_t;

int usb_disk_read(usb_disk_t *disk, void *buf, u8 count, idx_t lba);
int usb_disk_write(usb_disk_t *disk, void *buf, u8 count, idx_t lba);
int usb_part_read(usb_part_t *part, void *buf, u8 count, idx_t lba);
int usb_part_write(usb_part_t *part, void *buf, u8 count, idx_t lba);

void usb_gpt_install(usb_disk_t *disk);
void usb_install(void);

err_t usb_storage_read_sectors(usb_disk_t *disk, void *buf, u8 count, idx_t lba);
err_t usb_storage_write_sectors(usb_disk_t *disk, void *buf, u8 count, idx_t lba);
void usb_storage_init(void);

#endif
