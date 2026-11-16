SUMMARY = "IMX219 sensor study driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:${THISDIR}/../../../../examples/camera_sensor:"

SRC_URI = " \
    file://imx219_study.c \
    file://imx219_compat.h \
    file://Makefile \
    "
S = "${WORKDIR}"

inherit module
