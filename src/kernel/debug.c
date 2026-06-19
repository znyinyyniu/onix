#include <onix/debug.h>
#include <onix/stdarg.h>
#include <onix/stdio.h>
#include <onix/printk.h>

static char buf[1024];

void debugk(char *file, int line, const char *fmt, ...)
{
    int i = sprintf(buf, "[%s] [%d] ", file, line);
    kmsg_write(buf, i);

    va_list args;
    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

    kmsg_write(buf, i);
}