#include <onix/xhci.h>
#include <onix/pci.h>
#include <onix/mio.h>
#include <onix/memory.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/printk.h>
#include <onix/errno.h>
#include <onix/timer.h>
#include <onix/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define XHCI_CMD_RING_SIZE 64
#define XHCI_EVT_RING_SIZE 64
#define XHCI_XFER_RING_PAGES 8
#define XHCI_XFER_RING_SIZE (XHCI_XFER_RING_PAGES * (PAGE_SIZE / sizeof(xhci_trb_t)))

#define TRB_CYCLE (1u << 0)
#define TRB_IOC (1u << 5)
#define TRB_IDT (1u << 6)
#define TRB_ENT (1u << 1) /* Link TRB: toggle cycle */
#define TRB_DIR_IN (1u << 16)
#define TRB_LEN(p) ((p) & 0x1ffff)
#define TRB_TX_DATA_IN (3u << 16)
#define TRB_TX_DATA_OUT (2u << 16)

#define TRB_TYPE_NORMAL 1
#define TRB_TYPE_SETUP 2
#define TRB_TYPE_DATA 3
#define TRB_TYPE_STATUS 4
#define TRB_TYPE_LINK 6
#define TRB_TYPE_NOOP 8
#define TRB_TYPE_ENABLE_SLOT 9
#define TRB_TYPE_ADDR_DEV 11
#define TRB_TYPE_CONF_EP 12
#define TRB_TYPE_RESET_EP 14

#define TRB_TYPE_EVT_TRANSFER 32
#define TRB_TYPE_EVT_CC 33
#define TRB_TYPE_EVT_PSC 34

#define CC_SUCCESS 1

#define XHCI_PORTSC_CCS (1u << 0)
#define XHCI_PORTSC_PED (1u << 1)
#define XHCI_PORTSC_PR (1u << 4)
#define XHCI_PORTSC_PP (1u << 9)
#define XHCI_PORTSC_SPEED_SHIFT 20
#define XHCI_PORTSC_SPEED_MASK 0xF

#define XHCI_USBCMD_RUN (1u << 0)
#define XHCI_USBCMD_HCRST (1u << 1)
#define XHCI_USBCMD_INTE (1u << 2)

#define XHCI_USBSTS_HCH (1u << 0)
#define XHCI_USBSTS_HSE (1u << 2)
#define XHCI_USBSTS_EINT (1u << 3)
#define XHCI_USBSTS_PCD (1u << 4)

#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_DT_DEVICE 0x01
#define USB_DT_CONFIG 0x02

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

typedef struct usb_device_desc_t
{
    u8 len;
    u8 type;
    u16 version;
    u8 device_class;
    u8 device_subclass;
    u8 device_protocol;
    u8 max_packet;
    u16 vendor;
    u16 product;
    u16 release;
    u8 manufacturer;
    u8 product_idx;
    u8 serial;
    u8 num_configs;
} _packed usb_device_desc_t;

typedef struct usb_config_desc_t
{
    u8 len;
    u8 type;
    u16 total_len;
    u8 num_interfaces;
    u8 config_value;
    u8 config_idx;
    u8 attributes;
    u8 max_power;
} _packed usb_config_desc_t;

typedef struct usb_interface_desc_t
{
    u8 len;
    u8 type;
    u8 num;
    u8 alt;
    u8 endpoints;
    u8 class;
    u8 subclass;
    u8 protocol;
} _packed usb_interface_desc_t;

typedef struct usb_endpoint_desc_t
{
    u8 len;
    u8 type;
    u8 addr;
    u8 attr;
    u16 max_packet;
    u8 interval;
} _packed usb_endpoint_desc_t;

