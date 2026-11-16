# 전체 지도: 6개 장치 · 커널 서브시스템 · 부팅·빌드

Raspberry Pi 4에서 VEDA Final 장치 6개를 BSP 관점으로 연결하고, U-Boot·Yocto로 묶은 과정의 입구 문서다. 각 장치·단계의 상세는 표의 링크를 따라간다.

## 1. 전체 지도

```text
             IMU             camera            mic               PCA9685          sonar            motor PWM
hardware     MPU6050         IMX219            INMP441           PCA9685          HC-SR04          (on-SoC PWM)
             I2C1 0x68       CSI-2 2 lanes     I2S GPIO18-21     I2C1 0x40        GPIO23 / 24      GPIO12 / 13
             INT GPIO17      + I2C i2c-10
                |                 |                 |                 |                |                |
DT/overlay   mpu6050-iio*    imx219-study*     inmp441-study*    i2c-pwm-         srf04*           pwm-2chan
                |                 |                 |            pca9685a              |                |
kernel       mpu6050_iio*    imx219_study* +   bcm2835-i2s       pwm-pca9685      srf04            pwm-bcm2835
             IIO trigger +   unicam, V4L2      inmp441_codec*    PWM core         IIO              PWM core
             buffer          subdev + media    simple-card ASoC
                |                 |                 |                 |                |                |
user space   /dev/iio:devN   /dev/video0       /dev/snd/pcmC*c   /sys/class/pwm   in_distance_raw  /sys/class/pwm
             CSV app         rpicam-*          mic_capture       pwmchipN         (IIO sysfs)      pwmchip0
```

`*`는 직접 작성한 것이다. 나머지는 업스트림 드라이버·커널 트리 overlay를 DT로 연결했다. 모든 장치는 [devcheck](../../examples/devcheck/devcheck.c) 하나로 두 카드에서 같은 방식으로 확인한다([18](18-integration.md)).

## 2. 장치 표

| 장치 | 서브시스템 | 드라이버 (직접 / 업스트림) | 사용자 인터페이스 | 학습 문서 | 운영 문서 |
| --- | --- | --- | --- | --- | --- |
| IMU MPU6050 | I2C + IRQ, misc → IIO | 직접: misc FIFO 드라이버(06), IIO 드라이버 `mpu6050_iio`(09). 비교: mainline `inv_mpu6050` | `/dev/mpu6050`(06), `/dev/iio:deviceN` + sysfs(09) | [06](06-mpu6050.md), [09](09-iio-direct.md) | [imu](../components/imu.md) |
| 카메라 IMX219 | V4L2 + media controller | 업스트림 unicam. 직접: 센서 드라이버 `imx219_study`(13-3·13-4, Yocto는 18-2부터), V4L2 캡처 앱. 비교: mainline `imx219` | `/dev/video0`, `/dev/v4l-subdev0`, libcamera `rpicam-*` | [13](13-camera.md) | [camera](../components/camera.md) |
| 마이크 INMP441 | ALSA ASoC | 업스트림 `bcm2835-i2s`·`simple-audio-card`. 직접: 코덱 `inmp441_codec`, alsa-lib 앱 | `/dev/snd/pcmC*D0c`, `arecord`, `mic_capture` | [14](14-mic.md) | [mic](../components/mic.md) |
| PCA9685 | PWM | 업스트림 `pwm-pca9685`. 직접: 비교용 provider `pca9685_simple` | `/sys/class/pwm/pwmchipN` | [15](15-pca9685.md) | [pwm](../components/pwm.md) |
| 초음파 HC-SR04 | IIO | 업스트림 `srf04`(config fragment로 켬). 직접: overlay | `in_distance_raw`(mm) | [16](16-ultrasonic.md) | [ultrasonic](../components/ultrasonic.md) |
| 모터 PWM | PWM | 업스트림 `pwm-bcm2835`, 커널 트리 `pwm-2chan` | `/sys/class/pwm/pwmchipN` ch0·ch1 | [18](18-integration.md) | [pwm](../components/pwm.md) |

학습 문서는 왜·어떻게 만들었는지, 운영 문서([components](../components/README.md))는 정상 확인·수정 위치·장애 조치를 다룬다. 커널·U-Boot·Yocto 운영 문서는 [kernel](../components/kernel.md)·[uboot](../components/uboot.md)·[yocto](../components/yocto.md)다.

