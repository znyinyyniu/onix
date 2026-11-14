#include <onix/stdarg.h>
#include <onix/console.h>
#include <onix/stdio.h>
#include <onix/device.h>

static char buf[1024];

int printk(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);

    va_end(args);

#ifdef ONIX_USB_BOOT
    // 与 tty 一致：USB 启动时 printk/assert 也走串口，避免 GOP 下看不见
    device_t *device = device_find(DEV_SERIAL, 0);
    if (!device)
        device = device_find(DEV_CONSOLE, 0);
#else
    device_t *device = device_find(DEV_CONSOLE, 0);
#endif
    device_write(device->dev, buf, i, 0, 0);

    return i;
}
