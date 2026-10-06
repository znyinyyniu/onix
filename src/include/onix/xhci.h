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

// 已枚举的 USB 设备
typedef struct xhci_device_t
{
    u8 slot;           // xHCI 槽位号，Enable Slot 分配，用于索引上下文和门铃
    u8 address;        // USB 设备地址，Address Device 后由控制器分配
    u8 port;           // 根集线器端口号（从 1 起）
    u8 speed;          // 端口速率：1 Full、2 Low、3 High、4 Super
    u16 ep0_mps;       // 端点 0 最大包长
    u8 ep_in;          // 批量 IN 端点地址（含方向位）
    u8 ep_out;         // 批量 OUT 端点地址
    u16 ep_in_mps;     // 批量 IN 端点最大包长
    u16 ep_out_mps;    // 批量 OUT 端点最大包长
    bool mass_storage; // 是否为 Bulk-Only 大容量存储设备
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