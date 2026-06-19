## 背景

Onix 是 32 位（i386）教学操作系统，支持 multiboot2、Minix v1 根文件系统，块存储基于 IDE。当前启动路径：

- 传统方式：`boot.asm` → `loader.asm` → 内核（IDE PIO，固定 `0x1F0`）
- GRUB ISO：`grub-mkrescue` + ISO9660（只读），或配合 IDE `master.img`

`ide.c` 中的分区解析（`ide_part_init`）将 LBA 0 当作经典 MBR，读取 4 个主分区表项。UEFI U 盘使用 GPT：LBA 0 为保护性 MBR（类型 `0xEE`），真实分区表在 LBA 1 的 GPT 头中。对 GPT 磁盘套用 MBR 解析会得到一个错误的「整盘分区」。

目标硬件：Intel Core i5-5200U（Broadwell），USB 3.0 口引导，集成 xHCI 控制器。Secure Boot 由用户手动关闭。

探索阶段确定的约束：

- **最终验收以实体机（i5-5200U USB 3.0）为准**；开发过程使用 QEMU 冒烟缩短迭代，不替代实机门禁
- GRUB 加载内核；内核须能访问同一块 U 盘以挂载可写根
- U 盘上使用可写 Minix 根；内核内不做 FAT 根文件系统
- 从零实现（不延续 `origin/usb` 分支）
- 纯 USB 单盘（USB 镜像上不依赖 IDE 根分区回退）

## 目标 / 非目标

**目标：**

- 产出可用 `dd` 写入 U 盘的 GPT UEFI 镜像，在 i5-5200U 上通过 USB 3.0 引导
- GRUB（x86_64-efi）从 FAT32 ESP 加载 32 位 multiboot2 `kernel.bin`
- 内核初始化 xHCI、枚举 USB Mass Storage、解析 GPT，将 Minix P2 挂载为可写根
- 复用现有 Minix 文件系统、块缓冲及 IDE 的 `device_install` 模式
- 保持现有 BIOS ISO / IDE 启动路径不变
- 各阶段在实机验证前，优先通过 QEMU 冒烟隔离镜像/引导/驱动问题

**非目标：**

- Secure Boot / shim 签名
- 以 FAT 或 ext4 作为根文件系统
- EHCI/OHCI/UHCI 主机控制器（仅 USB 2.0 口引导）
- 延续或合并 `origin/usb` 分支代码
- USB 启动镜像与 IDE 磁盘共存
- 动态 `root=` 内核命令行（v1 固定 P2 布局）
- Minix 在闪存上的磨损均衡或日志
- 以 QEMU 通过作为变更完成的唯一标准

## 验证策略：QEMU 冒烟 + 实机验收

每个可测阶段采用双轨结构：**先 QEMU 冒烟，再实机确认**。实机端到端为最终门禁。

```
┌─────────────────────────────────────────────────────────────────┐
│                    双轨验证（每阶段）                            │
├────────────────────────────┬────────────────────────────────────┤
│  QEMU 冒烟（开发反馈快）    │  实机验收（i5-5200U，最终门禁）     │
├────────────────────────────┼────────────────────────────────────┤
│  OVMF + onix_usb.img       │  dd 到 U 盘 + USB 3.0 口引导       │
│  串口 log / GDB            │  真实 Intel xHCI + UEFI 交棒       │
│  qemu-xhci + usb-storage   │  可写 Minix P2 端到端               │
└────────────────────────────┴────────────────────────────────────┘
```

| 阶段 | QEMU 能验证 | 实机必须验证 |
|------|-------------|--------------|
| 镜像构建 | 宿主挂载检查 GPT/ESP/P2；OVMF 启动 GRUB、加载内核 | `dd` 后 UEFI 从 U 盘引导 |
| GPT 解析 | 扇区转储/单元逻辑；接入块设备后 QEMU 读超级块 | — |
| xHCI / Mass Storage | `qemu-xhci` 枚举、BOT 读写、快速调试 | Intel xHCI、UEFI 交棒后状态 |
| 端到端 | 用户态、`/hello.txt` 写入、跑 `/bin` 程序 | **同上，作为变更完成标准** |