typedef struct xhci_t
{
    u32 membase;
    u32 op_base;
    u32 db_base;
    u32 rt_base;
    u8 caplen;
    u8 max_slots;
    u8 max_ports;
    u8 ctx_size;
    u32 page_size;

    xhci_trb_t *cmd_ring;
    u32 cmd_enqueue;
    bool cmd_cycle;

    xhci_trb_t *event_ring;
    u32 event_dequeue;
    bool event_cycle;
    xhci_erst_t *erst;

    xhci_device_t devices[XHCI_MAX_DEVICES];
    int device_count;

    u8 *input_ctx;
    u32 *dcbaap;
    u8 *dev_ctx[XHCI_MAX_DEVICES + 1];
    xhci_trb_t *ep_rings[XHCI_MAX_DEVICES + 1][32];
    u32 ep_enqueue[XHCI_MAX_DEVICES + 1][32];
    bool ep_cycle[XHCI_MAX_DEVICES + 1][32];
} xhci_t;

static xhci_t xhci;

static u32 xhci_op(xhci_t *hc, u32 off)
{
    return hc->op_base + off;
}

static u32 xhci_port(xhci_t *hc, int port)
{
    return hc->op_base + 0x400 + (port - 1) * 0x10;
}

static u32 xhci_db(xhci_t *hc, u8 slot)
{
    return hc->db_base + slot * 4;
}

static u32 xhci_rt(xhci_t *hc, u32 off)
{
    return hc->rt_base + off;
}

static u32 trb_type(u32 control, u32 type)
{
    return (control & ~0xFC00u) | (type << 10);
}

static void trb_set_addr(xhci_trb_t *trb, u32 addr)
{
    trb->parameter_low = addr;
    trb->parameter_high = 0;
}

static u32 trb_event_type(xhci_trb_t *trb)
{
    return (trb->control >> 10) & 0x3F;
}

static u32 trb_completion_code(xhci_trb_t *trb)
{
    return (trb->status >> 24) & 0xFF;
}

static u8 trb_event_slot(xhci_trb_t *trb)
{
    return (trb->control >> 24) & 0xFF;
}

static u8 trb_event_ep(xhci_trb_t *trb)
{
    return (trb->control >> 16) & 0x1F;
}

static void xhci_kick_cmd(xhci_t *hc)
{
    u64 crcr = get_paddr((u32)hc->cmd_ring) | (hc->cmd_cycle ? 1u : 0u);
    moutl(xhci_op(hc, 0x18), (u32)crcr);
    moutl(xhci_op(hc, 0x1C), (u32)(crcr >> 32));
}

static u8 xhci_db_target(u8 ep_idx)
{
    return ep_idx;
}

static void xhci_ring_doorbell(xhci_t *hc, u8 slot, u8 target)
{
    moutl(xhci_db(hc, slot), target);
}

static xhci_trb_t *xhci_next_cmd(xhci_t *hc)
{
    xhci_trb_t *trb = &hc->cmd_ring[hc->cmd_enqueue % XHCI_CMD_RING_SIZE];
    if (hc->cmd_enqueue % XHCI_CMD_RING_SIZE == XHCI_CMD_RING_SIZE - 1)
    {
        trb->control = trb_type(0, TRB_TYPE_LINK) | TRB_CYCLE;
        trb_set_addr(trb, get_paddr((u32)hc->cmd_ring));
        trb->status = 0;
        hc->cmd_cycle = !hc->cmd_cycle;
        hc->cmd_enqueue++;
        trb = &hc->cmd_ring[hc->cmd_enqueue % XHCI_CMD_RING_SIZE];
    }
    return trb;
}

static void xhci_post_cmd(xhci_t *hc)
{
    hc->cmd_enqueue++;
    moutl(xhci_db(hc, 0), 0);
}

static void xhci_set_ep_dequeue(xhci_t *hc, u8 slot, u8 ep_idx, u32 trb_idx, bool cycle)
{
    u8 *epctx = hc->dev_ctx[slot] + hc->ctx_size * (ep_idx + 1);
    u32 ring = get_paddr((u32)hc->ep_rings[slot][ep_idx]);
    u64 dq = ring + (u64)trb_idx * sizeof(xhci_trb_t);
    if (cycle)
        dq |= 1;
    ((u32 *)epctx)[2] = (u32)dq;
    ((u32 *)epctx)[3] = (u32)(dq >> 32);
}

