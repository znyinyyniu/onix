# xHCI 驱动

## Purpose

提供 xHCI 主机控制器驱动，完成 UEFI 交棒后的复位、USB 枚举与 bulk 传输。

## Requirements

### 需求：xHCI PCI 发现

系统必须通过 PCI 枚举发现 xHCI 主机控制器（类代码 serial USB xHCI `0x0C0330`），并从 BAR0 初始化各控制器的 MMIO 基地址。

#### 场景：目标硬件上的 Intel 集成 xHCI

- **当** 内核在带 USB 3.0 的 Intel Core i5-5200U 级硬件上启动时
- **则** 至少发现一个 xHCI 控制器并完成初始化

### 需求：固件交棒后控制器复位

xHCI 驱动必须在端口枚举前的初始化阶段执行主机控制器复位。

#### 场景：UEFI 启动之后

- **当** 内核在 UEFI 从 U 盘加载 GRUB 之后启动时
- **则** xHCI 驱动复位控制器并进入就绪状态

### 需求：USB 设备枚举

xHCI 驱动必须检测端口上的设备连接、分配设备槽位，并完成足以供类驱动使用的标准 USB 枚举（GET_DESCRIPTOR、SET_ADDRESS、SET_CONFIGURATION）。

#### 场景：启动时连接 U 盘

- **当** 引导用 USB 口上连接了 USB Mass Storage 设备时
- **则** 设备完成枚举并可供 USB 存储类驱动使用

### 需求：Bulk 传输支持

xHCI 驱动必须支持已配置端点上的 bulk IN 与 bulk OUT 传输，供 Mass Storage 类驱动使用。

#### 场景：Bulk IN 读取

- **当** 存储驱动在 mass-storage bulk 端点上请求 bulk IN 传输时
- **则** xHCI 驱动完成传输并向调用方返回数据
