KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
DTC ?= $(KDIR)/scripts/dtc/dtc
PI_HOST := pi
PI_OVERLAY_DIR := bsp/overlays

.PHONY: all overlay deploy clean

all: overlay

overlay: srf04.dtbo

srf04.dtbo: srf04-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

deploy:
	scp srf04.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/

clean:
	$(RM) srf04.dtbo