static err_t xhci_wait_event(xhci_t *hc, u8 expect_type, u8 *slot_out, u8 *ep_out, int timeout_ms)
{
    int expires = timer_expire_jiffies(timeout_ms);
    while (true)
    {
        if (timeout_ms > 0 && timer_is_expires(expires))
            return -ETIME;

        xhci_trb_t *ev = &hc->event_ring[hc->event_dequeue % XHCI_EVT_RING_SIZE];
        bool cycle = (ev->control & TRB_CYCLE) != 0;
        if (cycle != hc->event_cycle)
        {
            task_sleep(1);
            if (timeout_ms > 0 && timer_is_expires(expires))
                return -ETIME;
            continue;
        }

        u8 type = trb_event_type(ev);
        u8 cc = trb_completion_code(ev);
        u8 slot = trb_event_slot(ev);
        u8 ep = trb_event_ep(ev);

        hc->event_dequeue++;
        if (hc->event_dequeue % XHCI_EVT_RING_SIZE == 0)
            hc->event_cycle = !hc->event_cycle;

        u64 erdp = get_paddr((u32)&hc->event_ring[hc->event_dequeue % XHCI_EVT_RING_SIZE]);
        erdp |= (hc->event_cycle ? 1u : 0u);
        moutl(xhci_rt(hc, 0x38), (u32)erdp);
        moutl(xhci_rt(hc, 0x3C), (u32)(erdp >> 32));

        if (type == TRB_TYPE_EVT_PSC)
        {
            task_sleep(1);
            if (timeout_ms > 0 && timer_is_expires(expires))
                return -ETIME;
            continue;
        }

        if (cc != CC_SUCCESS)
        {
            LOGK("xHCI event cc %d type %d\n", cc, type);
            return -EIO;
        }

        if (expect_type && type != expect_type)
        {
            task_sleep(1);
            if (timeout_ms > 0 && timer_is_expires(expires))
                return -ETIME;
            continue;
        }

        if (slot_out)
            *slot_out = slot;
        if (ep_out)
            *ep_out = ep;
        return EOK;
    }
}

static xhci_trb_t *xhci_next_xfer(xhci_t *hc, u8 slot, u8 ep_idx)
{
    u32 *enq = &hc->ep_enqueue[slot][ep_idx];
    xhci_trb_t *ring = hc->ep_rings[slot][ep_idx];

    if (*enq % XHCI_XFER_RING_SIZE == XHCI_XFER_RING_SIZE - 1)
    {
        xhci_trb_t *link = &ring[*enq % XHCI_XFER_RING_SIZE];
        link->control = trb_type(TRB_ENT, TRB_TYPE_LINK);
        if (hc->ep_cycle[slot][ep_idx])
            link->control |= TRB_CYCLE;
        trb_set_addr(link, get_paddr((u32)ring));
        link->status = 0;
        hc->ep_cycle[slot][ep_idx] = !hc->ep_cycle[slot][ep_idx];
        (*enq)++;
    }

    xhci_trb_t *trb = &ring[*enq % XHCI_XFER_RING_SIZE];
    (*enq)++;
    return trb;
}

static void xhci_post_xfer(xhci_t *hc, u8 slot, u8 ep_idx)
{
    xhci_ring_doorbell(hc, slot, xhci_db_target(ep_idx));
}

static u8 xhci_ep_id(u8 ep_addr)
{
    u8 num = ep_addr & 0x0F;
    if (num == 0)
        return 1;
    return (u8)(2 * num + ((ep_addr & 0x80) ? 1 : 0));
}

static void input_ctx_add(u8 *input, u32 flags)
{
    ((u32 *)input)[1] = flags;
}

