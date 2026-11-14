ifneq ($(KERNELRELEASE),)
obj-m := pca9685_simple.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
PI_HOST := pi
PI_MODULE_DIR := bsp/modules

.PHONY: all modules deploy clean

all: modules

modules:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

deploy:
	scp pca9685_simple.ko $(PI_HOST):$(PI_MODULE_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
endif
