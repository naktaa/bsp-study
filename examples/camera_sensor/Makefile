ifneq ($(KERNELRELEASE),)
obj-m := imx219_study.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
DTC ?= $(KDIR)/scripts/dtc/dtc
PI_HOST := pi
PI_MODULE_DIR := bsp/modules
PI_OVERLAY_DIR := bsp/overlays

.PHONY: modules overlay deploy clean

modules:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

overlay: imx219-study.dtbo

imx219-study.dtbo: imx219-study-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

deploy: modules overlay
	scp imx219_study.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp imx219-study.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) imx219-study.dtbo
endif
