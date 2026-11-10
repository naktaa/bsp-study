UBOOT_SRC ?= $(abspath ../../external/u-boot)
OUT ?= $(abspath ../../out/uboot-rpi4)
CROSS_COMPILE ?= aarch64-linux-gnu-
DEFCONFIG := rpi_arm64_defconfig
JOBS ?= $(shell nproc)

UBOOT_MAKE = $(MAKE) -C $(UBOOT_SRC) O=$(OUT) CROSS_COMPILE=$(CROSS_COMPILE)

.PHONY: all uboot clean

all: uboot $(OUT)/boot.scr

$(OUT)/.config:
	$(UBOOT_MAKE) $(DEFCONFIG)

uboot: $(OUT)/.config
	$(UBOOT_MAKE) -j$(JOBS)

# mkimage는 U-Boot 빌드가 만들므로 uboot 뒤에 실행한다.
$(OUT)/boot.scr: boot.cmd | uboot
	$(OUT)/tools/mkimage -A arm64 -O linux -T script -C none \
		-n "bsp-study boot" -d $< $@

clean:
	rm -rf $(OUT)
