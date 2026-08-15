#include <onix/debug.h>
#include <onix/pci.h>

void xhci_init(void)
{
    LOGK("xHCI init...\n");

    pci_device_t *device = pci_find_device_by_class(PCI_CLASS_SERIAL_USB_XHCI);
    if (!device)
    {
        USBLOG("xHCI controller not found\n");
        return;
    }
    LOGK("xHCI controller found: %s\n", pci_classname(device->classcode));
}