# Onix - 操作系统实现

![](./docs/others/images/snapshot.png)

- [本项目地址](https://github.com/StevenBaby/onix)

- [相关 bilibili 视频](https://www.bilibili.com/video/BV1gR4y1u7or/)

- [参考文献](./docs/others/参考文献.md)

## 主要功能

- 系统引导：
    - [x] bootloader
    - [x] multiboot2
- 设备驱动：
    - [x] PIC 8259 中断控制器
    - [x] PIT 8254 定时计数器
    - [x] 蜂鸣器
    - [x] CMOS 时钟 / 实时时钟
    - [x] CRTC 控制台 `0xb8000`
    - [x] PS/2 键盘
    - [x] 串口设备
    - [x] CPU 功能检测
    - [x] FPU 浮点处理单元
    - [x] IDE 硬盘 (PIO)
    - [x] 声霸卡驱动 (🐛 一些情况下播放卡顿)
    - [x] 软盘驱动
    - [x] PCI 总线枚举
    - [x] IDE 硬盘 (UDMA)
    - [x] e1000 网卡驱动
    - [x] ATAPI 光盘驱动
    - [ ] PS/2 鼠标 (🌱 panda)
    - [ ] VESA 图形驱动 (🌱 panda)
    - [ ] ACPI：控制关机和重启 [@lihanrui2913](https://github.com/lihanrui2913) (🌱 acpi)
    - [ ] RTL8139 网卡驱动
    - [ ] USB
- 内存管理：
    - [x] 保护模式和全局描述符
    - [x] **页映射**（核心功能）
    - [x] 堆内存管理
- 任务管理：
    - [x] **任务调度**（核心功能）
    - [x] 同步与互斥 (锁)
    - [x] 定时器
    - [x] 信号机制
- 用户程序：
    - [x] ELF 文件解析
    - [x] ELF 文件加载运行
    - [ ] 动态链接库
- 系统调用
- 文件系统：
    - [x] 管道
    - [x] 套接字
    - [x] Minix1 文件系统
    - [x] FAT 12/16/32 文件系统 (🌱 fat32)
    - [x] ISO9660 文件系统
- 网络协议：
    - [x] Ethernet 以太网协议
    - [x] ARP 协议
    - [x] IPv4 协议
    - [x] ICMP 协议
    - [x] UDP 协议
    - [x] TCP 协议
    - [x] DHCP 协议：局域网 IP 地址获取
    - [x] DNS 协议：域名解析
    - [x] HTTP 协议：简单的 HTTP 服务器
- 开发环境：
    - [x] WSL2 [@znyin](https://github.com/znyinyyniu)
    - [ ] docker (🌱 docker)

## 使用方法

> U 盘 UEFI 启动见 [UEFI U盘启动](./docs/01%20系统引导/009%20UEFI%20U盘启动.md)。需在固件中**关闭 Secure Boot**。

> 传统 ISO / IDE 启动：目前不支持 USB 驱动下的 legacy 自研 bootloader 实体机 U 盘，请使用 UEFI 镜像。

iso 文件的使用参考 [版本 1.0.0](./docs/13%20系统优化/179%20版本%201.0.0.md)；

对于 `qemu` 模拟器，需要提前配置 `tap0` 设备，用于网络：

    qemu-system-i386  -m 32M -audiodev pa,id=snd -machine pcspk-audiodev=snd -device sb16,audiodev=snd -rtc base=localtime -chardev stdio,mux=on,id=com1 -serial chardev:com1 -netdev tap,id=eth0,ifname=tap0,script=no,downscript=no -device e1000,netdev=eth0 -drive file=onix_1.0.0.iso,media=cdrom,if=ide -boot d

## Pull Request 约定

1. 确保每个修改的文件都是有意义的，不要添加与系统无关的文件；
2. 确保每个 commit 都有足够的分量，对于无关紧要的 commit 可以合并在一起；
3. Pull Request 请先提交到 dev 分支，若有新功能开发，再开新分支；

Commit Decription 前面加上 emoji ❤️ 提高阅读性：

- ✨ 视频录制：与 B 站某一视频相关
- 🐛 漏洞修复：修改了系统中的某个 Bug
- 🎈 功能开发：加入原系统中没有的新功能
- 📖 文档整理：修改 docs 中的内容
- 📔 学习笔记：记录学习过程中的一些问题或者感悟
- 🍕 其他：若有与 commit 强相关的 emoji 也可以添加，如：💾 软盘驱动

## 问题及答案

- [问题及答案](./docs/others/问题及答案%20(Question%20and%20Answer).md)
    - [gcc -m32 链接错误](./docs/others/问题及答案%20(Question%20and%20Answer).md#gcc--m32-%E9%93%BE%E6%8E%A5%E9%94%99%E8%AF%AF)
    - [无法调试汇编代码](./docs/others/问题及答案%20(Question%20and%20Answer).md#%E6%97%A0%E6%B3%95%E8%B0%83%E8%AF%95%E6%B1%87%E7%BC%96%E4%BB%A3%E7%A0%81)

## 相关软件版本参考

- bochs >= 2.7 [^bochs]
- qemu >= 6.2.0 [^qemu]
- gcc >= 11.2.0 [^gcc]
- gdb >= 12.1 [^gdb]
- nasm >= 2.15.05 [^nasm]
- binutils >= 2.38 [^binutils]
- sfdisk >= 2.39.3
- vmware >= 16.1 [^vmware]
- vscode >= 1.74.3 [^vscode]
- ffmpeg >= 6.0 [^ffmpeg]
- python >= 3.6 [^python]
- pyelftools >= 0.29 [^pyelftools]

## Star History

![Star History Chart](https://api.star-history.com/svg?repos=StevenBaby/onix&type=Date)

## 参考

[^bochs]: <https://bochs.sourceforge.io>
[^qemu]: <https://www.qemu.org/docs/master/>
[^gcc]: <https://gcc.gnu.org/>
[^gdb]: <https://www.sourceware.org/gdb/>
[^nasm]: <https://www.nasm.us/>
[^binutils]: <https://www.gnu.org/software/binutils/>
[^vmware]: <https://www.vmware.com/products/workstation-pro.html>
[^vscode]: <https://code.visualstudio.com/>
[^ffmpeg]: <https://ffmpeg.org>
[^python]: <https://www.python.org/>
[^pyelftools]: <https://github.com/eliben/pyelftools>
