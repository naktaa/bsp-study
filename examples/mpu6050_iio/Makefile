ifneq ($(KERNELRELEASE),)
obj-m := mpu6050_iio.o
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

overlay: mpu6050-iio-overlay.dtbo

mpu6050-iio-overlay.dtbo: mpu6050-iio-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

app: mpu6050_iio_app

mpu6050_iio_app: mpu6050_iio_app.c
	$(APP_CC) $(APP_CFLAGS) $< -o $@

deploy:
	scp mpu6050_iio.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp mpu6050-iio-overlay.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/
	scp mpu6050_iio_app $(PI_HOST):$(PI_APP_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) mpu6050-iio-overlay.dtbo mpu6050_iio_app
endif
