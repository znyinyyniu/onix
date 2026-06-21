#ifndef ONIX_XHCI_H
#define ONIX_XHCI_H

#include <onix/types.h>

#define XHCI_MAX_DEVICES 4
#define XHCI_MAX_PORTS 16 // 实体机 xHCI 端口数可达 15，需覆盖全部端口
#define XHCI_MAX_SLOTS 32 // slot 由控制器递增分配，按 slot 索引的数组须与之对齐

// USB Mass Storage Bulk-Only interface
#define USB_CLASS_MASS_STORAGE 0x08
#define USB_SUBCLASS_SCSI 0x06
#define USB_PROTO_BOT 0x50

typedef struct xhci_device_t
{
    u8 slot;
    u8 address;
    u8 port;
    u8 speed;
    u16 ep0_mps;
    u8 ep_in;
    u8 ep_out;
    u16 ep_in_mps;
    u16 ep_out_mps;
    bool mass_storage;
} xhci_device_t;

typedef struct usb_setup_t
{
    u8 request_type;
    u8 request;
    u16 value;
    u16 index;
    u16 length;
} _packed usb_setup_t;

void xhci_init(void);

int xhci_device_count(void);
xhci_device_t *xhci_get_device(int idx);

err_t xhci_control_transfer(xhci_device_t *dev, usb_setup_t *setup, void *data, int len);
err_t xhci_bulk_in(xhci_device_t *dev, void *buf, u32 len);
err_t xhci_bulk_out(xhci_device_t *dev, void *buf, u32 len);

#endif
