CROSS_COMPILE ?= aarch64-linux-gnu-
APP_CC ?= $(CROSS_COMPILE)gcc
APP_CFLAGS ?= -Wall -Wextra -O2
PI_HOST := pi
PI_APP_DIR := bsp/apps

.PHONY: all app deploy clean

all: app

app: mic_capture

mic_capture: mic_capture.c
	$(APP_CC) $(APP_CFLAGS) $< -o $@ -lasound -lm

deploy:
	scp mic_capture $(PI_HOST):$(PI_APP_DIR)/
	scp mic_plot.py $(PI_HOST):$(PI_APP_DIR)/

clean:
	$(RM) mic_capture
