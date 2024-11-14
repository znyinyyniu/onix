#ifndef ONIX_USB_H
#define ONIX_USB_H

#include <onix/pci.h>

#define PCI_CLASS_SERIAL_USB_UHCI	0x0c0300
#define PCI_CLASS_SERIAL_USB_OHCI	0x0c0310
#define PCI_CLASS_SERIAL_USB_EHCI	0x0c0320
#define PCI_CLASS_SERIAL_USB_XHCI	0x0c0330

typedef struct usb_hcd {
    list_node_t node;
    pci_device_t *device;
} usb_hcd;

typedef struct usb_device {
    list_node_t node;
    
    usb_hcd *hcd;
    
    /* static strings from the device */
	char *product;
	char *manufacturer;
	char *serial;
} usb_device;

#endif