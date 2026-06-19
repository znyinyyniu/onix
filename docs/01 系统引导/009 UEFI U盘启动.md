# UEFI U 盘启动

Onix 支持从 U 盘以 UEFI 方式启动，镜像布局为 GPT 双分区：

| 分区 | 类型 | 文件系统 | 内容 |
|------|------|----------|------|
| P1 | EFI System | FAT32 | GRUB (`BOOTX64.EFI`) + `kernel.bin` |
| P2 | Linux | Minix v1 | 可写根文件系统 |

## 宿主依赖

```sh
sudo apt update
sudo apt install grub-efi-amd64 dosfstools parted ovmf qemu-system-x86
```

`qemu-system-x86` 提供 `qemu-system-x86_64`，用于 UEFI 冒烟（OVMF 为 x86_64 固件）。`make qemu` 仍使用 `qemu-system-i386`。

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

`qemu-usb` / `qemu-usb-xhci` **不需要**配置 `tap0` 或执行 `make tap0`（与 `make qemu` 不同）。二者使用 **`qemu-system-x86_64`**（与 x86_64 OVMF、`BOOTX64.EFI` 匹配），参数为 256M 内存、串口输出、无网卡/声卡。GRUB 通过 multiboot2 加载 32 位 `kernel.bin`，与实机 UEFI 路径一致。

```sh
make qemu-usb          # OVMF + virtio 磁盘（Phase 1 引导）
make qemu-usb-xhci     # OVMF + qemu-xhci + usb-storage（驱动阶段）
```

GRUB 与内核日志走当前终端串口（`grub-uefi.cfg` 已启用 `serial`）。

宿主 OVMF 固件路径为 `/usr/share/OVMF/OVMF_CODE.fd`。

## 实机验收

在 i5-5200U 上从 USB 3.0 口启动，应出现 GRUB 菜单并加载内核。完整根文件系统挂载需 xHCI 与 Mass Storage 驱动就绪（见 `openspec/changes/uefi-usb-boot/tasks.md`）。

## 常见问题

**已安装 `grub-efi-amd64`，仍提示缺少 GRUB**

旧版构建脚本误检查 `grub.efi`（该文件在 Ubuntu 上通常不存在）。请更新代码后执行 `make check-usb-host-deps`，应检查 `multiboot2.mod` 与 `grub-install`。

**`cp: Permission denied`（写入 `/tmp/onix-usb-esp`）**

ESP 由 `sudo mount` 与 `grub-install` 创建，需使用 `sudo cp` 写入 `kernel.bin` 与 `grub.cfg`（构建脚本已处理）。

**`could not configure /dev/net/tun (tap0): Operation not permitted`**

旧版 `qemu-usb` 继承了 `make qemu` 的 `-netdev tap,tap0` 参数，与 U 盘引导冒烟无关。更新后 `qemu-usb` 不再使用 `tap0`。若仍看到此错误，请确认已拉取最新 `cmd.mk`。在宿主机测网络请用 `make qemu`（需先 `make tap0`）。

**QEMU 黑屏、串口无任何 OVMF/GRUB 输出**

`qemu-usb` 必须使用 `qemu-system-x86_64`，不能用 `qemu-system-i386` 加载 x86_64 OVMF。若手动测试，请用：

```sh
qemu-system-x86_64 -m 256M -bios /usr/share/OVMF/OVMF_CODE.fd \
  -drive file=../build/onix_usb.img,format=raw,if=virtio -boot order=c \
  -serial stdio -monitor none
```

**`grub-install` 对 loop 设备失败**

把完整终端输出保存下来；必要时可改用 `grub-mkimage` 生成 `BOOTX64.EFI`（见 OpenSpec 设计文档方案 C）。
