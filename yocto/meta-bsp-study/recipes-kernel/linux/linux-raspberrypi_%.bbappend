FILESEXTRAPATHS:prepend := "${THISDIR}/files:${THISDIR}/../../../../examples/mpu6050_iio:${THISDIR}/../../../../examples/mic_codec:${THISDIR}/../../../../examples/ultrasonic:${THISDIR}/../../../../examples/camera_sensor:"

SRC_URI += " \
    file://bsp-study.cfg \
    file://mpu6050-iio-overlay.dts \
    file://inmp441-study-overlay.dts \
    file://srf04-overlay.dts \
    file://imx219-study-overlay.dts \
    "

# checkout과 work-shared 이동이 끝난 소스에 배치한다.
do_patch:append() {
	install -m 0644 ${WORKDIR}/mpu6050-iio-overlay.dts ${S}/arch/arm/boot/dts/overlays/
	install -m 0644 ${WORKDIR}/inmp441-study-overlay.dts ${S}/arch/arm/boot/dts/overlays/
	install -m 0644 ${WORKDIR}/srf04-overlay.dts ${S}/arch/arm/boot/dts/overlays/
	install -m 0644 ${WORKDIR}/imx219-study-overlay.dts ${S}/arch/arm/boot/dts/overlays/
}
