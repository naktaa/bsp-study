ifneq ($(KERNELRELEASE),)
obj-m := inmp441_codec.o
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

overlay: inmp441-study.dtbo

inmp441-study.dtbo: inmp441-study-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

deploy:
	scp inmp441_codec.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp inmp441-study.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) inmp441-study.dtbo
endif
