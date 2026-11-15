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
  - `mpu6050_iio.c`는 include 한 줄만 변경했다. `mpu6050_compat.h`에 unaligned 헤더·direct claim/release·timestamp 배열·push API의 버전 분기를 추가했다. 원본과 비교해 include 외 본문이 같은 것을 확인했다.
  - 레이어 의존성과 템플릿에 meta-openembedded 3개 레이어를 추가하고, `bsp-study-devices.inc`에 overlay 5개·I2C·Camera V2·`i2c-dev` 자동 적재를 모았다.
  - 커널 bbappend·SRF04 fragment, 모듈 레시피 2개와 전용 Makefile, `devcheck.bb`, libcamera gst bbappend를 추가했다. 이미지에 지정 패키지 11개와 추가 공간 524288KiB를 넣었다.
  - `examples/`의 기존 Makefile·overlay·마이크 C 소스, `configs/bsp-study.config`, `build/conf`, B의 파일은 수정하지 않았다. 작업 시작 전부터 있던 `18-integration.md` 변경도 유지했다.
- 빌드 결과 (2026-11-15, 데스크톱 6.6.63):
  - 아래 명령으로 임시 복사본의 IMU를 빌드했고 종료 코드 0·경고 0이었다. 이후 레시피 전용 Makefile을 통한 IMU·마이크 빌드도 모두 종료 코드 0·경고 0, 산출물은 `file`로 ARM aarch64 ELF임을 확인했다. `T` 아래 `imu/`·`codec/`에 해당 소스와 레시피 Makefile을 복사했고, `imu/Kbuild`는 `obj-m := mpu6050_iio.o`이다.

    ```bash
    W=$HOME/yocto/build/tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git
    K=$HOME/yocto/build/tmp/work-shared/raspberrypi4-64/kernel-source
    B=$W/linux-raspberrypi4_64-standard-build
    T=/tmp/bsp-integration-a-7upk38kl
    export PATH=$W/recipe-sysroot-native/usr/bin/aarch64-poky-linux:$W/recipe-sysroot-native/usr/bin:$PATH
    make -C "$K" O="$B" M="$T/imu" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
    make -C "$T/imu" KERNEL_SRC="$K" O="$B" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
    make -C "$T/codec" KERNEL_SRC="$K" O="$B" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
    ```

  - 임시 검증 모듈에서 compat 헤더를 포함해 `sizeof(struct scan_layout) == 24`와 `__alignof__(struct scan_layout) == 8`을 `static_assert`로 확인했다. 최종 `make -C "$K" O="$B" M="$T/layout" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules`는 종료 코드 0·경고 0이었다. 검증 모듈은 저장소에 추가하지 않았다.
  - overlay 3개는 아래 형식으로 각각 CPP 전처리 후 6.6 빌드 트리의 dtc로 컴파일했다. 모두 종료 코드 0·경고 0이었다. `mpu6050-iio.dtbo` 1052B, `inmp441-study.dtbo` 1475B, `srf04.dtbo` 555B이며 산출물은 `$T/dtbo/`에만 두었다.

    ```bash
    # source는 기존 examples/의 각 overlay, stem은 결과 dtbo의 이름이다.
    aarch64-poky-linux-gcc -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
        -I "$K/scripts/dtc/include-prefixes" "$source" > "$T/dtbo/$stem.dts"
    "$B/scripts/dtc/dtc" -@ -H epapr -I dts -O dtb \
        -o "$T/dtbo/$stem.dtbo" "$T/dtbo/$stem.dts"
    ```

  - `FILESEXTRAPATHS`의 실제 경로를 해석해 모든 기존 소스가 연결되는지, 두 모듈의 `file://Makefile`이 `examples/`가 아닌 레시피의 `files/Makefile`을 먼저 선택하는지 확인했다. fragment의 설정도 기존 fragment와 일치한다.
  - `RPI_EXTRA_CONFIG` 문자열을 rpi-config와 같은 `printf` 형식으로 출력해 선행 빈 줄 1개 다음에 앞 공백 없는 지정 `dtoverlay` 5줄이 나오는 것을 확인했다. 실제 BitBake 변수 확장·배포된 config.txt 확인은 미실행이다.
  - `devcheck.c`는 현재 데스크톱에 없어 앱 빌드는 미실행이다. B 소스 수신이 필요하다. 6.18 트리도 없어 해당 빌드·전처리 비교는 B의 후속 확인으로 남긴다. BitBake·전체 커널 빌드·설치·보드 실행은 하지 않았다.
