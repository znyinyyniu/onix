1. 日志按级别打印，灵活控制日志输出
2. 新增USBLOG函数，便于将 USB 引导期里程碑日志在 USB 启动构建下直接走 printk（经帧缓冲上屏，不受 ONIX_LOG_LEVEL 裁剪，便于实体机无串口时定位失败阶段）
3. memory_init 兼容UEFI mmap
