#ifndef ONIX_PRINTK_H
#define ONIX_PRINTK_H

void kmsg_write(const char *buf, int len);

int printk(const char *fmt, ...);

#endif