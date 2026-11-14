# 18 장치 통합 (Yocto 6.6) — Codex 지시서

작업 노트: [18-integration.md](18-integration.md)

A는 데스크톱(`~/yocto` 6.6 트리 있음, `external/linux` 없음), B는 노트북(`external/linux` 6.18 있음)에서 진행한다. 두 절의 변경 파일은 겹치지 않는다. 결정과 근거는 작업 노트 '결정과 근거'를 따른다.

## 지시 (Claude)

### A. 6.6 포팅과 레이어 (데스크톱)
- 변경 파일:
  - `examples/mpu6050_iio/mpu6050_iio.c`(include 한 줄만), 새 파일 `examples/mpu6050_iio/mpu6050_compat.h`
  - `yocto/meta-bsp-study/conf/layer.conf`(`LAYERDEPENDS`에 `multimedia-layer`)
  - `yocto/meta-bsp-study/conf/templates/bsp-study/bblayers.conf.sample`(meta-oe·meta-python·meta-multimedia 3줄, `##OEROOT##/../meta-openembedded/...`)
  - `yocto/meta-bsp-study/conf/templates/bsp-study/local.conf.sample`(`require conf/bsp-study-devices.inc` 한 줄)
  - 새 파일 `yocto/meta-bsp-study/conf/bsp-study-devices.inc`
  - 새 파일 `yocto/meta-bsp-study/recipes-kernel/linux/linux-raspberrypi_%.bbappend`, `.../linux/files/bsp-study.cfg`
  - 새 레시피 `yocto/meta-bsp-study/recipes-kernel/mpu6050-iio/`, `yocto/meta-bsp-study/recipes-kernel/inmp441-codec/`(각 `.bb`와 레시피 전용 `files/Makefile`)
  - 새 레시피 `yocto/meta-bsp-study/recipes-apps/devcheck/devcheck.bb`
  - 새 파일 `yocto/meta-bsp-study/recipes-multimedia/libcamera/libcamera_%.bbappend`
  - `yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb`
