#include <onix/usb.h>
#include <onix/arena.h>

extern list_t usb_hcd_list;

static list_t root_hub_list;

// 初始化 usb 设备
void usb_init()
{
    list_init(&root_hub_list);
    
    list_t *list = &usb_hcd_list;
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        usb_hcd *hcd = element_entry(usb_hcd, node, node);
        usb_device *rhdev=(usb_device *)kmalloc(sizeof(usb_device));
        rhdev->hcd=hcd;
        list_push(&root_hub_list, &rhdev->node);
    }
}