핀·주소 공유: I2C1에 IMU(0x68)와 PCA9685(0x40)가 같이 있고, I2S가 GPIO18~21을 쓰므로 모터 PWM은 GPIO12/13으로 옮겼다. 상세 배선은 [bringup](../bringup.md#40핀-한눈에-보기).

## 3. 부팅·빌드 흐름

**부팅:** Pi firmware(`start4.elf` + `config.txt`, DTB·overlay 적용) → (선택) U-Boot `boot.scr` → kernel + DT → rootfs

| 카드 | 커널 | rootfs | 만든 방법 |
| --- | --- | --- | --- |
| 64GB | 6.18.59-v8-bsp-study01 | Debian 13 | WSL에서 커널 크로스 빌드, 장치마다 외부 모듈·overlay 수동 배포([07](07-kernel-build.md)) |
| 32GB | 6.6.63-v8 | Yocto `bsp-study-image` | `meta-bsp-study` 레이어로 통합 빌드([12](12-yocto-minimal.md) → [18](18-integration.md)), 18-2부터 U-Boot 2024.01 경유 |

- U-Boot는 firmware가 만든 DT를 그대로 넘긴다. 장치 노드는 바꾸지 않고 `/chosen/u-boot,version`과 비활성 `framebuffer` 노드만 더한다([11](11-uboot-boot.md)).
- Yocto 이미지에서는 커널 config 조각·overlay·`config.txt` 줄·직접 작성 모듈이 레시피로 들어가 부팅만 하면 자동 적재된다.
- 직접 작성한 `mpu6050_iio`·`imx219_study`는 compat 헤더로 6.6과 6.18 모두에서 빌드된다.

## 4. 단계별 한 줄 요약

| 단계 | 한 줄 요약 | 문서 |
| --- | --- | --- |
| 0 | 호스트·보드·커널·센서 상태와 참고 경로 확인 | [bringup](../bringup.md) |
| 1 | WSL에서 ARM64 사용자 프로그램을 크로스 빌드해 Pi에서 실행 | [01](01-cross-compile.md) |
| 2 | Kbuild 외부 모듈 빌드·vermagic·insmod/rmmod | [02](02-kernel-module.md) |
| 3 | 문자 디바이스로 open·write·read·ioctl, copy_*_user·mutex | [03](03-char-device.md) |
| 4 | MPU6050 레지스터 식별·설정·값 변환(부분: 회전 로그 미확보) | [06](06-mpu6050.md) |
| 5 | DT 노드 → compatible 매칭 → probe/remove → sysfs | [05](05-platform-device.md) |
| 6 | 최소 I2C 센서 드라이버(misc `/dev/mpu6050`) | [06](06-mpu6050.md) |
| 7 | 커널 소스 고정·크로스 빌드·별도 이름 설치·initramfs·복구 | [07](07-kernel-build.md) |
| 8 | threaded IRQ + 센서 FIFO drain + 커널 큐 | [06](06-mpu6050.md) |
| 9 | IIO 채널·scale·trigger·buffer·timestamp, mainline 비교 | [09](09-iio-direct.md) |
| 10 | 측정·디버깅·재현(진행: 60초 정지 기준선 확보, 회전 검증 남음) | [09 6장](09-iio-direct.md#6-트러블슈팅과-한계) |
| 11 | U-Boot 경유 부팅, DT·bootargs 전달 확인(Yocto 이미지 적용은 18-2) | [11](11-uboot-boot.md) |
| 12 | 직접 만든 레이어로 Yocto 최소 이미지·Wi-Fi SSH | [12](12-yocto-minimal.md) |
| 13 | IMX219 센서·unicam 연결, 노출·타임스탬프 경로, libcamera 없이 캡처, 고정 모드 센서 드라이버 직접 작성(13-3)·libcamera 연동(13-4) | [13](13-camera.md) |
| 14 | INMP441 ASoC 연결, 코덱 드라이버, alsa-lib 캡처 앱 | [14](14-mic.md) |
| 15 | PCA9685 업스트림 PWM 기준값과 직접 작성 provider 비교 | [15](15-pca9685.md) |
| 16 | config fragment로 IIO `srf04` 켜기, 거리 값 확인 | [16](16-ultrasonic.md) |
| 17 | 모터 PWM(제외: 18단계에서 sysfs 값만 확인) | [18](18-integration.md) |
| 18 | Yocto 6.6 이미지 하나에 6개 장치 통합, `devcheck`로 두 카드 비교. 18-2: U-Boot 2024.01과 직접 작성 카메라 드라이버 | [18](18-integration.md) |

## 5. 복습 순서

1. **기초:** [01](01-cross-compile.md) → [02](02-kernel-module.md) → [03](03-char-device.md) → [05](05-platform-device.md). 크로스 빌드, 모듈, 사용자/커널 경계, DT 매칭.
2. **IMU 심화:** [06](06-mpu6050.md) → [09](09-iio-direct.md). 직접 만든 IRQ·FIFO·큐가 IIO에서 무엇으로 바뀌는지.
3. **부팅·빌드:** [07](07-kernel-build.md) → [11](11-uboot-boot.md) → [12](12-yocto-minimal.md). 커널 설치·복구, 부트로더, 레이어·이미지 레시피.
4. **장치:** [13](13-camera.md) → [14](14-mic.md) → [15](15-pca9685.md) → [16](16-ultrasonic.md). 업스트림 드라이버 + DT 연결, 필요한 부분만 직접 작성.
5. **통합:** [18](18-integration.md). 한 이미지에 모으고 두 커널(6.6·6.18)에서 같은 값인지 비교.