QEMU 参考命令（实现时写入 `cmd.mk` / 文档）：

```bash
# UEFI + U 盘镜像（需 ovmf）
qemu-system-i386 -m 256M -bios /usr/share/OVMF/OVMF.fd \
  -drive file=build/onix_usb.img,format=raw,if=virtio \
  -serial stdio

# 驱动阶段附加（整盘作为 USB 存储）
-device qemu-xhci -device usb-storage,drive=usb0 \
  -drive id=usb0,file=build/onix_usb.img,format=raw,if=none
```

**QEMU 不能替代实机的部分：** UEFI 交棒后 xHCI 真实状态、笔记本固件对 USB 启动的差异、USB 3.0 物理口引导路径。

## 技术决策

### 1. 通过 x86_64 GRUB 做 UEFI 启动，加载 32 位 multiboot2 内核

**选择：** ESP 包含 `BOOTX64.EFI`（grub x86_64-efi）、`grub.cfg`、`kernel.bin`。

**理由：** 现代 UEFI 固件为 x86_64；GRUB multiboot2 可加载 32 位内核。Onix 已有 multiboot2 头及 `memory_init` 中的 mmap 解析。

**备选方案：**

- `grubia32.efi`：桌面 UEFI 上少见，硬件覆盖差
- 自研 UEFI 应用：工作量大，相对 GRUB 无优势

### 2. GPT 双分区布局

**选择：**

| 分区 | 类型 | 文件系统 | 用途 |
|------|------|----------|------|
| P1 | EFI System（`EF00`） | FAT32 ~256MB | GRUB + 内核 |
| P2 | Linux（`8304`） | Minix v1 | 可写根（`/bin`、`/etc` 等） |

**理由：** 符合 UEFI 常规做法；`grub-install --target=x86_64-efi --removable` 要求 GPT + ESP。Minix 根与 FAT ESP 分离。

**备选方案：**

- 仅 MBR：与标准 UEFI ESP 流程不兼容
- 单 FAT 分区放全部内容：内核须实现 FAT 根文件系统驱动

### 3. 新增 GPT 解析器，不扩展 `ide_part_init`

**选择：** USB 块设备层使用独立 `gpt.c`；`ide_part_init` 保持不变。

**理由：** GPT 磁盘上的保护性 MBR 会使 MBR 解析失效。仅 USB 场景可保持 IDE 代码稳定。头校验 + 表项遍历约 300 行。

**备选方案：**

- 构建时固定 LBA 偏移写入内核：ESP 大小变更时脆弱
- 让 `ide_part_init` 支持 GPT：纯 USB 镜像无必要

### 4. 仅实现 xHCI 主机控制器驱动

**选择：** PCI 类 `0x0C0330`（xHCI）；UEFI 交棒后完整控制器复位；基于 ring 的 I/O。

**理由：** i5-5200U 的 USB 3.0 口使用 Intel xHCI，同一控制器也处理这些口上的 USB 2.0 设备。

**备选方案：**

- 先做 EHCI（旧 `origin/usb` 思路）：不符合 USB 3.0 引导主路径
- xHCI + EHCI 双支持：延后；当前硬件不需要

### 5. USB Mass Storage 采用 Bulk-Only Transport

**选择：** 通过 BOT 协议执行 SCSI READ(10) / WRITE(10)；暴露为 `DEV_USB_DISK` + `DEV_USB_PART` 块设备，ioctl/read/write 语义与 IDE 一致。

**理由：** U 盘通用标准；可接入现有 `device_read` / `buffer` / Minix 路径。

### 6. 根挂载：USB GPT 分区索引 1（第二分区）

**选择：** USB 初始化后，`mount_root` 找到第一块 USB 磁盘、解析 GPT、挂载索引 1 的分区（Minix P2）。USB 启动构建下不做 IDE 回退（`ONIX_USB_BOOT` 或运行时检测）。

