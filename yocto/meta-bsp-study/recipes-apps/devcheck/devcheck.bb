SUMMARY = "BSP study device integration checks"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

FILESEXTRAPATHS:prepend := "${THISDIR}/../../../../examples/devcheck:"

SRC_URI = "file://devcheck.c"
S = "${WORKDIR}"

DEPENDS = "alsa-lib"

do_compile() {
	${CC} ${CFLAGS} ${LDFLAGS} ${S}/devcheck.c -o devcheck -lasound -lm
}

do_install() {
	install -d ${D}${bindir}
	install -m 0755 ${B}/devcheck ${D}${bindir}/devcheck
}
