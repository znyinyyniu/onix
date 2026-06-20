#include <onix/tty.h>
#include <onix/device.h>
#include <onix/assert.h>
#include <onix/fifo.h>
#include <onix/task.h>
#include <onix/mutex.h>
#include <onix/debug.h>
#include <onix/errno.h>
#include <onix/syscall.h>
#ifdef ONIX_USB_BOOT
#include <onix/fbcon.h>
#endif


#ifdef ONIX_USB_BOOT
static void fbcon_write_filtered(const char *buf, int len)
{
    if (!fbcon_ready())
        return;

    bool esc = false;
    for (int i = 0; i < len; i++)
    {
        char ch = buf[i];
        if (esc)
        {
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
                esc = false;
            continue;
        }
        if (ch == '\033')
        {
            esc = true;
            continue;
        }
        fbcon_write(&ch, 1);
    }
}
#endif

extern task_t *task_table[TASK_NR]; // 任务表
static tty_t typewriter;

#ifdef ONIX_USB_BOOT
static dev_t usb_kbd_dev;
static dev_t usb_serial_dev;
static task_t *usb_tty_waiter;

void tty_input_wake(void)
{
    if (usb_tty_waiter != NULL)
    {
        task_unblock(usb_tty_waiter, EOK);
        usb_tty_waiter = NULL;
    }
}

static int usb_tty_read(char *buf, u32 count)
{
    int nr = 0;
    while (nr < (int)count)
    {
        if (usb_serial_dev && serial_has_char(usb_serial_dev))
        {
            device_read(usb_serial_dev, buf + nr, 1, 0, 0);
            nr++;
            continue;
        }
        if (keyboard_has_char())
        {
            device_read(usb_kbd_dev, buf + nr, 1, 0, 0);
            nr++;
            continue;
        }

        assert(usb_tty_waiter == NULL);
        usb_tty_waiter = running_task();
        task_block(usb_tty_waiter, NULL, TASK_BLOCKED, TIMELESS);
        usb_tty_waiter = NULL;
    }
    return nr;
}
#else
void tty_input_wake(void)
{
}
#endif

// 向前台组进程发送 SIGINT 信号
int tty_intr()
{
    tty_t *tty = &typewriter;
    if (!tty->pgid)
    {
        return 0;
    }
    for (size_t i = 0; i < TASK_NR; i++)
    {
        task_t *task = task_table[i];
        if (!task)
            continue;
        if (task->pgid != tty->pgid)
            continue;
        kill(task->pid, SIGINT);
    }
    return 0;
}

int tty_rx_notify(char *ch, bool ctrl, bool shift, bool alt)
{
    switch (*ch)
    {
    case '\r':
        *ch = '\n';
        break;
    default:
        break;
    }
    if (!ctrl)
        return 0;

    tty_t *tty = &typewriter;
    switch (*ch)
    {
    case 'c':
    case 'C':
        LOGK("CTRL + C Pressed\n");
        tty_intr();
        *ch = '\n';
        return 0;
    case 'l':
    case 'L':
        // 清屏
        device_write(tty->wdev, "\x1b[2J\x1b[0;0H\r", 12, 0, 0);
        *ch = '\r';
        return 0;
    default:
        return 1;
    }
    return 1;
}

// TTY 读
int tty_read(tty_t *tty, char *buf, u32 count)
{
#ifdef ONIX_USB_BOOT
    if (usb_kbd_dev)
        return usb_tty_read(buf, count);
#endif
    return device_read(tty->rdev, buf, count, 0, 0);
}

// TTY 写
int tty_write(tty_t *tty, char *buf, u32 count)
{
    int ret = device_write(tty->wdev, buf, count, 0, 0);
#ifdef ONIX_USB_BOOT
    fbcon_write_filtered(buf, count);
#endif
    return ret;
}

int tty_ioctl(tty_t *tty, int cmd, void *args, int flags)
{
    switch (cmd)
    {
    // 设置 tty 参数
    case TIOCSPGRP:
        tty->pgid = (pid_t)args;
        return EOK;
    default:
        break;
    }
    return -EINVAL;
}

int sys_stty()
{
    return -ENOSYS;
}

int sys_gtty()
{
    return -ENOSYS;
}

// 初始化串口
void tty_init()
{
    device_t *device = NULL;

    tty_t *tty = &typewriter;

#ifdef ONIX_USB_BOOT
    // 输入：QEMU 窗口键盘 + 串口终端均可；输出：串口 + 帧缓冲
    device = device_find(DEV_KEYBOARD, 0);
    assert(device);
    usb_kbd_dev = device->dev;
    tty->rdev = usb_kbd_dev;

    device = device_find(DEV_SERIAL, 0);
    if (device)
    {
        usb_serial_dev = device->dev;
        tty->wdev = usb_serial_dev;
    }
    else
    {
        usb_serial_dev = 0;
        device = device_find(DEV_CONSOLE, 0);
        tty->wdev = device->dev;
    }
#else
    // 输入设备是键盘
    device = device_find(DEV_KEYBOARD, 0);
    tty->rdev = device->dev;

    // 输出设备是控制台
    device = device_find(DEV_CONSOLE, 0);
    tty->wdev = device->dev;
#endif

    device_install(DEV_CHAR, DEV_TTY, tty, "tty", 0, tty_ioctl, tty_read, tty_write);
}
