#include <onix/pci.h>
#include <onix/xhci.h>
#include <onix/mio.h>
#include <onix/memory.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/printk.h>
#include <onix/errno.h>
#include <onix/timer.h>
#include <onix/task.h>

typedef struct xhci_trb_t
{
    u32 parameter_low;
    u32 parameter_high;
    u32 status;
    u32 control;
} _packed xhci_trb_t;

typedef struct xhci_erst_t
{
    u32 ring_base_low;
    u32 ring_base_high;
    u16 ring_size;
    u16 reserved;
} _packed xhci_erst_t;

// xHCI 主机控制器全局状态（MMIO 基址、Ring、设备上下文）
typedef struct xhci_t
{
    // --- MMIO 寄存器窗口（物理地址，经 map_area 后可 minl/moutl 访问）---
    u32 membase;  // MEM BAR 基址
    u32 op_base;  // Operational Registers 基址
    u32 db_base;  // Doorbell Array 基址
    u32 rt_base;  // Runtime Registers 基址

    // --- 从 Capability 寄存器读出的控制器能力 ---
    u8 caplen;       // Capability 区长度（字节）
    u8 max_slots;    // 最大 Device Slot 数
    u8 max_ports;    // 物理 USB 端口数
    u8 ctx_size;     // Device/Endpoint Context 大小（32 或 64 字节）
    u32 page_size;   // xHCI 要求的内存页大小

    // --- Command Ring：驱动写 TRB，硬件读，管理命令（Enable Slot 等）---
    xhci_trb_t *cmd_ring;
    u32 cmd_enqueue; // 下一个待写入的 TRB 索引
    bool cmd_cycle;  // Command Ring 当前 cycle bit

    // --- Event Ring：硬件写 TRB，驱动读，命令/传输完成通知 ---
    xhci_trb_t *event_ring;
    u32 event_dequeue; // 下一个待处理的 Event TRB 索引
    bool event_cycle;  // Event Ring 当前 cycle bit
    xhci_erst_t *erst; // Event Ring Segment Table（描述 event_ring 物理地址与长度）

    // --- 已枚举 USB 设备（供上层 usb_storage 等使用）---
    xhci_device_t devices[XHCI_MAX_DEVICES];
    int device_count;

    // --- Scratchpad Buffer（实体机 xHCI 常需，物理页地址填入 dcbaap[0]）---
    u32 max_scratchpad;
    u64 *scratchpad_array;

    // --- Device Context：每个 slot 的硬件上下文与端点 Transfer Ring ---
    u8 *input_ctx;                              // 提交 Address/Configure 命令用的 Input Context
    u32 *dcbaap;                                // Device Context Base Address Array
    u8 *dev_ctx[XHCI_MAX_SLOTS + 1];            // 各 slot 的 Device Context（含 Endpoint Context）
    xhci_trb_t *ep_rings[XHCI_MAX_SLOTS + 1][32]; // 各 slot 各端点的 Transfer Ring
    u32 ep_enqueue[XHCI_MAX_SLOTS + 1][32];   // Transfer Ring 写指针
    bool ep_cycle[XHCI_MAX_SLOTS + 1][32];      // Transfer Ring 当前 cycle bit
} xhci_t;

static xhci_t xhci;

void xhci_init(void)
{
    // 1. 找到xHCI controller
    LOGK("xHCI init...\n");

    pci_device_t *device = pci_find_device_by_class(PCI_CLASS_SERIAL_USB_XHCI);
    if (!device)
    {
        USBLOG("xHCI controller not found\n");
        return;
    }
    LOGK("xHCI controller found: %s\n", pci_classname(device->classcode));

    // 2. xHCI 主机控制器全局状态（MMIO 基址、Ring、设备上下文）
    xhci_t *hc = &xhci;
    memset(hc, 0, sizeof(*hc));

    pci_bar_t membar;
    err_t ret = pci_map_mem_bar(device, &membar);
    if (ret != EOK || membar.iobase == 0)
    {
        USBLOG("xHCI MMIO BAR not available\n");
        return;
    }

    LOGK("xHCI membase 0x%x size 0x%x\n", membar.iobase, membar.size);

    hc->membase = membar.iobase;
    map_area(membar.iobase, membar.size);

    hc->caplen = minb(hc->membase);
    hc->op_base = hc->membase + hc->caplen;
    hc->db_base = hc->membase + minl(hc->membase + 0x14);
    hc->rt_base = hc->membase + minl(hc->membase + 0x18);

    u32 hcsparams1 = minl(hc->membase + 0x04);
    hc->max_slots = hcsparams1 & 0xFF;
    hc->max_ports = (hcsparams1 >> 24) & 0xFF;

    u32 hccparams1 = minl(hc->membase + 0x10);
    hc->ctx_size = (hccparams1 & (1u << 2)) ? 64 : 32;

    // HCSPARAMS2(cap 偏移 0x08)：Max Scratchpad Buffers = Hi[31:27] | Lo[25:21]
    u32 hcsparams2 = minl(hc->membase + 0x08);
    u32 sp_hi = (hcsparams2 >> 21) & 0x1F;
    u32 sp_lo = (hcsparams2 >> 27) & 0x1F;
    hc->max_scratchpad = (sp_hi << 5) | sp_lo;

    u32 pagesize = minl(hc->op_base + 0x08);
    hc->page_size = 1u << (pagesize + 12);

    LOGK("xHCI slots %d ports %d ctx %d scratch %d\n",
           hc->max_slots, hc->max_ports, hc->ctx_size, hc->max_scratchpad);
}