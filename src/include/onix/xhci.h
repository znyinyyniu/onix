#ifndef ONIX_XHCI_H
#define ONIX_XHCI_H

#include <onix/types.h>

#define XHCI_MAX_DEVICES 4
#define XHCI_MAX_PORTS 16 // 实体机 xHCI 端口数可达 15，需覆盖全部端口
#define XHCI_MAX_SLOTS 32 // slot 由控制器递增分配，按 slot 索引的数组须与之对齐

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





#endif