# UEFI U 盘镜像

## Purpose

构建可 `dd` 写入 U 盘的 GPT 原始磁盘镜像，包含 ESP（GRUB + 内核）与 Minix 根分区。

## Requirements

### 需求：U 盘磁盘镜像构建目标

构建系统必须提供可生成原始 GPT 磁盘镜像的目标，该镜像适合通过 `dd` 写入 U 盘。

#### 场景：构建后产出镜像文件

- **当** 开发者执行 U 盘镜像构建目标时
- **则** 在构建输出目录中生成原始磁盘镜像文件

### 需求：GPT 双分区布局

U 盘镜像必须使用 GPT 分区表，且恰好包含两个数据分区：FAT32 EFI 系统分区（P1）和 Minix 根分区（P2）。

#### 场景：分区类型正确

- **当** 使用宿主分区工具检查 U 盘镜像的分区表时
- **则** P1 类型为 EF00（EFI System），P2 类型为 8304（Linux）

### 需求：ESP 包含 UEFI GRUB 与内核

P1 必须格式化为 FAT32，并包含可引导的 x86_64 UEFI GRUB 安装、`grub.cfg`，以及 GRUB 可通过 multiboot2 加载的 `kernel.bin`。

#### 场景：标准 UEFI 可移动介质路径

- **当** 列出 ESP 内容时
- **则** 存在 `EFI/BOOT/BOOTX64.EFI`，且存在 `boot/kernel.bin`

### 需求：Minix 根分区内容填充

P2 必须格式化为 Minix v1，并填充与现有 `master.img` 根分区相同的应用程序与配置布局（如 `/bin`、`/etc`、`/dev`、`/mnt`）。

#### 场景：根分区二进制文件存在

- **当** 镜像创建后在构建主机上挂载 P2 时
- **则** `/bin` 下存在内置应用程序

### 需求：Secure Boot 文档说明

项目文档必须说明：在目标机器上启动未签名 GRUB 前，须手动关闭 Secure Boot。

#### 场景：README 启动说明

- **当** 用户阅读 U 盘镜像的启动说明时
- **则** 说明中包含在固件设置中关闭 Secure Boot 的步骤
