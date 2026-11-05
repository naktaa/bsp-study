ifneq ($(KERNELRELEASE),)
obj-m := platform_drv.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
DTC ?= $(KDIR)/scripts/dtc/dtc
PI_HOST := pi
PI_MODULE_DIR := bsp/modules
PI_OVERLAY_DIR := bsp/overlays

.PHONY: all modules overlay deploy clean

all: modules overlay

modules:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

overlay: platform-overlay.dtbo

platform-overlay.dtbo: platform-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

deploy:
	scp platform_drv.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp platform-overlay.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) platform-overlay.dtbo
endif
