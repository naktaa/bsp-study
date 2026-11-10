# 현재 진행 상태

## 현재 단계
- 2부 진행 중이다. 11단계 U-Boot 경유 부팅과 13단계 카메라 IMX219를 완료했다.
- 카메라는 `imx219`·unicam 드라이버로 연결되고 노출 요청값·20fps 프레임 간격(49.985ms)까지 확인했다([카메라 문서](../examples/camera/camera.md)).
- 기존 카드 `config.txt`는 직접 부팅·`camera_auto_detect=1` 상태다. 미검증·한계는 각 학습 문서 6장에 있다.

## 다음 작업
- 14단계 I2S 마이크: VEDA Final에서 쓴 마이크 모델을 먼저 확인하고 작업 노트를 만든다.
- 12단계 Yocto 최소 이미지: 새 SD 카드가 오면 데스크톱에서 [T-B02 초안](tasks/T-B02-yocto-minimal.md)을 참고해 작업 노트를 만든다.
- 이후 15~17단계 장치를 하나씩 진행하고 18단계에서 통합한다.

## 완료한 학습 문서
- [첫 사용자 프로그램](../examples/hello/cross-compile.md)
- [커널 소스·설정·빌드·부팅·복구](kernel-build.md)
- [모듈 기초](../examples/hello_module/kernel-module.md)
- [문자 디바이스](../examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](../examples/platform_device/platform-device.md)
- [MPU6050 I2C·IRQ·FIFO](../examples/mpu6050/mpu6050.md)
- [MPU6050 IIO·버퍼·mainline 비교](../examples/mpu6050_iio/iio-direct.md)
- [U-Boot 경유 부팅](../boot/uboot/uboot-boot.md)
- [카메라 IMX219](../examples/camera/camera.md)

환경·배선은 [bringup](bringup.md), 전체 순서와 단계 상태는 [roadmap](roadmap.md).
