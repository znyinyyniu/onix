## Why

`make qemu-usb-xhci`（OVMF + Multiboot2）下内核在 `memory_init` 因 `assert(memory_base == MEMORY_BASE)` 失败而自旋：当前实现按「最长 AVAILABLE」选型，UEFI 内存图中最大块从 9MiB 起，与 Onix「可用物理内存从 1MiB 起」的模型冲突。失败发生在 `serial_init` 之前，且 `printk` 仍写 VGA text，表现为终端只有 GRUB 的 `WARNING: no console will be available to OS`、无内核日志。

## What Changes

- Multiboot2 路径下改为：固定 `memory_base = MEMORY_BASE`（1MiB），用 mmap 中 AVAILABLE 区域的最高结尾地址推导 `memory_size`
- 在建立物理页占用表时，将管理范围内非 AVAILABLE（及未被 AVAILABLE 覆盖）的页标记为已占用，避免把 NVS/RESERVED 洞当作可分配内存
- 保持现有 `total_pages` / `start_page` / `memory_map` 放在 1MiB 的模型与 assert；不改为「沿用 base=9MiB」
- **不在本变更范围内**：早期串口、`printk` 改出口、Multiboot2 framebuffer、GRUB/`gfxpayload`、xHCI 功能本身

## Capabilities

### New Capabilities

- `physical-memory`: 物理内存发现与页占用表初始化（尤其 Multiboot2/UEFI 下的选型与洞标记）

### Modified Capabilities

- （无；仓库尚无既有 main specs）

## Impact

- 主要代码：`src/kernel/memory.c`（`memory_init` Multiboot2 分支、`memory_map_init` 或紧随的洞标记；可能增加静态缓存以免 MBI 被 `memory_map` 覆盖）
- 头文件：`src/include/onix/memory.h`、`multiboot2.h` 预期无需改行为宏；最多局部辅助类型/常量
- 启动路径：`make qemu-usb` / `qemu-usb-xhci`（UEFI）；BIOS/`ONIX_MAGIC` 路径应保持可引导，行为可与现逻辑兼容或统一算法
- 验收：UEFI 启动不再在 `memory.c:117` assert 处自旋；可进入后续初始化（至少越过 `memory_init` / `memory_map_init`）
