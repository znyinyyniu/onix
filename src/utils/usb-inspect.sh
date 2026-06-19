#!/bin/bash
# 检查 onix_usb.img 分区布局与内容（宿主冒烟，对应 tasks 2.2）
set -e
IMG="${1:-../build/onix_usb.img}"
if [ ! -f "$IMG" ]; then
    echo "镜像不存在: $IMG"
    exit 1
fi

echo "=== GPT 分区 ==="
parted -s "$IMG" unit s print

P1_TYPE=$(parted -s "$IMG" print | awk '/^ 1 /{print $6}')
P2_TYPE=$(parted -s "$IMG" print | awk '/^ 2 /{print $6}')
echo "P1 type: $P1_TYPE (期望 fat32/EFI)"
echo "P2 type: $P2_TYPE (期望 ext2 或未知 — Minix)"

LOOP=$(sudo losetup --find --show --partscan "$IMG")
trap 'sudo umount /tmp/onix-inspect-esp /tmp/onix-inspect-root 2>/dev/null; sudo losetup -d "$LOOP" 2>/dev/null' EXIT

sudo mkdir -p /tmp/onix-inspect-esp /tmp/onix-inspect-root
sudo mount "${LOOP}p1" /tmp/onix-inspect-esp
sudo mount "${LOOP}p2" /tmp/onix-inspect-root

echo "=== ESP 文件系统 ==="
fsck.vfat -n /tmp/onix-inspect-esp 2>&1 | grep -E 'FAT32|FAT16|clusters' || true
fsck.vfat -n /tmp/onix-inspect-esp 2>&1 | grep -q 'less than the required minimum' && \
	echo "错误: ESP 为不合规 FAT32（簇数不足），OVMF 无法引导。请用 mkfs.vfat -F 16 重建。" && exit 1

echo "=== ESP ==="
test -f /tmp/onix-inspect-esp/EFI/BOOT/BOOTX64.EFI
test -f /tmp/onix-inspect-esp/boot/kernel.bin
test -f /tmp/onix-inspect-esp/boot/grub/grub.cfg
ls -la /tmp/onix-inspect-esp/EFI/BOOT/
ls -la /tmp/onix-inspect-esp/boot/

echo "=== Minix 根 (P2) ==="
test -d /tmp/onix-inspect-root/bin
ls /tmp/onix-inspect-root/bin | head -5
echo "检查通过."
