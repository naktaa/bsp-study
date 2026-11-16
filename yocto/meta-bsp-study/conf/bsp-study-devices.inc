RPI_KERNEL_DEVICETREE_OVERLAYS:append = " \
    overlays/mpu6050-iio.dtbo \
    overlays/inmp441-study.dtbo \
    overlays/srf04.dtbo \
    overlays/i2c-pwm-pca9685a.dtbo \
    overlays/pwm-2chan.dtbo \
    overlays/imx219-study.dtbo \
    "

ENABLE_I2C = "1"
KERNEL_MODULE_AUTOLOAD += "i2c-dev"

RPI_EXTRA_CONFIG:append = "\ndtoverlay=mpu6050-iio\ndtoverlay=i2c-pwm-pca9685a,addr=0x40\ndtoverlay=inmp441-study\ndtoverlay=srf04\ndtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4\ndtoverlay=imx219-study"

# firmware가 만든 DT를 U-Boot를 거쳐 커널에 전달한다.
RPI_USE_U_BOOT = "1"