static void slot_ctx_set_dword(u8 *input, int ctx_size, int dword, u32 val)
{
    ((u32 *)(input + ctx_size))[dword] = val;
}

static void ep_ctx_set_dword(u8 *input, int ctx_size, int ep_id, int dword, u32 val)
{
    ((u32 *)(input + ctx_size * (ep_id + 1)))[dword] = val;
}

static void xhci_init_ep_ring(xhci_t *hc, u8 slot, u8 ep_idx)
{
    hc->ep_rings[slot][ep_idx] = (xhci_trb_t *)alloc_kpage(XHCI_XFER_RING_PAGES);
    memset(hc->ep_rings[slot][ep_idx], 0, XHCI_XFER_RING_PAGES * PAGE_SIZE);
    hc->ep_enqueue[slot][ep_idx] = 0;
    hc->ep_cycle[slot][ep_idx] = true;
}

static void xhci_set_ep_ctx(xhci_t *hc, u8 *input, u8 ep_id, u8 type, u16 mps, xhci_trb_t *ring, bool cycle)
{
    u64 dequeue = get_paddr((u32)ring) | (cycle ? 1u : 0u);
    ep_ctx_set_dword(input, hc->ctx_size, ep_id, 0, (3u << 1));
    u32 ep_info2 = (type << 3) | ((u32)mps << 16);
    ep_ctx_set_dword(input, hc->ctx_size, ep_id, 1, ep_info2);
    ep_ctx_set_dword(input, hc->ctx_size, ep_id, 2, (u32)dequeue);
    ep_ctx_set_dword(input, hc->ctx_size, ep_id, 3, (u32)(dequeue >> 32));
    ep_ctx_set_dword(input, hc->ctx_size, ep_id, 4, 8);
}

static void xhci_set_slot_ctx(xhci_t *hc, u8 *input, u8 port, u8 speed, u8 num_ctx, u8 address)
{
    u32 dev_info = ((u32)num_ctx << 27) | ((u32)speed << 20);
    slot_ctx_set_dword(input, hc->ctx_size, 0, dev_info);
    slot_ctx_set_dword(input, hc->ctx_size, 1, (u32)port << 16);
    if (address)
        slot_ctx_set_dword(input, hc->ctx_size, 3, address);
}

static u8 xhci_ep0_mps(u8 speed)
{
    if (speed == 2)
        return 64;
    return 8;
}

static err_t xhci_cmd_enable_slot(xhci_t *hc, u8 *slot_out)
{
    xhci_trb_t *trb = xhci_next_cmd(hc);
    memset(trb, 0, sizeof(*trb));
    trb->control = trb_type(TRB_CYCLE, TRB_TYPE_ENABLE_SLOT);
    if (hc->cmd_cycle)
        trb->control |= TRB_CYCLE;
    xhci_post_cmd(hc);

    u8 slot = 0;
    err_t ret = xhci_wait_event(hc, TRB_TYPE_EVT_CC, &slot, NULL, 5000);
    if (ret < EOK)
        return ret;
    *slot_out = slot;
    return EOK;
}

static err_t xhci_cmd_address_device(xhci_t *hc, u8 slot, u8 *input_ctx, bool bsr)
{
    xhci_trb_t *trb = xhci_next_cmd(hc);
    memset(trb, 0, sizeof(*trb));
    trb->control = trb_type(TRB_CYCLE, TRB_TYPE_ADDR_DEV);
    if (hc->cmd_cycle)
        trb->control |= TRB_CYCLE;
    if (bsr)
        trb->control |= (1u << 9);
    trb->control |= (u32)slot << 24;
    trb_set_addr(trb, get_paddr((u32)input_ctx));
    xhci_post_cmd(hc);
    return xhci_wait_event(hc, TRB_TYPE_EVT_CC, NULL, NULL, 5000);
}

