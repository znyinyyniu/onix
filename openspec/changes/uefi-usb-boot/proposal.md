## 背景与动机

Onix 目前通过传统 BIOS 启动（自研 bootloader 或 GRUB ISO），根文件系统只能从 IDE 磁盘挂载。README 已明确说明不支持 USB，因此无法在实体机上从 U 盘运行系统。目标平台（Intel Core i5-5200U、USB 3.0 口引导）需要 UEFI + GPT + xHCI，以便在 GRUB 加载内核后，从同一块 U 盘上的可写 Minix 根分区挂载并运行完整系统。

## 变更内容

- 新增构建目标，产出可用 `dd` 写入 U 盘的 GPT UEFI 磁盘镜像：FAT32 ESP（GRUB + 内核）+ Minix 根分区
- 实现 xHCI 主机控制器驱动，支持 Intel USB 3.0（目标硬件上的主要引导路径）
- 实现 USB Mass Storage 类（Bulk-Only Transport），并注册 USB 块设备
- 为 USB 块设备实现 GPT 分区解析（现有 `ide_part_init` 仅支持 MBR，会误读 GPT 磁盘上的保护性 MBR）
- 调整根挂载逻辑：在纯 USB 单盘配置下，从 USB GPT 第二分区挂载 Minix 根文件系统
- 文档说明 Secure Boot 要求（需用户手动关闭）
- 保持现有 BIOS ISO / IDE 启动路径不变

## 能力范围

### 新增能力

- `uefi-usb-image`：UEFI GPT U 盘镜像的构建系统与分区布局（ESP + Minix 根分区）
- `gpt-partition`：GPT 分区表解析与分区块设备注册
- `xhci-driver`：xHCI 主机控制器初始化、端口复位与 USB 设备枚举
- `usb-mass-storage`：USB Mass Storage 类块设备（基于 Bulk-Only Transport 的 SCSI 读写）
- `usb-root-mount`：从 USB GPT Minix 分区挂载根文件系统（纯 USB 单盘启动）

### 修改的能力

<!-- 尚无既有 openspec spec；根挂载行为变更由 usb-root-mount 覆盖 -->

## 影响范围

- **构建**：新增 `usb-efi.mk`（或类似目标），宿主依赖 `grub-efi-amd64`、GPT 分区工具；与现有 `cdrom.mk` / `image.mk` 并存
- **内核**：新增 `xhci.c`、`usb.c`、`usb_storage.c`、`gpt.c`；`include/onix/` 下新增头文件；`device.h` 增加 `DEV_USB_*` 设备类型
- **文件系统**：`super.c` 中 `mount_root()` 在 USB 启动镜像下优先从 USB 挂载
- **初始化**：`init.c` 调整顺序——USB 协议栈在 `super_init()` 之前完成
- **文档/README**：更新 U 盘启动说明与 Secure Boot 注意事项
- **非目标**：Secure Boot 签名、FAT 根文件系统、EHCI/OHCI 回退、USB 启动镜像与 IDE 共存、复用 `origin/usb` 分支代码