- checkpatch (`kernel-source/scripts/checkpatch.pl --no-tree -f <파일>`, 데스크톱에 `external/linux` 없음):
  - `mpu6050_iio.c`: 종료 코드 1, **0 errors / 1 warning**. 기존 SPDX 누락이다. include 한 줄만 바꾸라는 범위를 지키기 위해 유지했다.
  - `mpu6050_compat.h`: 종료 코드 1, **1 error / 6 warnings**. `LINUX_VERSION_CODE` 사용 경고 3건은 지시서의 명시적 요구에 따른다. 비교 방향 경고 3건은 `LINUX_VERSION_CODE < KERNEL_VERSION(...)`의 양쪽이 상수인 전처리 조건을 검사기가 일반 비교로 판정한 것이다.
  - error는 `IIO_DECLARE_BUFFER_WITH_TS`에 대한 `COMPLEX_MACRO`이다. 이 매크로는 표현식이 아니라 초기화 가능한 배열 선언이므로 전체를 괄호로 감쌀 수 없다. 임시 파일의 단순 `type name[count] __aligned(8)` 선언 매크로에서도 같은 오류가 재현되어 오탐으로 판단했다. 억제 옵션이나 형변환은 추가하지 않았다. 빌드 경고와 checkpatch 진단은 별개이다.