static err_t xhci_cmd_reset_ep(xhci_t *hc, u8 slot, u8 ep_id)
{
    xhci_trb_t *trb = xhci_next_cmd(hc);
    memset(trb, 0, sizeof(*trb));
    trb->control = trb_type(TRB_CYCLE, TRB_TYPE_RESET_EP);
    if (hc->cmd_cycle)
        trb->control |= TRB_CYCLE;
    trb->control |= (u32)slot << 24;
    trb->status = (u32)ep_id << 16;
    xhci_post_cmd(hc);
    return xhci_wait_event(hc, TRB_TYPE_EVT_CC, NULL, NULL, 5000);
}

static err_t xhci_cmd_configure_ep(xhci_t *hc, u8 slot, u8 *input_ctx)
{
    xhci_trb_t *trb = xhci_next_cmd(hc);
    memset(trb, 0, sizeof(*trb));
    trb->control = trb_type(TRB_CYCLE, TRB_TYPE_CONF_EP);
    if (hc->cmd_cycle)
        trb->control |= TRB_CYCLE;
    trb->control |= (u32)slot << 24;
    trb_set_addr(trb, get_paddr((u32)input_ctx));
    xhci_post_cmd(hc);
    return xhci_wait_event(hc, TRB_TYPE_EVT_CC, NULL, NULL, 5000);
}

static err_t xhci_do_control(xhci_t *hc, xhci_device_t *dev, usb_setup_t *setup, void *data, int len, bool in)
{
    u8 slot = dev->slot;
    u8 ep_idx = 1;
    xhci_trb_t *setup_trb = xhci_next_xfer(hc, slot, ep_idx);
    xhci_trb_t *status_trb = NULL;
    xhci_trb_t *data_trb = NULL;

    memset(setup_trb, 0, sizeof(*setup_trb));
    setup_trb->control = trb_type(TRB_CYCLE | TRB_IDT, TRB_TYPE_SETUP);
    if (hc->ep_cycle[slot][ep_idx])
        setup_trb->control |= TRB_CYCLE;
    if (len > 0)
        setup_trb->control |= in ? TRB_TX_DATA_IN : TRB_TX_DATA_OUT;
    setup_trb->status = TRB_LEN(8);
    memcpy(&setup_trb->parameter_low, setup, 8);

    if (len > 0)
    {
        data_trb = xhci_next_xfer(hc, slot, ep_idx);
        memset(data_trb, 0, sizeof(*data_trb));
        data_trb->control = trb_type(TRB_CYCLE, TRB_TYPE_DATA);
        if (hc->ep_cycle[slot][ep_idx])
            data_trb->control |= TRB_CYCLE;
        if (in)
            data_trb->control |= TRB_DIR_IN;
        data_trb->status = TRB_LEN(len);
        trb_set_addr(data_trb, get_paddr((u32)data));
    }

    status_trb = xhci_next_xfer(hc, slot, ep_idx);
    memset(status_trb, 0, sizeof(*status_trb));
    status_trb->control = trb_type(TRB_CYCLE | TRB_IOC, TRB_TYPE_STATUS);
    if (hc->ep_cycle[slot][ep_idx])
        status_trb->control |= TRB_CYCLE;
    if (!in || len == 0)
        status_trb->control |= TRB_DIR_IN;

    xhci_trb_t *ring = hc->ep_rings[slot][ep_idx];
    u32 trb_idx = (u32)(setup_trb - ring);
    bool cycle = (setup_trb->control & TRB_CYCLE) != 0;
    xhci_set_ep_dequeue(hc, slot, ep_idx, trb_idx, cycle);
    xhci_post_xfer(hc, slot, ep_idx);

    err_t ret = xhci_wait_event(hc, TRB_TYPE_EVT_TRANSFER, NULL, NULL, 5000);
    return ret;
}

err_t xhci_control_transfer(xhci_device_t *dev, usb_setup_t *setup, void *data, int len)
{
    bool in = (setup->request_type & 0x80) != 0;
    return xhci_do_control(&xhci, dev, setup, data, len, in);
}

