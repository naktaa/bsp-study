CROSS_COMPILE ?= aarch64-linux-gnu-
APP_CC ?= $(CROSS_COMPILE)gcc
APP_CFLAGS ?= -Wall -Wextra -O2
PI_HOST := pi
PI_APP_DIR := bsp/apps

.PHONY: all app deploy clean

all: app

app: devcheck

devcheck: devcheck.c
	$(APP_CC) $(APP_CFLAGS) $< -o $@ -lasound -lm

deploy:
	scp devcheck $(PI_HOST):$(PI_APP_DIR)/

clean:
	$(RM) devcheck
