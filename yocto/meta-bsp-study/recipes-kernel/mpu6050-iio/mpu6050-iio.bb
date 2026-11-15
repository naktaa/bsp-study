SUMMARY = "MPU6050 IIO study driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:${THISDIR}/../../../../examples/mpu6050_iio:"

SRC_URI = " \
    file://mpu6050_iio.c \
    file://mpu6050_compat.h \
    file://Makefile \
    "
S = "${WORKDIR}"

inherit module
