#include <onix/debug.h>
#include <onix/stdarg.h>
#include <onix/stdio.h>
#include <onix/printk.h>
#ifdef ONIX_USB_BOOT
#include <onix/device.h>
extern void early_serial_write(const char *buf, int count);
#endif

static char buf[1024];

#ifdef ONIX_USB_BOOT
// 方案 D：DEBUGK 仅串口，不刷帧缓冲，避免冲掉 shell 输出
static void debugk_emit(const char *data, int len)
{
    device_t *dev = device_find(DEV_SERIAL, 0);
    if (dev)
        device_write(dev->dev, (char *)data, len, 0, 0);
    else
        early_serial_write(data, len);
}
#endif

void debugk(char *file, int line, const char *fmt, ...)
{
    int i = sprintf(buf, "[%s] [%d] ", file, line);
#ifdef ONIX_USB_BOOT
    debugk_emit(buf, i);
#else
    kmsg_write(buf, i);
#endif

    va_list args;
    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

#ifdef ONIX_USB_BOOT
    debugk_emit(buf, i);
#else
    kmsg_write(buf, i);
#endif
}
