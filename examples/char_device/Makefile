ifneq ($(KERNELRELEASE),)
obj-m := char_drv.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
APP_CC ?= $(CROSS_COMPILE)gcc
APP_CFLAGS ?= -Wall -Wextra -O0 -g
PI_HOST := pi
PI_MODULE_DIR := bsp/modules
PI_APP_DIR := bsp/apps

.PHONY: all modules app deploy clean

all: modules app

modules:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

app: char_app

char_app: char_app.c char_ioctl.h
	$(APP_CC) $(APP_CFLAGS) $< -o $@

deploy:
	scp char_drv.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp char_app $(PI_HOST):$(PI_APP_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) char_app
endif
