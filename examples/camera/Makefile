CROSS_COMPILE ?= aarch64-linux-gnu-
APP_CC ?= $(CROSS_COMPILE)gcc
APP_CFLAGS ?= -Wall -Wextra -O2
PI_HOST := pi
PI_APP_DIR := bsp/apps

.PHONY: all app deploy clean

all: app

app: v4l2_capture

v4l2_capture: v4l2_capture.c
	$(APP_CC) $(APP_CFLAGS) $< -o $@

deploy: app
	scp v4l2_capture raw_tool.py $(PI_HOST):$(PI_APP_DIR)/

clean:
	$(RM) v4l2_capture
