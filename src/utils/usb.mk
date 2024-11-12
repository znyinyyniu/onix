$(BUILD)/usb.img:

# 创建一个 32M 的硬盘镜像
	yes | bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@

# 执行硬盘分区
	sfdisk $@ < $(SRC)/utils/slave.sfdisk

# 挂载设备
	sudo losetup $(LOOP) --partscan $@

# 创建 minux 文件系统
	sudo mkfs.minix -1 -n 14 $(LOOP)p1

# 挂载文件系统
	sudo mount $(LOOP)p1 /mnt

# 切换所有者
	sudo chown ${USER} /mnt 

# 创建文件
	echo "hello,usb disk..." > /mnt/hello.txt

# 卸载文件系统
	sudo umount /mnt

# 卸载设备
	sudo losetup -d $(LOOP)
	
IMAGES += $(BUILD)/usb.img