- 판단이 필요했던 부분:
  - **버전 경계:** 로컬 6.6.63 헤더와 upstream의 인접 릴리스 헤더를 비교했다. unaligned는 [v6.11 asm-generic 헤더](https://github.com/torvalds/linux/blob/v6.11/include/asm-generic/unaligned.h)와 [v6.12 linux 헤더](https://github.com/torvalds/linux/blob/v6.12/include/linux/unaligned.h), 이동 커밋 [5f60d5f6bbc1](https://github.com/torvalds/linux/commit/5f60d5f6bbc12e782fac78110b0ee62698f3b576)을 근거로 6.12에서 분기했다.
  - direct claim/release는 [v6.14 iio.h](https://github.com/torvalds/linux/blob/v6.14/include/linux/iio/iio.h)에는 구 API만 있고 [v6.15 iio.h](https://github.com/torvalds/linux/blob/v6.15/include/linux/iio/iio.h)에 bool 래퍼가 있으며, 도입 커밋은 [d795e38df4b7](https://github.com/torvalds/linux/commit/d795e38df4b7ebac1072bbf7d8a5500c1ea83332)이다. 따라서 6.15에서 분기했다.
  - 배열 매크로와 길이 인자 push API는 v6.15 헤더에는 없고 v6.16의 [iio.h](https://github.com/torvalds/linux/blob/v6.16/include/linux/iio/iio.h)·[buffer.h](https://github.com/torvalds/linux/blob/v6.16/include/linux/iio/buffer.h)에 있다. 도입 커밋은 각각 [63fc53526d30](https://github.com/torvalds/linux/commit/63fc53526d3090b27bd06bb43b92e8bc85f46fb1)·[8f08055bc67a](https://github.com/torvalds/linux/commit/8f08055bc67a355aca55856cc810b89645506a5f)이며 6.16에서 함께 분기했다. [upstream v6.18 iio.h](https://github.com/torvalds/linux/blob/v6.18/include/linux/iio/iio.h)에서도 신 API를 읽기 확인했지만 로컬 6.18 빌드를 대신하지는 않는다.
  - **timestamp 배치:** `__be16` 6개의 12B를 16B로 올리고 timestamp 8B를 더해 배열은 24B, 정렬은 8B다. 6.6 `buffer.h`의 push 함수는 timestamp를 `scan_bytes / 8 - 1`번째 s64에 쓰므로 이 장치의 24B scan에서는 offset 16B와 맞는다. 구 push API에 없는 길이 검사는 추가하지 않았다.
  - **overlay 충돌·태스크:** 구현 전 6.6 `arch/arm/boot/dts/overlays/`에서 지정 DTS·DTBO 3개가 없고 Makefile에도 해당 이름이 없는 것을 확인했다. 기존 `scripts/Makefile.lib:444`의 `%.dtbo: %-overlay.dts` 규칙을 이용한다. `kernel.bbclass`의 `do_symlink_kernsrc`와 `kernel-yocto.bbclass`의 `do_kernel_checkout` 이후인 `do_patch:append()`에서 `${S}`에 넣어 unpack 이후 소스 이동·checkout에 영향을 받지 않게 했다. 이 태스크 자체는 실행하지 않았다.
  - **fragment·fetch:** `bsp-study.cfg`는 일반 파일로 작성해 `CONFIG_SRF04=m` 설정만 복제했다. 로컬 fetcher가 `FILESPATH`에서 파일을 찾고 unpack 시 복사하는 경로를 읽기 확인했다(`bitbake/lib/bb/fetch2/local.py`, `fetch2/__init__.py:1618`). 저장소 밖을 가리키는 링크 없이 지정 내용을 가져올 수 있고, 기존 fragment의 WSL용 주석도 포함하지 않는다. 실제 fetch 태스크는 미실행이다.
  - **부트 설정:** 각 설정 사이의 리터럴 `\n`을 `rpi-config_git.bb:304`의 `printf`가 줄바꿈으로 해석한다. 문자열 시작의 `\n`은 앞선 설정과 분리하고, 이후에는 들여쓰기나 줄 이음을 넣지 않아 각 `dtoverlay`가 공백 없이 시작한다.
  - **패키지명:** 로컬 `libcamera_0.4.0.bb`의 `${PN}-gst`, `libcamera-apps_git.bb`의 기본 `${PN}`, `v4l-utils_1.26.1.bb`의 `media-ctl`, poky `gstreamer1.0-plugins-packaging.inc`의 `${PN}-meta`를 확인했다. 모듈은 `module.bbclass`가 `${PN}`을 빈 메타 패키지로 두고 분할 모듈을 의존성에 넣으므로 이미지에서 레시피 이름을 사용했다. 지시서와 다른 이름은 없었다. 패키지 생성·rootfs 설치 성공은 미검증이다.
  - 레시피의 GPL-2.0-only 공통 라이선스 체크섬은 로컬 poky 파일의 MD5로 확인했다. `devcheck`의 실제 소스 라이선스는 B 소스 수신 후 함께 확인해야 한다.

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
- 리뷰 A (2026-11-15, 데스크톱, `git diff`와 새 파일 대조):
  - 범위: 변경 파일이 지시서 A 목록과 일치. `examples/*/Makefile`·overlay `.dts`·`configs/bsp-study.config`·`build/conf`·B 파일은 그대로. 커널 config는 `recipes-kernel/linux/files/bsp-study.cfg`(`CONFIG_SRF04=m` 한 줄)로 Yocto 커널에만 들어가고 내용은 `configs/bsp-study.config`와 같다.
  - 포팅: `mpu6050_iio.c` diff는 L16 include 한 줄. `mpu6050_compat.h`는 6.6에서만 정의가 생기고 6.18에서는 `linux/unaligned.h` include 외에 아무것도 정의하지 않는다(L10~40). 6.6 `include/linux/iio/`에 같은 이름(`iio_device_claim_direct`·`IIO_DECLARE_BUFFER_WITH_TS`·`iio_push_to_buffers_with_ts`)이 없어 재정의 충돌 없음(읽기 확인). bool 변환 `== 0`(L19)으로 6.18 호출부 `if (!iio_device_claim_direct())`의 의미가 같다. 경계 버전(6.12·6.15·6.16)은 upstream 헤더·커밋 링크로 근거를 남겼다.
  - 버퍼 크기: L29~31은 `ALIGN(6, 8/2) + 8/2 = 8 + 4 = 12`개 × 2B = **24B**, `__aligned(8)`(Codex가 upstream v6.18 `iio.h`를 읽고 같은 식으로 작성. 로컬 6.18 `external/linux`와의 대조는 노트북 B에서 확인 필요). Codex가 `static_assert`로 24B·8B 정렬을 확인했다. 6.6 push는 `((s64 *)data)[scan_bytes / 8 - 1]` = offset 16B에 timestamp를 쓰므로 6.18의 `IIO_DECLARE_BUFFER_WITH_TS` 배치와 같다. 6.6 wrapper가 `data_total_len`을 쓰지 않는 것은 구 API에 길이 인자가 없기 때문이며 검사를 새로 만들지 않은 것이 지시에 맞다.
  - 레이어: `KERNEL_DEVICETREE ??= ${RPI_KERNEL_DEVICETREE} ${RPI_KERNEL_DEVICETREE_OVERLAYS}`(rpi-base.inc L107)라 `.inc`의 `:append`가 빌드·부트 파티션 배포 목록에 함께 들어간다. `RPI_EXTRA_CONFIG:append`는 리터럴 `\n` 구분이고 rpi-config L304 `printf`가 줄바꿈으로 바꾼다. 앞 공백 없음. 모듈 레시피 Makefile은 `oe_runmake`의 `O=`가 하위 make에 전달되는 형식이고, `FILESEXTRAPATHS`에서 `files/`가 먼저라 레시피 Makefile이 선택된다. 이미지 rootfs에 `udev`가 있어(`/sbin/udevd`, 현재 rootfs 확인) modalias 자동 적재 경로가 있다.
  - 오류·문맥: 드라이버 본문 변경이 없어 probe·remove·IRQ 경로는 6.18과 같다.
  - 관찰(수정 불요): kernel-yocto `do_patch()`는 `set +e`로 시작한다(kernel-yocto.bbclass L343~). 그래서 `do_patch:append`의 `install`이 실패해도 태스크가 바로 멈추지 않을 수 있다. 다만 그 경우 `do_compile`에서 dtbo 규칙이 없다는 오류로 드러나므로 7단계 로그로 판단한다.
  - checkpatch: `mpu6050_iio.c` 기존 SPDX 경고 1건(범위상 유지), `mpu6050_compat.h`의 `LINUX_VERSION_CODE` 경고 3건(지시대로 버전 분기), 비교 방향 경고 3건(전처리 상수 비교), `COMPLEX_MACRO` error 1건(배열 선언 매크로라 괄호로 감쌀 수 없음, upstream 같은 형태). 이유가 타당해 유지한다.
  - 미검증: bitbake 파싱·fetch·`do_patch`·패키지 생성, 6.18 재컴파일(노트 5단계), `devcheck.bb` 빌드(B 소스 필요).
- B에 전달: `devcheck.bb`가 `LICENSE = "GPL-2.0-only"`이므로 `examples/devcheck/devcheck.c` 첫 줄을 `// SPDX-License-Identifier: GPL-2.0-only`로 한다.
- 결과: A 통과 (B는 구현 후 리뷰)
- 수정 지시: 없음