static err_t xhci_do_bulk(xhci_t *hc, xhci_device_t *dev, u8 ep_idx, void *buf, u32 len, bool in)
{
    xhci_trb_t *trb = xhci_next_xfer(hc, dev->slot, ep_idx);
    memset(trb, 0, sizeof(*trb));
    trb->control = trb_type(TRB_CYCLE | TRB_IOC, TRB_TYPE_NORMAL);
    if (hc->ep_cycle[dev->slot][ep_idx])
        trb->control |= TRB_CYCLE;
    if (in)
        trb->control |= TRB_DIR_IN;
    trb->status = TRB_LEN(len);
    trb_set_addr(trb, get_paddr((u32)buf));
    u32 trb_idx = hc->ep_enqueue[dev->slot][ep_idx] - 1;
    bool cycle = (trb->control & TRB_CYCLE) != 0;
    xhci_set_ep_dequeue(hc, dev->slot, ep_idx, trb_idx, cycle);
    xhci_post_xfer(hc, dev->slot, ep_idx);
    return xhci_wait_event(hc, TRB_TYPE_EVT_TRANSFER, NULL, NULL, 30000);
}

err_t xhci_bulk_in(xhci_device_t *dev, void *buf, u32 len)
{
    u8 ep_idx = xhci_ep_id(dev->ep_in);
    return xhci_do_bulk(&xhci, dev, ep_idx, buf, len, true);
}

err_t xhci_bulk_out(xhci_device_t *dev, void *buf, u32 len)
{
    u8 ep_idx = xhci_ep_id(dev->ep_out);
    return xhci_do_bulk(&xhci, dev, ep_idx, buf, len, false);
}

static err_t xhci_get_descriptor(xhci_device_t *dev, u8 type, u8 index, void *buf, int len)
{
    usb_setup_t setup;
    memset(&setup, 0, sizeof(setup));
    setup.request_type = 0x80;
    setup.request = USB_REQ_GET_DESCRIPTOR;
    setup.value = (u16)type << 8 | index;
    setup.length = len;
    return xhci_control_transfer(dev, &setup, buf, len);
}

static err_t xhci_set_configuration(xhci_device_t *dev, u8 config)
{
    usb_setup_t setup;
    memset(&setup, 0, sizeof(setup));
    setup.request_type = 0x00;
    setup.request = USB_REQ_SET_CONFIGURATION;
    setup.value = config;
    return xhci_control_transfer(dev, &setup, NULL, 0);
}

static err_t xhci_port_reset(xhci_t *hc, int port)
{
    u32 portsc = minl(xhci_port(hc, port));
    portsc |= XHCI_PORTSC_PR;
    moutl(xhci_port(hc, port), portsc);

    int expires = timer_expire_jiffies(5000);
    while (true)
    {
        portsc = minl(xhci_port(hc, port));
        if (!(portsc & XHCI_PORTSC_PR))
            break;
        if (timer_is_expires(expires))
            return -ETIME;
    }
    return EOK;
}

