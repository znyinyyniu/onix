#include <onix/stdarg.h>
#include <onix/stdio.h>
#include <onix/device.h>
#include <onix/printk.h>
#ifdef ONIX_USB_BOOT
#include <onix/fbcon.h>
#endif

static char buf[1024];

#ifdef ONIX_USB_BOOT
extern void early_serial_write(const char *buf, int len);
#endif

void kmsg_write(const char *buf, int len)
{
    device_t *device;

#ifdef ONIX_USB_BOOT
    if (fbcon_ready())
        fbcon_write(buf, len);
#endif

    device = device_find(DEV_CONSOLE, 0);
    if (device)
        device_write(device->dev, (char *)buf, len, 0, 0);

    device = device_find(DEV_SERIAL, 0);
    if (device)
        device_write(device->dev, (char *)buf, len, 0, 0);
#ifdef ONIX_USB_BOOT
    else
        early_serial_write(buf, len);
#endif
}

int printk(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);

    va_end(args);

    kmsg_write(buf, i);

    return i;
}
