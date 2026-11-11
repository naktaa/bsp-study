# 현재 진행 상태

## 현재 단계
- 2부 진행 중이다. 11 U-Boot, 13 카메라, 14 I2S 마이크를 완료했다.
- 마이크는 기존 드라이버(`bcm2835-i2s`·`ics43432`·`simple-audio-card`)를 직접 작성한 overlay로 묶어 녹음까지 확인했고, 14-3에서 코덱을 직접 작성한 `inmp441_codec`으로 바꿔 probe 지연·모듈 해제·자동 적재를, 14-4에서 alsa-lib 캡처 앱으로 period·buffer와 xrun·실시간 파형을 확인했다([마이크 문서](../examples/mic/mic.md)).
- 기존 카드 `config.txt`는 원복 상태(직접 부팅, `camera_auto_detect=1`, 마이크 overlay 없음)다. 40핀 배치 계획은 [bringup](bringup.md#40핀-한눈에-보기)에 있다.

## 다음 작업
- 15단계 PCA9685 팬·틸트: I2C1을 IMU와 공유하고 VCC 3.3V·서보 외부 전원을 확인한 뒤 작업 노트를 만든다.
- 12단계 Yocto 최소 이미지: 새 SD 카드가 오면 데스크톱에서 [T-B02 초안](tasks/T-B02-yocto-minimal.md)을 참고해 작업 노트를 만든다.
- 이후 16·17단계 장치를 진행하고 18단계에서 통합한다. 마이크 CPU DAI(I2S·DMA) 직접 작성은 선택 과제로 남긴다.

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
- [I2S 마이크 INMP441](../examples/mic/mic.md)

환경·배선은 [bringup](bringup.md), 전체 순서와 단계 상태는 [roadmap](roadmap.md).
