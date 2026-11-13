SUMMARY = "Wait for wlan0 and connect to Wi-Fi during boot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://bsp-study-wifi"
S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "bsp-study-wifi"
INITSCRIPT_PARAMS = "start 02 5 . stop 80 0 6 1 ."

RDEPENDS:${PN} = "wpa-supplicant"

do_install () {
	install -d ${D}${sysconfdir}/init.d
	install -m 0755 ${WORKDIR}/bsp-study-wifi ${D}${sysconfdir}/init.d/bsp-study-wifi
}
