RPI_KERNEL_DEVICETREE_OVERLAYS:append = " \
    overlays/mpu6050-iio.dtbo \
    overlays/inmp441-study.dtbo \
    overlays/srf04.dtbo \
    overlays/i2c-pwm-pca9685a.dtbo \
    overlays/pwm-2chan.dtbo \
    "

ENABLE_I2C = "1"
RASPBERRYPI_CAMERA_V2 = "1"
KERNEL_MODULE_AUTOLOAD += "i2c-dev"

RPI_EXTRA_CONFIG:append = "\ndtoverlay=mpu6050-iio\ndtoverlay=i2c-pwm-pca9685a,addr=0x40\ndtoverlay=inmp441-study\ndtoverlay=srf04\ndtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4"
