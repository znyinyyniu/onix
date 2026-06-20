GRUB_EFI_DIR:= /usr/lib/grub/x86_64-efi
OVMF:= /usr/share/OVMF/OVMF_CODE.fd
USB_LOOP:= $(shell sudo losetup --find 2>/dev/null)
USB_ESP_MNT:= /tmp/onix-usb-esp
USB_ROOT_MNT:= /tmp/onix-usb-root
USB_IMG_SIZE:= 256M
USB_ESP_END:= 33MiB

.PHONY: check-usb-host-deps
check-usb-host-deps:
	@command -v grub-install >/dev/null || \
		(echo "缺少 grub-install，请执行: sudo apt install grub-efi-amd64" && exit 1)
	@command -v grub-mkimage >/dev/null || \
		(echo "缺少 grub-mkimage，请执行: sudo apt install grub-common" && exit 1)
	@test -f $(GRUB_EFI_DIR)/multiboot2.mod || \
		(echo "缺少 UEFI GRUB 模块 $(GRUB_EFI_DIR)/multiboot2.mod" && \
		 echo "请执行: sudo apt install grub-efi-amd64" && exit 1)
	@command -v parted >/dev/null && command -v mkfs.vfat >/dev/null && command -v mkfs.minix >/dev/null || \
		(echo "请执行: sudo apt install dosfstools parted minix" && exit 1)
	@command -v qemu-system-x86_64 >/dev/null || \
		(echo "缺少 qemu-system-x86_64，请执行: sudo apt install qemu-system-x86" && exit 1)
	@echo "USB 镜像宿主依赖检查通过"

$(BUILD)/onix_usb.img: $(BUILD)/kernel.bin \
	$(SRC)/utils/grub-uefi.cfg \
	$(SRC)/utils/usb-populate-root.sh \
	$(SRC)/utils/network.conf \
	$(SRC)/utils/resolv.conf \
	$(BUILTIN_APPS) \
	$(BUILD)/mono.wav \
	$(BUILD)/stereo.wav
	$(MAKE) check-usb-host-deps
	test -f $(OVMF) || test -f /usr/share/OVMF/OVMF.fd || \
		(echo "需要安装: sudo apt install ovmf" && false)
	grub-file --is-x86-multiboot2 $(BUILD)/kernel.bin

	rm -f $@
	truncate -s $(USB_IMG_SIZE) $@
	parted -s $@ mklabel gpt
	parted -s $@ mkpart ESP fat32 1MiB $(USB_ESP_END)
	parted -s $@ set 1 esp on
	parted -s $@ mkpart primary $(USB_ESP_END) 100%

	sudo losetup $(USB_LOOP) --partscan $@
	sudo mkfs.vfat -F 16 -n ONIXESP $(USB_LOOP)p1
	sudo mkfs.minix -1 -n 14 $(USB_LOOP)p2

	sudo mkdir -p $(USB_ESP_MNT)
	sudo mount $(USB_LOOP)p1 $(USB_ESP_MNT)
	sudo mkdir -p $(USB_ESP_MNT)/boot/grub

	sudo grub-install --target=x86_64-efi \
		--efi-directory=$(USB_ESP_MNT) \
		--boot-directory=$(USB_ESP_MNT)/boot \
		--removable \
		--no-nvram \
		$(USB_LOOP)

	sudo cp $(BUILD)/kernel.bin $(USB_ESP_MNT)/boot/kernel.bin
	sudo cp $(SRC)/utils/grub-uefi.cfg $(USB_ESP_MNT)/boot/grub/grub.cfg

	sudo umount $(USB_ESP_MNT)

	sudo mkdir -p $(USB_ROOT_MNT)
	sudo mount $(USB_LOOP)p2 $(USB_ROOT_MNT)
	sudo chown $(USER) $(USB_ROOT_MNT)
	bash $(SRC)/utils/usb-populate-root.sh $(USB_ROOT_MNT)
	sudo umount $(USB_ROOT_MNT)
	sudo losetup -d $(USB_LOOP)

	cp $@ $(BUILD)/onix_$(ONIX_VERSION)_usb.img

USB_IMAGES:= $(BUILD)/onix_usb.img

# 仅更新 ESP 中的内核与 grub.cfg，无需 sudo 重建整盘镜像
.PHONY: usb-patch-esp
usb-patch-esp: $(BUILD)/kernel.bin $(SRC)/utils/grub-uefi.cfg
	@test -f $(BUILD)/onix_usb.img || (echo "需要先 make usb-image 生成 onix_usb.img" && exit 1)
	@command -v mcopy >/dev/null || (echo "请安装: sudo apt install mtools" && exit 1)
	grub-file --is-x86-multiboot2 $(BUILD)/kernel.bin
	mcopy -o -i $(BUILD)/onix_usb.img@@2048s $(BUILD)/kernel.bin ::boot/kernel.bin
	mcopy -o -i $(BUILD)/onix_usb.img@@2048s $(SRC)/utils/grub-uefi.cfg ::boot/grub/grub.cfg
	@echo "已更新 ESP: kernel.bin + grub.cfg"
