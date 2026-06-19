## 新增需求

### 需求：Mass Storage 类绑定

系统必须在枚举完成后，为实现了 USB Mass Storage 类（class 8、subclass 6、protocol Bulk-Only）的设备挂载驱动。

#### 场景：U 盘设备

- **当** 已枚举的 USB 设备上报 Mass Storage Bulk-Only 接口时
- **则** mass storage 驱动接管该设备

### 需求：Bulk-Only Transport 协议

mass storage 驱动必须通过 USB Mass Storage Bulk-Only Transport（CBW / 数据 / CSW）执行 SCSI 命令。

#### 场景：READ 成功

- **当** 驱动对有效 LBA 发送 SCSI READ(10) 时
- **则** 命令以 CSW 成功状态完成，并返回扇区数据

### 需求：USB 磁盘块设备

每个 mass storage 设备必须注册为块设备（`DEV_USB_DISK`），支持扇区读、扇区写及与 IDE 磁盘设备一致的扇区数量/大小 ioctl 命令。

#### 场景：整盘扇区数量

- **当** 用户代码查询 USB 磁盘设备的扇区数量时
- **则** 返回值与设备上报的容量一致

### 需求：可写扇区

mass storage 驱动必须对可写 USB 设备支持 SCSI WRITE(10)。

#### 场景：扇区写入

- **当** 内核向 USB 磁盘块设备写入完整扇区时
- **则** 随后对同一 LBA 的读取返回已写入的数据
