# 현재 진행 상태

## 현재 단계
- 1부(0~10단계)를 마치고 2부를 진행 중이다. 11단계 U-Boot 경유 부팅을 완료했다.
- 13단계 카메라 IMX219 작업 노트([13-camera](tasks/13-camera.md))를 만들었고, 카메라 연결과 보드 확인을 기다린다.
- 기존 카드 `config.txt`는 직접 부팅(`kernel=kernel8-bsp-study01.img`) 상태다. 미검증·한계는 각 학습 문서 6장에 있다.

## 다음 작업
- 13단계: 카메라 연결 후 작업 노트 진행 순서대로 바인딩·media 그래프·사진·노출·프레임 간격을 확인한다.
- 12단계 Yocto 최소 이미지: 새 SD 카드가 오면 데스크톱에서 [T-B02 초안](tasks/T-B02-yocto-minimal.md)을 참고해 작업 노트를 만든다.
- 이후 14~17단계 장치를 하나씩 진행하고 18단계에서 통합한다.

## 완료한 학습 문서
- [첫 사용자 프로그램](../examples/hello/cross-compile.md)
- [커널 소스·설정·빌드·부팅·복구](kernel-build.md)
- [모듈 기초](../examples/hello_module/kernel-module.md)
- [문자 디바이스](../examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](../examples/platform_device/platform-device.md)
- [MPU6050 I2C·IRQ·FIFO](../examples/mpu6050/mpu6050.md)
- [MPU6050 IIO·버퍼·mainline 비교](../examples/mpu6050_iio/iio-direct.md)
- [U-Boot 경유 부팅](../boot/uboot/uboot-boot.md)

환경·배선은 [bringup](bringup.md), 전체 순서와 단계 상태는 [roadmap](roadmap.md).