static err_t xhci_enumerate_port(xhci_t *hc, int port)
{
    u32 portsc = minl(xhci_port(hc, port));
    if (!(portsc & XHCI_PORTSC_CCS))
        return -ENODEV;

    LOGK("xHCI port %d connected\n", port);

    if (xhci_port_reset(hc, port) < EOK)
        return -EIO;

    portsc = minl(xhci_port(hc, port));
    u8 speed = (portsc >> XHCI_PORTSC_SPEED_SHIFT) & XHCI_PORTSC_SPEED_MASK;

    u8 slot = 0;
    if (xhci_cmd_enable_slot(hc, &slot) < EOK)
        return -EIO;

    LOGK("xHCI slot %d enabled on port %d speed %d\n", slot, port, speed);

    hc->dev_ctx[slot] = (u8 *)alloc_kpage(1);
    memset(hc->dev_ctx[slot], 0, PAGE_SIZE);
    ((u64 *)hc->dcbaap)[slot] = get_paddr((u32)hc->dev_ctx[slot]);

    xhci_init_ep_ring(hc, slot, 1);

    u8 ep0_mps = xhci_ep0_mps(speed);
    u8 *input = hc->input_ctx;
    memset(input, 0, PAGE_SIZE);
    input_ctx_add(input, (1u << 0) | (1u << 1));
    xhci_set_slot_ctx(hc, input, port, speed, 1, 0);
    xhci_set_ep_ctx(hc, input, 1, 4, ep0_mps, hc->ep_rings[slot][1], hc->ep_cycle[slot][1]);

    if (xhci_cmd_address_device(hc, slot, input, true) < EOK)
    {
        LOGK("xHCI address device (BSR) failed on port %d\n", port);
        return -EIO;
    }

    xhci_device_t *xdev = &hc->devices[hc->device_count];
    memset(xdev, 0, sizeof(*xdev));
    xdev->slot = slot;
    xdev->port = port;
    xdev->speed = speed;
    xdev->ep0_mps = ep0_mps;

    usb_device_desc_t dev_desc;
    if (xhci_get_descriptor(xdev, USB_DT_DEVICE, 0, &dev_desc, 8) < EOK)
    {
        LOGK("xHCI get device descriptor failed on port %d\n", port);
        return -EIO;
    }

    ep0_mps = dev_desc.max_packet;
    xdev->ep0_mps = ep0_mps;

    u8 address = (u8)(hc->device_count + 1);
    memset(input, 0, PAGE_SIZE);
    input_ctx_add(input, (1u << 0) | (1u << 1));
    xhci_set_slot_ctx(hc, input, port, speed, 1, address);
    xhci_set_ep_ctx(hc, input, 1, 4, ep0_mps, hc->ep_rings[slot][1], hc->ep_cycle[slot][1]);
    if (xhci_cmd_address_device(hc, slot, input, false) < EOK)
        return -EIO;
    xdev->address = address;

    u8 config_buf[256];
    memset(config_buf, 0, sizeof(config_buf));
    if (xhci_get_descriptor(xdev, USB_DT_CONFIG, 0, config_buf, 9) < EOK)
        return -EIO;

    usb_config_desc_t *cfg = (usb_config_desc_t *)config_buf;
    u16 cfg_len = cfg->total_len;
    if (cfg_len > sizeof(config_buf))
        cfg_len = sizeof(config_buf);
    if (cfg_len > 9 &&
        xhci_get_descriptor(xdev, USB_DT_CONFIG, 0, config_buf, cfg_len) < EOK)
        return -EIO;

    u8 *ptr = config_buf;
    u8 *end = config_buf + cfg->total_len;
    u8 ep_in = 0, ep_out = 0;
    u16 ep_in_mps = 0, ep_out_mps = 0;
    bool found_ms = false;
    u8 num_ctx = 1;
    u8 ep_in_id = 0, ep_out_id = 0;

    while (ptr < end)
    {
        u8 dlen = ptr[0];
        u8 type = ptr[1];
        if (dlen == 0)
            break;

        if (type == 0x04)
        {
            usb_interface_desc_t *iface = (usb_interface_desc_t *)ptr;
            if (iface->class == USB_CLASS_MASS_STORAGE &&
                iface->subclass == USB_SUBCLASS_SCSI &&
                iface->protocol == USB_PROTO_BOT)
            {
                found_ms = true;
                LOGK("USB Mass Storage interface found\n");
            }
        }
        else if (type == 0x05 && found_ms)
        {
            usb_endpoint_desc_t *ep = (usb_endpoint_desc_t *)ptr;
            u8 ep_addr = ep->addr;
            u8 ep_type = ep->attr & 0x03;
            if (ep_type == 2)
            {
                u8 ep_id = xhci_ep_id(ep_addr);
                if (ep_id > num_ctx)
                    num_ctx = ep_id;
                if (ep_addr & 0x80)
                {
                    ep_in = ep_addr;
                    ep_in_mps = ep->max_packet;
                    ep_in_id = ep_id;
                }
                else
                {
                    ep_out = ep_addr;
                    ep_out_mps = ep->max_packet;
                    ep_out_id = ep_id;
                }
            }
        }
        ptr += dlen;
    }

    if (!found_ms || !ep_in || !ep_out)
    {
        LOGK("port %d: not a BOT mass storage device\n", port);
        return -ENODEV;
    }

    xdev->ep_in = ep_in;
    xdev->ep_out = ep_out;
    xdev->ep_in_mps = ep_in_mps;
    xdev->ep_out_mps = ep_out_mps;
    xdev->mass_storage = true;

    xhci_init_ep_ring(hc, slot, ep_in_id);
    xhci_init_ep_ring(hc, slot, ep_out_id);

    u32 add_flags = (1u << 0) | (1u << ep_in_id) | (1u << ep_out_id);

    memset(input, 0, PAGE_SIZE);
    input_ctx_add(input, add_flags);
    xhci_set_slot_ctx(hc, input, port, speed, num_ctx, address);
    xhci_set_ep_ctx(hc, input, ep_in_id, 6, ep_in_mps, hc->ep_rings[slot][ep_in_id], hc->ep_cycle[slot][ep_in_id]);
    xhci_set_ep_ctx(hc, input, ep_out_id, 2, ep_out_mps, hc->ep_rings[slot][ep_out_id], hc->ep_cycle[slot][ep_out_id]);

    if (xhci_cmd_configure_ep(hc, slot, input) < EOK)
        return -EIO;

    if (xhci_set_configuration(xdev, cfg->config_value) < EOK)
        return -EIO;

    LOGK("USB device addr %d slot %d ep_in 0x%x ep_out 0x%x\n",
         address, slot, ep_in, ep_out);

    hc->device_count++;
    return EOK;
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
    moutl(xhci_op(hc, 0x30), get_paddr((u32)hc->dcbaap));
    moutl(xhci_op(hc, 0x34), 0);

    moutl(xhci_op(hc, 0x38), hc->max_slots);
}

