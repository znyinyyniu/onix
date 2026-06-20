## 1. UEFI U 盘镜像构建

- [x] 1.1 新增 `src/utils/usb-efi.mk`，创建 GPT 原始镜像（整盘 256MB；P1 EF00 FAT32 ~32MB，P2 8304 Minix）
- [x] 1.2 向 ESP 安装 GRUB x86_64-efi（`EFI/BOOT/BOOTX64.EFI`），并添加含 multiboot2 `/boot/kernel.bin` 的 `grub.cfg`
- [x] 1.3 将 `kernel.bin` 复制到 ESP `/boot/`，填充 P2 Minix 根（复用 `image.mk` 布局：`/bin`、`/etc`、`/dev`、`/mnt` 及应用程序）
- [x] 1.4 在 `src/makefile` 中接入 `usb-efi` / `usb-image` 目标，并文档化宿主依赖（`grub-efi-amd64`、`dosfstools`、`parted` 或 `sgdisk`）
- [x] 1.5 在 README 或 docs 中说明 `dd` 部署步骤及关闭 Secure Boot 的方法

## 2. QEMU 冒烟：镜像与 UEFI 引导（Phase 1）

- [x] 2.1 在 `cmd.mk` 增加 `qemu-usb` 目标（OVMF + `onix_usb.img` + `-serial stdio`），文档化宿主依赖 `ovmf`
- [x] 2.2 构建后在宿主检查镜像：GPT 分区类型、ESP 文件、`P2` Minix 可挂载且 `/bin` 存在
- [x] 2.3 QEMU + OVMF：出现 GRUB 菜单，multiboot2 成功加载内核（驱动完成前预期 `mount_root` panic）
- [x] 2.4 可选：GRUB 启用串口输出；在 QEMU 中确认 multiboot2 mmap 路径正常

## 3. 实机验收：镜像与 UEFI 引导（Phase 1）

- [x] 3.1 构建 `onix_usb.img` 并 `dd` 写入 U 盘
- [x] 3.2 在 i5-5200U 上从 USB 3.0 口验证 UEFI 启动：出现 GRUB 菜单并成功加载内核（驱动完成前预期 `mount_root` panic）
- [x] 3.3 可选：实机串口调试；确认 UEFI 下 multiboot2 mmap 与 QEMU 观察一致

## 4. 设备模型与 GPT 分区

- [x] 4.1 在 `device.h` 中新增 `DEV_USB_DISK` 和 `DEV_USB_PART`
- [x] 4.2 新增 `gpt.h` / `gpt.c`，实现 GPT 头校验与分区表项解析
- [x] 4.3 新增 `usb_disk_t` / `usb_part_t` 结构，镜像 IDE 分区的 LBA 偏移读写
- [x] 4.4 实现 `usb_part_install()`，在磁盘解析后将 GPT 分区注册为块设备
- [x] 4.5 在构建主机上用已知 `onix_usb.img` 扇区转储验证 GPT 解析（脚本或手动 hexdump）

## 5. xHCI 主机控制器驱动

- [x] 5.1 新增 `xhci.h` / `xhci.c`：PCI 匹配 `0x0C0330`，映射 BAR0 MMIO，能力/运行时寄存器
- [x] 5.2 实现 UEFI 交棒后的主机控制器复位及 run/halt 状态机
- [x] 5.3 实现命令环、事件环、传输环分配（页对齐）
- [x] 5.4 实现端口连接检测、复位及 slot context 配置
- [x] 5.5 实现 USB 枚举流程（GET_DESCRIPTOR、SET_ADDRESS、SET_CONFIGURATION）
- [x] 5.6 实现 bulk TRB 提交及 IN/OUT 端点完成处理
- [x] 5.7 在 `pci_init` 路径或 `init.c` 中于文件系统初始化前调用 `xhci_init()`

## 6. USB Mass Storage 类

- [x] 6.1 新增 `usb.h` / `usb_storage.c`：检测 Mass Storage Bulk-Only 接口（class 8/6/50）
- [x] 6.2 在 bulk 端点上实现 CBW / 数据 / CSW 事务封装
- [x] 6.3 实现 SCSI READ(10)、WRITE(10)、READ CAPACITY(10)
- [x] 6.4 注册 `DEV_USB_DISK`，read/write/ioctl 与 IDE 扇区语义一致
- [x] 6.5 Mass Storage 就绪后对 USB 磁盘触发 GPT 解析

## 7. USB 根挂载

- [x] 7.1 新增 `ONIX_USB_BOOT` 编译选项（或等价机制），用于 USB 启动内核构建
- [x] 7.2 在 `super.c` 中更新 `mount_root()`：设置 `ONIX_USB_BOOT` 时挂载 USB GPT 分区索引 1（Minix P2）
- [x] 7.3 确保 `init.c` 在 `super_init()` 前完成 USB/xHCI 初始化；`ONIX_USB_BOOT` 下可选跳过 `ide_init()`
- [x] 7.4 将 USB 启动版 `kernel.bin` 构建进 `onix_usb.img` 的 ESP

## 8. QEMU 冒烟：驱动与端到端（Phase 3–4）

- [x] 8.1 在 `cmd.mk` 增加 `qemu-usb-xhci` 目标（`qemu-xhci` + `usb-storage` 挂载 `onix_usb.img`）
- [x] 8.2 QEMU：xHCI 枚举 U 盘，GPT 解析成功，日志可见 P2 块设备
- [x] 8.3 QEMU：内核进入用户态，`mount_root` 挂载 Minix P2
- [ ] 8.4 QEMU：可写根验证——创建或覆盖 `/hello.txt` 并读回；运行至少一个 `/bin` 内置程序

## 9. 实机验收：端到端（最终门禁）

- [ ] 9.1 从 USB 3.0 在 i5-5200U 上启动：内核进入用户态，Minix 根位于 P2
- [ ] 9.2 验证可写根：创建或覆盖文件（如 `/hello.txt`）并读回
- [ ] 9.3 从 USB 根运行至少一个内置 `/bin` 应用程序
- [ ] 9.4 确认未设置 `ONIX_USB_BOOT` 时，传统 ISO / `master.img` 启动仍正常
