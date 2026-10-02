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

#define XHCI_CMD_RING_SIZE 64
#define XHCI_EVT_RING_SIZE 64
#define XHCI_XFER_RING_PAGES 8
#define XHCI_XFER_RING_SIZE (XHCI_XFER_RING_PAGES * (PAGE_SIZE / sizeof(xhci_trb_t)))

// 扩展能力（xECP）位于 HCCPARAMS1[31:16]，单位为 dword
#define XHCI_HCCPARAMS1 0x10
#define XHCI_ECAP_ID_LEGACY 1 // USB Legacy Support 能力 ID

// USBLEGSUP / USBLEGCTLSTS（相对扩展能力首址）
#define XHCI_USBLEGSUP 0x00
#define XHCI_USBLEGCTLSTS 0x04
#define XHCI_LEGSUP_BIOS_OWNED (1u << 16)
#define XHCI_LEGSUP_OS_OWNED (1u << 24)
// 关闭固件 SMI 触发源（与 Linux xhci 一致）
#define XHCI_LEGCTL_DISABLE_SMI ((0x7u << 1) | (0xFFu << 5) | (0x7u << 17))
#define XHCI_LEGCTL_SMI_EVENTS (0x7u << 29)

// Intel PCH xHCI 端口路由寄存器（PCI 配置空间）
#define INTEL_VENDOR_ID 0x8086
#define INTEL_XUSB2PR 0xD0    // USB2 端口路由到 xHCI
#define INTEL_USB2PRM 0xD4    // USB2 端口路由掩码
#define INTEL_USB3_PSSEN 0xD8 // USB3 超速使能
#define INTEL_USB3PRM 0xDC    // USB3 端口路由掩码

#define XHCI_USBCMD_RUN (1u << 0)
#define XHCI_USBCMD_HCRST (1u << 1)
#define XHCI_USBCMD_INTE (1u << 2)

#define XHCI_USBSTS_HCH (1u << 0)
#define XHCI_USBSTS_HSE (1u << 2)
#define XHCI_USBSTS_EINT (1u << 3)
#define XHCI_USBSTS_PCD (1u << 4)

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

static u32 xhci_op(xhci_t *hc, u32 off)
{
    return hc->op_base + off;
}

static u32 xhci_rt(xhci_t *hc, u32 off)
{
    return hc->rt_base + off;
}

// BIOS/UEFI -> OS 控制权交接。
// 实体机上固件（SMM）持有 xHCI 控制器，必须在复位前请求所有权并关闭固件 SMI，
// 否则 OS 复位/运行控制器会与固件冲突，导致端口枚举不到设备。
// QEMU 无固件占用，所以缺少此步骤时只在实体机暴露。
static void xhci_takeover_bios(xhci_t *hc)
{
    u32 hccparams1 = minl(hc->membase + XHCI_HCCPARAMS1);
    u32 xecp = (hccparams1 >> 16) & 0xFFFF;
    if (!xecp)
    {
        LOGK("xHCI no extended capabilities\n");
        return;
    }

    u32 addr = hc->membase + (xecp << 2);
    while (true)
    {
        u32 cap = minl(addr);
        u8 id = cap & 0xFF;
        u8 next = (cap >> 8) & 0xFF;

        if (id == XHCI_ECAP_ID_LEGACY)
        {
            u32 legsup = minl(addr + XHCI_USBLEGSUP);
            if (legsup & XHCI_LEGSUP_BIOS_OWNED)
            {
                // 请求 OS 所有权，等待固件释放
                moutl(addr + XHCI_USBLEGSUP, legsup | XHCI_LEGSUP_OS_OWNED);
                int expires = timer_expire_jiffies(1000);
                while (minl(addr + XHCI_USBLEGSUP) & XHCI_LEGSUP_BIOS_OWNED)
                {
                    if (timer_is_expires(expires))
                    {
                        LOGK("xHCI BIOS handoff timeout, forcing ownership\n");
                        break;
                    }
                }
            }

            // 关闭固件 SMI 触发源，并写 1 清除 SMI 状态位
            u32 ctlsts = minl(addr + XHCI_USBLEGCTLSTS);
            ctlsts &= ~XHCI_LEGCTL_DISABLE_SMI;
            ctlsts |= XHCI_LEGCTL_SMI_EVENTS;
            moutl(addr + XHCI_USBLEGCTLSTS, ctlsts);

            LOGK("xHCI BIOS handoff complete\n");
            return;
        }

        if (!next)
            break;
        addr += (next << 2);
    }
}