int xhci_device_count(void)
{
    return xhci.device_count;
}

xhci_device_t *xhci_get_device(int idx)
{
    if (idx < 0 || idx >= xhci.device_count)
        return NULL;
    return &xhci.devices[idx];
}

void xhci_init(void)
{
    LOGK("xHCI init...\n");

    pci_device_t *device = pci_find_device_by_class(PCI_CLASS_SERIAL_USB_XHCI);
    if (!device)
    {
        LOGK("xHCI controller not found\n");
        return;
    }

    xhci_t *hc = &xhci;
    memset(hc, 0, sizeof(*hc));

    pci_bar_t membar;
    err_t ret = pci_map_mem_bar(device, &membar);
    if (ret != EOK || membar.iobase == 0)
    {
        LOGK("xHCI MMIO BAR not available\n");
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

    u32 pagesize = minl(hc->op_base + 0x08);
    hc->page_size = 1u << (pagesize + 12);

    LOGK("xHCI slots %d ports %d ctx %d page 0x%x\n",
         hc->max_slots, hc->max_ports, hc->ctx_size, hc->page_size);

    xhci_reset_controller(hc);
    xhci_init_rings(hc);

    u32 cmd = minl(xhci_op(hc, 0x00));
    cmd |= XHCI_USBCMD_RUN | XHCI_USBCMD_INTE;
    moutl(xhci_op(hc, 0x00), cmd);

    for (int port = 1; port <= hc->max_ports && port <= XHCI_MAX_PORTS; port++)
    {
        if (hc->device_count >= XHCI_MAX_DEVICES)
            break;
        xhci_enumerate_port(hc, port);
    }

    LOGK("xHCI init done, %d device(s)\n", hc->device_count);
}
