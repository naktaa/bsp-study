ifneq ($(KERNELRELEASE),)
obj-m := mpu6050_drv.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
APP_CC ?= $(CROSS_COMPILE)gcc
APP_CFLAGS ?= -Wall -Wextra -O0 -g
DTC ?= $(KDIR)/scripts/dtc/dtc
PI_HOST := pi
PI_MODULE_DIR := bsp/modules
PI_OVERLAY_DIR := bsp/overlays
PI_APP_DIR := bsp/apps

.PHONY: all modules overlay app deploy clean

all: modules overlay app

modules:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

overlay: mpu6050-overlay.dtbo

mpu6050-overlay.dtbo: mpu6050-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

app: mpu6050_app

mpu6050_app: mpu6050_app.c mpu6050_data.h
	$(APP_CC) $(APP_CFLAGS) $< -o $@

deploy:
	scp mpu6050_drv.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp mpu6050-overlay.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/
	scp mpu6050_app $(PI_HOST):$(PI_APP_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) mpu6050-overlay.dtbo mpu6050_app
endif