**理由：** 专用 U 盘镜像布局固定，选择逻辑最简单可靠。GRUB 无需挂载 Minix。

**备选方案：**

- Multiboot2 命令行 `root=usb0p2`：更灵活，延后
- 按 Linux FS 类型 GUID 扫描：双分区镜像下等价，代码略多

### 7. 构建目标 `usb-efi.mk` 与 `cdrom.mk` 分离

**选择：** 新建 makefile 片段；输出 `onix_usb.img`；宿主依赖 `grub-efi-amd64`、`dosfstools`、`parted` 或 `sgdisk`。

**理由：** BIOS ISO（`grub-mkrescue`）与 UEFI U 盘镜像工具链不同，分离可避免破坏现有流程。

### 8. 开发期 QEMU 冒烟，实机端到端门禁

**选择：** 在 `cmd.mk` 增加 `qemu-usb` / `qemu-usb-xhci` 目标；每个 Phase 先 QEMU 再实机；§9 实机端到端为唯一完成标准。

**理由：** 镜像/GRUB/xHCI 问题在 QEMU 上迭代更快；避免实机调试与镜像构建问题耦合。QEMU `qemu-xhci` 与 Intel xHCI 行为有差异，不能省实机验收。

**备选方案：**

- 纯实机调试：周期长，Issue 难定位
- 仅 QEMU 验收：无法覆盖 UEFI 交棒与目标硬件

## 风险与权衡

| 风险 | 缓解措施 |
|------|----------|
| xHCI 复杂度高（ring、事件处理） | QEMU 上先跑通再实机；分阶段 reset → 枚举 → bulk；串口调试 |
| UEFI 交棒后 xHCI 状态未知 | 驱动初始化时完整 HC 复位 + 端口复位；**实机 Phase 必测** |
| GPT 解析错误导致挂错分区 | 启动打印分区表；宿主 + QEMU 读 P2 超级块；实机确认 |
| Minix 在闪存上磨损 / 掉电损坏 | 文档说明为演示限制；教学 OS 与硬盘场景相同 |
| Secure Boot 阻止未签名 GRUB | 文档说明在 BIOS 中关闭 Secure Boot |
| QEMU 与实机行为不一致 | QEMU 作冒烟；端到端以 i5-5200U 为准 |
| UEFI 下 32 位内核内存映射 | QEMU OVMF 先验 multiboot2 mmap；实机 Phase 1 再确认 |

## 迁移计划

1. **Phase 1 — 镜像 + GRUB（无 USB 驱动）**
   - QEMU：OVMF 启动、`onix_usb.img`、GRUB 加载内核（预期 `mount_root` panic）
   - 实机：`dd` 到 U 盘，i5-5200U USB 3.0 同样验证

2. **Phase 2 — GPT + 块设备层**
   - 宿主扇区转储 / 逻辑测试
   - QEMU：接入后读 P2 超级块（可与 Phase 3 合并冒烟）

3. **Phase 3 — xHCI + Mass Storage**
   - QEMU：`qemu-xhci` + `usb-storage` 枚举与 BOT 读写
   - 实机：Intel xHCI 枚举与读写

4. **Phase 4 — 根挂载与端到端**
   - QEMU：用户态、写 `/hello.txt`、跑 `/bin` 程序
   - **实机：同上，作为变更完成门禁**

5. **回滚：** 传统 ISO / IDE `master.img` 构建不受影响；USB 镜像为增量能力

## 待决问题

- 使用编译期 `ONIX_USB_BOOT` 还是运行时自动检测纯 USB 配置（v1 建议 `ONIX_USB_BOOT`，避免改变默认 IDE `mount_root` 行为）。
- ESP 确切大小（默认 256MB；可在构建脚本中调整）。
- `ONIX_USB_BOOT` 下是否完全跳过 `ide_init()`（次要启动时间优化）。
- QEMU 启动时整盘 `onix_usb.img` 同时作为 virtio 磁盘与 `usb-storage` 后端是否足够，或需拆分镜像（实现 Phase 3 时确定）。
