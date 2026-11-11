KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
DTC ?= $(KDIR)/scripts/dtc/dtc
PI_HOST := pi
PI_OVERLAY_DIR := bsp/overlays

.PHONY: all overlay deploy clean

all: overlay

overlay: inmp441.dtbo

inmp441.dtbo: inmp441-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

deploy:
	scp inmp441.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/

clean:
	$(RM) inmp441.dtbo
