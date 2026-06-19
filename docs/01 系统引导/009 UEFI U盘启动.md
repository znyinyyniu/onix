# UEFI U 盘启动

Onix 支持从 U 盘以 UEFI 方式启动，镜像布局为 GPT 双分区：

| 分区 | 类型 | 文件系统 | 内容 |
|------|------|----------|------|
| P1 | EFI System | FAT32 | GRUB (`BOOTX64.EFI`) + `kernel.bin` |
| P2 | Linux | Minix v1 | 可写根文件系统 |

## 宿主依赖

```sh
sudo apt update
sudo apt install grub-efi-amd64 dosfstools parted ovmf
```

说明：安装 `grub-efi-amd64` 后，**不会**在 `/usr/lib/grub/x86_64-efi/` 下出现 `grub.efi` 文件，这是 Ubuntu/Debian 上的正常现象。该目录提供的是 `*.mod` 模块；构建镜像时由 `grub-install` 在 ESP 中生成 `EFI/BOOT/BOOTX64.EFI`。

检查依赖是否就绪：

```sh
cd src
make check-usb-host-deps
```

## 构建

```sh
cd src
make usb-image      # 生成 ../build/onix_usb.img（USB_BOOT 内核）
make usb-inspect    # 宿主检查分区与文件
```

## 写入 U 盘

```sh
sudo dd if=../build/onix_usb.img of=/dev/sdX bs=4M status=progress conv=fsync
```

将 `/dev/sdX` 替换为实际 U 盘设备（务必确认，避免写错盘）。

## 固件设置

1. **关闭 Secure Boot**（未签名 GRUB 需要）
2. 启动顺序选择 **UEFI: <U盘名称>**
3. 使用 **USB 3.0 口**（目标平台 i5-5200U）

## QEMU 冒烟

```sh
make qemu-usb          # OVMF + virtio 磁盘（Phase 1 引导）
make qemu-usb-xhci     # OVMF + qemu-xhci + usb-storage（驱动阶段）
```

宿主 OVMF 固件路径为 `/usr/share/OVMF/OVMF_CODE.fd`。

## 实机验收

在 i5-5200U 上从 USB 3.0 口启动，应出现 GRUB 菜单并加载内核。完整根文件系统挂载需 xHCI 与 Mass Storage 驱动就绪（见 `openspec/changes/uefi-usb-boot/tasks.md`）。

## 常见问题

**已安装 `grub-efi-amd64`，仍提示缺少 GRUB**

旧版构建脚本误检查 `grub.efi`（该文件在 Ubuntu 上通常不存在）。请更新代码后执行 `make check-usb-host-deps`，应检查 `multiboot2.mod` 与 `grub-install`。

**`cp: Permission denied`（写入 `/tmp/onix-usb-esp`）**

ESP 由 `sudo mount` 与 `grub-install` 创建，需使用 `sudo cp` 写入 `kernel.bin` 与 `grub.cfg`（构建脚本已处理）。

**`grub-install` 对 loop 设备失败**

把完整终端输出保存下来；必要时可改用 `grub-mkimage` 生成 `BOOTX64.EFI`（见 OpenSpec 设计文档方案 C）。