// Intel PCH 默认把 USB2 端口路由给 EHCI，把 USB3 端口置于非超速模式。
// 需要将端口切换到 xHCI，否则插在 USB 口上的设备不会出现在 xHCI 的 PORTSC 上。
static void xhci_intel_port_route(pci_device_t *device)
{
    if (device->vendorid != INTEL_VENDOR_ID)
        return;

    u32 ports;

    ports = pci_inl(device->bus, device->dev, device->func, INTEL_USB3PRM);
    pci_outl(device->bus, device->dev, device->func, INTEL_USB3_PSSEN, ports);

    ports = pci_inl(device->bus, device->dev, device->func, INTEL_USB2PRM);
    pci_outl(device->bus, device->dev, device->func, INTEL_XUSB2PR, ports);

    LOGK("xHCI Intel ports routed to xHCI\n");
}

static void xhci_reset_controller(xhci_t *hc)
{
    u32 cmd = minl(xhci_op(hc, 0x00));
    cmd &= ~XHCI_USBCMD_RUN;
    moutl(xhci_op(hc, 0x00), cmd);

    int expires = timer_expire_jiffies(5000);
    while (!(minl(xhci_op(hc, 0x04)) & XHCI_USBSTS_HCH))
    {
        if (timer_is_expires(expires))
            break;
    }

    cmd |= XHCI_USBCMD_HCRST;
    moutl(xhci_op(hc, 0x00), cmd);

    expires = timer_expire_jiffies(5000);
    while (minl(xhci_op(hc, 0x00)) & XHCI_USBCMD_HCRST)
    {
        if (timer_is_expires(expires))
            break;
    }

    LOGK("xHCI controller reset complete\n");
}

static void xhci_init_rings(xhci_t *hc)
{
    hc->cmd_ring = (xhci_trb_t *)alloc_kpage(1);
    hc->event_ring = (xhci_trb_t *)alloc_kpage(1);
    hc->erst = (xhci_erst_t *)alloc_kpage(1);
    hc->input_ctx = (u8 *)alloc_kpage(1);
    memset(hc->cmd_ring, 0, PAGE_SIZE);
    memset(hc->event_ring, 0, PAGE_SIZE);
    memset(hc->erst, 0, PAGE_SIZE);
    memset(hc->input_ctx, 0, PAGE_SIZE);

    hc->cmd_enqueue = 0;
    hc->cmd_cycle = true;
    hc->event_dequeue = 0;
    hc->event_cycle = true;

    u64 cmd_ring_paddr = get_paddr((u32)hc->cmd_ring) | 1;
    moutl(xhci_op(hc, 0x18), (u32)cmd_ring_paddr);
    moutl(xhci_op(hc, 0x1C), (u32)(cmd_ring_paddr >> 32));

    hc->erst[0].ring_base_low = get_paddr((u32)hc->event_ring);
    hc->erst[0].ring_base_high = 0;
    hc->erst[0].ring_size = XHCI_EVT_RING_SIZE;

    moutl(xhci_rt(hc, 0x28), 1);
    u64 erst_paddr = get_paddr((u32)hc->erst);
    moutl(xhci_rt(hc, 0x30), (u32)erst_paddr);
    moutl(xhci_rt(hc, 0x34), (u32)(erst_paddr >> 32));

    u64 erdp = get_paddr((u32)hc->event_ring) | 1;
    moutl(xhci_rt(hc, 0x38), (u32)erdp);
    moutl(xhci_rt(hc, 0x3C), (u32)(erdp >> 32));

    moutl(xhci_rt(hc, 0x20), 1);

    hc->dcbaap = (u32 *)alloc_kpage(1);
    memset(hc->dcbaap, 0, PAGE_SIZE);

    // 实体机 xHCI 通常要求 Scratchpad Buffer Array：
    // 分配 max_scratchpad 个物理页，把各页物理地址填入 array，
    // 再将 array 物理地址写入 DCBAAP[0]（DCBA 槽 0 专用于 scratchpad）。
    if (hc->max_scratchpad > 0)
    {
        hc->scratchpad_array = (u64 *)alloc_kpage(1);
        memset(hc->scratchpad_array, 0, PAGE_SIZE);
        for (u32 i = 0; i < hc->max_scratchpad; i++)
        {
            u32 page = (u32)alloc_kpage(1);
            memset((void *)page, 0, PAGE_SIZE);
            hc->scratchpad_array[i] = get_paddr(page);
        }
        ((u64 *)hc->dcbaap)[0] = get_paddr((u32)hc->scratchpad_array);
    }

    moutl(xhci_op(hc, 0x30), get_paddr((u32)hc->dcbaap));
    moutl(xhci_op(hc, 0x34), 0);

    moutl(xhci_op(hc, 0x38), hc->max_slots);
}

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

    // 实体机必须先从固件接管控制器，再做端口路由，最后才复位/运行
    xhci_takeover_bios(hc);
    xhci_intel_port_route(device);

    xhci_reset_controller(hc);
    xhci_init_rings(hc);

    u32 cmd = minl(xhci_op(hc, 0x00));
    cmd |= XHCI_USBCMD_RUN | XHCI_USBCMD_INTE;
    moutl(xhci_op(hc, 0x00), cmd);

    // 控制器启动后端口连接状态需要时间稳定（实体机更明显）
    task_sleep(100);
}