- 요구사항:
  1. **IMU 포팅:** 6.6에서 깨지는 4가지만 `mpu6050_compat.h`에서 `LINUX_VERSION_CODE`로 분기해 메운다. 드라이버 본문은 6.18 API 그대로 두고 `#include <linux/unaligned.h>` 줄을 compat 헤더 include로 바꾸는 것 외에는 고치지 않는다.
     - `<linux/unaligned.h>` (L16) → 6.6은 `<asm/unaligned.h>`
     - `iio_device_claim_direct()`/`iio_device_release_direct()` (L102, L118, bool, 성공 true) → 6.6 `iio_device_claim_direct_mode()`(int, 성공 0)/`iio_device_release_direct_mode()`를 감싼 `static inline` 함수
     - `IIO_DECLARE_BUFFER_WITH_TS(type, name, count)` (L192) → 6.6에서 같은 크기(데이터 + 8바이트 정렬 s64 timestamp 자리)의 `__aligned(8)` 배열로 정의
     - `iio_push_to_buffers_with_ts(dev, data, size, ts)` (L206) → 6.6 `iio_push_to_buffers_with_timestamp(dev, data, ts)`(6.6 `include/linux/iio/buffer.h` L37)
     - 각 분기 경계 버전은 근거(커밋·헤더)를 구현 보고에 적는다. 확인하지 못하면 "6.6·6.18 두 트리에서만 확인"으로 적는다.
  2. **커널 bbappend:** `bsp-study.cfg`는 `CONFIG_SRF04=m` 한 줄(저장소 `configs/bsp-study.config`와 같은 내용. 링크·복사 중 fetcher에서 동작하는 쪽을 고르고 이유를 적음). 직접 작성 overlay 3개(`examples/mpu6050_iio/mpu6050-iio-overlay.dts`, `examples/mic_codec/inmp441-study-overlay.dts`, `examples/ultrasonic/srf04-overlay.dts`)는 저장소에 복사본을 만들지 않고 `FILESEXTRAPATHS`로 가져와 커널 소스의 `arch/arm/boot/dts/overlays/`에 넣는다. 결과 dtbo 이름은 `mpu6050-iio.dtbo`·`inmp441-study.dtbo`·`srf04.dtbo`이다. 어느 태스크에서 넣는지(`work-shared` 소스이므로)와 이유를 적는다.
     - 구현 전에 6.6 `kernel-source/arch/arm/boot/dts/overlays/`에 `mpu6050-iio-overlay.dts`·`inmp441-study-overlay.dts`·`srf04-overlay.dts`(그리고 같은 이름의 `.dtbo` 규칙)가 없는지 확인해 결과를 구현 보고에 적는다. 하나라도 있으면 구현을 멈추고 이름 변경안을 보고한다. (Claude 읽기 확인 2026-11-15: 세 이름 모두 없음)
  3. **`bsp-study-devices.inc`:** `RPI_KERNEL_DEVICETREE_OVERLAYS:append`에 위 3개와 `overlays/i2c-pwm-pca9685a.dtbo`·`overlays/pwm-2chan.dtbo`, `ENABLE_I2C = "1"`, `RASPBERRYPI_CAMERA_V2 = "1"`, `KERNEL_MODULE_AUTOLOAD += "i2c-dev"`(노트 결정 1 근거), `RPI_EXTRA_CONFIG`에 아래 5줄. 기존 `ENABLE_UART`·Wi-Fi 설정은 건드리지 않는다.
     - 여러 줄 형식 근거: rpi-config 레시피 L304 `printf "${RPI_EXTRA_CONFIG}\n" >> $CONFIG`, meta-raspberrypi `docs/extra-build-config.md` L402~414("To add multiple lines you must include `\n` separators"). 문서 예시처럼 `\` 줄 이음 뒤에 들여쓰기를 두면 config.txt 줄 앞에 공백이 생기므로, 각 줄이 공백 없이 시작하도록 `\n`으로 구분한다. 결과가 5개의 별도 줄이 되는 이유를 구현 보고에 적는다.
     ```
     dtoverlay=mpu6050-iio
     dtoverlay=i2c-pwm-pca9685a,addr=0x40
     dtoverlay=inmp441-study
     dtoverlay=srf04
     dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4
     ```
  4. **모듈 레시피 2개:** `inherit module`. 소스(`mpu6050_iio.c`·`mpu6050_compat.h`, `inmp441_codec.c`)는 `FILESEXTRAPATHS`로 `examples/`에서 가져온다. `files/Makefile`은 `KERNEL_SRC`를 쓰고 `modules`·`modules_install` 타깃을 둔다(poky `module.bbclass` L49~69 참고). `examples/`의 Makefile은 6.18 작업용이므로 바꾸지 않는다. `KERNEL_MODULE_AUTOLOAD`는 넣지 않는다.
  5. **`devcheck.bb`:** `examples/devcheck/devcheck.c`(B가 작성)를 `${CC} ${CFLAGS} ${LDFLAGS} … -lasound -lm`로 빌드해 `${bindir}`에 설치. `DEPENDS = "alsa-lib"`.
  6. **`libcamera_%.bbappend`:** `PACKAGECONFIG:append = " gst"` 한 줄.
  7. **이미지:** `IMAGE_INSTALL:append`에 `mpu6050-iio inmp441-codec devcheck libcamera libcamera-apps libcamera-gst v4l-utils media-ctl gstreamer1.0 gstreamer1.0-plugins-base-meta gstreamer1.0-plugins-good-meta`, `IMAGE_ROOTFS_EXTRA_SPACE = "524288"`. 패키지 이름은 레시피에서 다시 확인하고 다르면 보고한다.
- 하지 말 것: 드라이버 동작·반환값·오류 처리 변경, 형변환으로 경고 숨기기, `examples/*/Makefile` 수정, overlay `.dts` 내용·compatible 변경, RTSP·plugins-bad·x264 추가, `build/conf` 수정, bitbake 실행, B의 파일(`examples/devcheck/`) 작성.
- 빌드 확인 명령:

```bash
# [WSL 데스크톱] 저장소를 건드리지 않도록 복사본에서 6.6 컴파일
W=~/yocto/build/tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git
K=~/yocto/build/tmp/work-shared/raspberrypi4-64/kernel-source
B=$W/linux-raspberrypi4_64-standard-build
export PATH=$W/recipe-sysroot-native/usr/bin/aarch64-poky-linux:$W/recipe-sysroot-native/usr/bin:$PATH
T=$(mktemp -d); cp examples/mpu6050_iio/mpu6050_iio.c examples/mpu6050_iio/mpu6050_compat.h $T/
echo 'obj-m := mpu6050_iio.o' > $T/Kbuild
make -C $K O=$B M=$T ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
$K/scripts/checkpatch.pl --no-tree -f examples/mpu6050_iio/mpu6050_iio.c
$K/scripts/checkpatch.pl --no-tree -f examples/mpu6050_iio/mpu6050_compat.h
# overlay 3개: 6.6 빌드 트리의 dtc($B/scripts/dtc/dtc)로 컴파일해 경고 확인(cpp 전처리 포함)
```

### B. 테스트 앱 `devcheck` (노트북)
- 변경 파일: 새 파일 `examples/devcheck/devcheck.c`, `examples/devcheck/Makefile`(`examples/mic_app/Makefile` 형식: `CROSS_COMPILE ?= aarch64-linux-gnu-`, `app`·`deploy`(`pi:bsp/apps/`)·`clean`)
- 요구사항:
  1. 사용법은 `devcheck all|imu|pca|cam|mic|sonar|pwm`. 장치마다 `<장치>: key=value …` 한두 줄과 `<장치>: PASS|FAIL <이유>`를 출력한다. `all`은 실패해도 다음 장치를 계속 진행하고, 하나라도 실패하면 종료 코드 1을 돌려준다.
  2. 장치는 sysfs의 `name`·드라이버 링크로 찾는다. `iio:deviceN` 번호, `pwmchipN` 번호, 카드 번호, 모듈 이름은 고정하지 않는다(6.6과 6.18에서 다를 수 있음. 예: 6.18 unicam 모듈은 `bcm2835_unicam_legacy`).
  3. 하위 명령별 내용:
     - `imu`: `/sys/bus/i2c/devices/1-0068/driver`의 링크 이름 출력. IIO `name`이 `mpu6050-iio-study`인 장치의 버퍼로 20샘플을 읽고 timestamp 간격 평균·최소·최대(ms) 출력. 흐름은 `examples/mpu6050_iio/mpu6050_iio_app.c`를 따르고, 끝나면 buffer enable과 scan element를 원래대로 되돌린다.
     - `pca`: `1-0040`에 붙은 pwmchip의 채널 0을 period 20ms·duty 1.5ms로 설정·enable한 뒤 다시 읽은 값과, `/dev/i2c-1` 0x40의 MODE1·PRE_SCALE·LED0 레지스터를 출력. 드라이버가 바인딩되어 있으므로 `I2C_SLAVE_FORCE`로 **읽기만** 하고 이유를 주석 한 줄로 남긴다. 끝나면 disable·unexport.
     - `cam`: media 장치 중 entity 이름에 `imx219`가 있는 것, unicam video 노드와 `bcm2835-codec` 노드의 `VIDIOC_QUERYCAP` driver·card. 이어서 `rpicam-still --immediate -o /tmp/devcheck-cam.jpg`를 fork/exec(`-n` 포함, 미리보기 없음)해 종료 코드·파일 크기·앞 2바이트 `FFD8` 여부 출력. `rpicam-still`이 없으면 캡처는 `SKIP`로 출력하고 인식 결과만으로 판정.
     - `mic`: `/proc/asound/cards`에서 `inmp441study` 카드를 찾아 alsa-lib로 1초 캡처, RMS·xrun 횟수 출력. 형식·설정은 `examples/mic_app/mic_capture.c`를 따른다.
     - `sonar`: IIO `name`이 `srf04`인 장치의 `in_distance_raw` 5회.
     - `pwm`: `pwm-bcm2835`(`*.pwm` 플랫폼 장치)에 붙은 pwmchip 채널 0·1을 period 1ms·duty 0.5ms로 설정·enable한 뒤 다시 읽은 값. 끝나면 disable·unexport.
  4. 외부 라이브러리는 alsa-lib(`-lasound`)와 `-lm`만 쓴다. 한 파일, 장치마다 함수 하나 수준으로 단순하게 쓴다.
  5. **정적 확인(보고만):** `external/linux`에서 `grep -rn '"nakta,' drivers sound`, `grep -rn '"invensense,mpu6050"' drivers`의 결과를 구현 보고에 적는다.
  6. **A push 후:** `make -C examples/mpu6050_iio modules`(6.18, `W=1`이 Makefile에 없으면 명령에 추가)와 `external/linux/scripts/checkpatch.pl --no-tree -f`로 `mpu6050_iio.c`·`mpu6050_compat.h`를 확인해 보고에 추가한다.
- 하지 말 것: 쓰기 I2C 접근, 그래프·Python, 결과 파일 저장(카메라 jpg 제외), 장치 설정을 바꾼 채 종료, A의 파일 수정, 보드 조작.
- 빌드 확인 명령:

```bash
# [WSL 노트북] ~/bspworkspace/bsp-study
make -C examples/devcheck app          # 경고 0 (-Wall -Wextra)
file examples/devcheck/devcheck        # ARM aarch64
external/linux/scripts/checkpatch.pl --no-tree -f examples/devcheck/devcheck.c
```

## 구현 보고 (Codex)

### A
- 변경 요약:
- 빌드 결과:
- checkpatch (`kernel-source/scripts/checkpatch.pl --no-tree -f <파일>`, 데스크톱에 `external/linux` 없음):
- 판단이 필요했던 부분:

### B
- 변경 요약:
- 빌드 결과:
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`):
- 판단이 필요했던 부분:

## 리뷰 (Claude)
- 확인 항목(A):
  - compat의 `IIO_DECLARE_BUFFER_WITH_TS` 대체 배열이 `__be16`, count=6에서 24바이트이고 8바이트 정렬인지. 계산: 데이터 6 × 2B = 12B → timestamp(s64) 정렬을 위해 16B로 올림 → + timestamp 8B = 24B. 6.6 `iio_push_to_buffers_with_timestamp()`는 timestamp를 `scan_bytes - 8` 위치에 쓰므로 6.6 `scan_bytes`(채널 6개 + ts = 24B)와 일치해야 한다.
  - 6.18 경로에서 드라이버 본문·전처리 결과가 포팅 전과 같은지(diff는 include 한 줄)
  - `RPI_EXTRA_CONFIG` 결과가 앞 공백 없는 5줄인지, overlay 이름 충돌 확인 결과
- 결과: 통과 | 수정 필요
- 수정 지시:
