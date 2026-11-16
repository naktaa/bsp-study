# bsp-study

Raspberry Pi 4에서 장치 6개를 커널 드라이버·Device Tree로 연결하고 U-Boot·Yocto 이미지로 통합한 BSP 학습 저장소

```text
hardware   MPU6050   IMX219    INMP441   PCA9685   HC-SR04   PWM
DT         overlay*  overlay*  overlay*  overlay   overlay*  overlay
kernel     IIO*      V4L2*     ASoC*     PWM       IIO       PWM
user       iio:dev   video0    pcmC*c    pwmchip   sysfs     pwmchip
```
`*` 직접 작성. 상세 지도는 [00-overview](docs/learning/00-overview.md).

## 무엇을 했나
- **IMU:** threaded IRQ + 센서 FIFO를 쓰는 misc 드라이버를 직접 작성한 뒤 IIO 드라이버(trigger·buffer·timestamp)로 다시 작성, mainline `inv_mpu6050`과 비교
- **카메라:** 고정 모드 IMX219 센서 드라이버 `imx219_study`를 직접 작성해 unicam·libcamera와 연결, libcamera 없이 쓰는 V4L2 캡처 앱
- **마이크:** INMP441 ASoC 코덱 드라이버·overlay와 alsa-lib 캡처 앱
- **PCA9685·초음파·모터 PWM:** 업스트림 드라이버를 DT로 연결. PCA9685는 비교용 provider를 직접 작성, 초음파는 config fragment로 드라이버를 켜고 overlay 작성, 모터 PWM은 I2S와 겹치지 않게 GPIO12/13으로 옮김
- **부팅·이미지:** 커널 크로스 빌드·설치·복구, U-Boot 경유 부팅, `meta-bsp-study` 레이어로 6개 장치 자동 적재, 6.6/6.18 compat 헤더
- **검증:** 확인 앱 `devcheck`로 6.18 카드와 Yocto 6.6 카드 모두 6개 PASS

## 기술 스택
Linux 6.18.59 / 6.6.63 · U-Boot v2026.10(수동) / 2024.01(Yocto) · Yocto scarthgap + meta-raspberrypi + meta-openembedded · Device Tree overlay · IIO · V4L2·media controller · ASoC · PWM · WSL2 크로스 빌드

## Quick Start (Yocto 이미지)
이 저장소는 `~/bspworkspace/bsp-study`, Yocto 소스는 `~/yocto`에 둔다. 레이어 템플릿(`bblayers.conf.sample`)이 이 상대 위치를 기준으로 한다.

```bash
# [WSL] ~/yocto — 소스와 Wi-Fi 값(저장소 밖, WIFI_SSID·WIFI_PSK 두 줄)
mkdir -p ~/yocto && cd ~/yocto
git clone -b scarthgap https://git.yoctoproject.org/poky
git clone -b scarthgap https://github.com/agherzan/meta-raspberrypi
git clone -b scarthgap https://git.openembedded.org/meta-openembedded
( umask 077; touch wifi-secret.conf )
# [WSL, tmux] 빌드 셸 — Windows PATH를 빼고, 템플릿으로 build/conf를 만든다
tmux new -A -s yocto
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
TEMPLATECONF=$HOME/bspworkspace/bsp-study/yocto/meta-bsp-study/conf/templates/bsp-study \
  source poky/oe-init-build-env build
bitbake bsp-study-image
# tmp/deploy/images/raspberrypi4-64/*.wic.bz2 → bzcat → Raspberry Pi Imager "Use custom" → SD 카드
# [Yocto] 부팅 후 (노트북에서 ssh root@<IP>)
devcheck all
```

RAM 16GB PC는 첫 빌드 전에 `build/conf/local.conf`에 `BB_NUMBER_THREADS = "3"`, `PARALLEL_MAKE = "-j 3"`을 더한다. 자세한 절차는 [components/yocto](docs/components/yocto.md), 직접 빌드 커널(64GB 카드) 경로는 [components/kernel](docs/components/kernel.md).

## 배운 점 · 대표 트러블슈팅
- **커널 교체 후 화면 없음 → `overlay_map.dtb` 누락 → overlay는 `dtbs_install`로 묶어 배포한다** ([kernel](docs/components/kernel.md#6-트러블슈팅))
- **실행 중 `dtoverlay`로 붙인 mainline IMU에 buffer가 없음 → `interrupt-parent` 미해결로 IRQ 없이 probe → IRQ가 필요한 overlay는 부팅 때 적용한다** ([imu](docs/components/imu.md#6-트러블슈팅))
- **Yocto 증분 빌드에서 `do_package` pseudo abort → 레시피 `clean`이 부른 Kbuild clean이 `${WORKDIR}` 아래 패키지 파일까지 지움 → 레시피 Makefile의 `clean`은 현재 폴더만 지운다** ([yocto](docs/components/yocto.md#6-트러블슈팅))
- **같은 1.5ms인데 PCA9685 레지스터가 카드마다 1카운트 다름 → 6.6은 올림, 6.18은 버림 → 커널 버전이 바뀌면 업스트림 드라이버 동작도 바뀐다** ([pwm](docs/components/pwm.md#6-트러블슈팅))

## 문서
- [전체 지도](docs/learning/00-overview.md): 장치·서브시스템·부팅 흐름, 복습 순서
- [컴포넌트 문서](docs/components/README.md): 장치·커널·U-Boot·Yocto별 구성, 정상 확인, 수정 위치, 장애 조치
- [학습 문서](docs/learning/): 단계별로 왜·어떻게 만들었나
- [환경·배선](docs/bringup.md) · [장치 사양](docs/hardware.md) · [학습 순서](docs/roadmap.md)
- 참고 저장소(비교 기준): [VEDA](https://github.com/naktaa/VEDA)(교육 자료·수업 코드), [VEDA_Final_Project](https://github.com/naktaa/VEDA_Final_Project)(기존 사용자 공간 구현)

## 한계
- `imx219_study`는 고정 모드 1개(3280×2464, 최대 21.19fps)이고, 레지스터 초기값은 mainline `imx219`에서 가져왔다.
- `mpu6050_iio`는 센서 FIFO 없이 IRQ마다 한 샘플을 읽고, 100Hz·±2g·±250°/s로 고정했다.
- `srf04`는 sysfs를 읽을 때마다 한 번 측정하므로 수집 주기는 사용자 공간이 정한다.
