# 18 장치 통합 (Yocto 6.6)

상태: 계획 · 브랜치: `feat/integration` · Codex 지시서: [18-integration.codex.md](18-integration.codex.md)
진행 장소: 데스크톱 WSL(Codex A·레이어·bitbake·카드 쓰기) ↔ 노트북(Codex B·64GB 카드 기준값·Yocto 카드 확인)

## 목표
12단계 Yocto 이미지(커널 6.6.63)에 IMU·PCA9685·카메라·마이크·초음파·모터 PWM을 **커널 재빌드를 포함한 통합 빌드 1회**로 넣는다. 부팅 후 C 테스트 앱 `devcheck`로 장치별 핵심 수치를 출력해 64GB 카드(6.18.59) 기준값과 비교하고, 카메라는 HDMI 미리보기와 사진 촬영까지 확인한다.

## 알아둘 개념
1. **커널 bbappend로 config·overlay를 한 번에:** `linux-raspberrypi.inc`가 `linux-yocto.inc`를 require해서 `SRC_URI`의 `.cfg` 조각이 병합된다. overlay `.dts`를 `arch/arm/boot/dts/overlays/`에 두고 `RPI_KERNEL_DEVICETREE_OVERLAYS`에 넣으면 6.6 `scripts/Makefile.lib` L444 `%.dtbo: %-overlay.dts` 규칙으로 빌드되고 `rpi-base.inc` L142 `make_dtb_boot_files()`가 부트 파티션 `overlays/`에 넣는다.
2. **compatible 매칭과 자동 적재:** DT 노드의 compatible → 드라이버 `of_match_table` 매칭, 같은 문자열이 `MODULE_DEVICE_TABLE(of)`로 modalias가 되어 udev가 모듈을 올린다. I2C는 OF 매칭이 없으면 `i2c_device_id` 이름(compatible에서 vendor를 뗀 이름)으로도 매칭하므로(6.6 `drivers/i2c/i2c-core-base.c` L146) 업스트림과 이름이 겹치지 않아야 한다.
3. **카메라 사용자 공간 경로:** `rpicam-*` → libcamera(`rpi/vc4` 파이프라인·IPA) → unicam(RAW) → `bcm2835-isp`(YUV) → `bcm2835-codec`(JPEG·H.264 하드웨어). 미리보기는 X/EGL이 없으면 DRM으로 HDMI에 직접 그린다(아래 결정 4).

## 결정과 근거
읽기 확인 위치: 데스크톱 `~/yocto`(poky `6b7c1066f7`, meta-raspberrypi `018d444`), 6.6 `kernel-source`·`linux-raspberrypi4_64-standard-build/.config`, meta-openembedded scarthgap `2db6aece32c7f8ebf6015b468e1b4e21de072e3b`(원격 읽기), rpicam-apps 레시피 SRCREV `9ae39f85`(원격 읽기).

### 1. 장치

| 장치 | 드라이버 | 6.6 config | overlay | 사용자 공간 |
| --- | --- | --- | --- | --- |
| IMU MPU6050 | 직접 작성 `mpu6050_iio`(6.6 포팅 필요) | `IIO=m`, `IIO_TRIGGERED_BUFFER=m` | 직접 작성 `mpu6050-iio` (I2C1 0x68, GPIO17) | IIO 버퍼 |
| PCA9685 | 업스트림 `pwm-pca9685` | `PWM_PCA9685=m` | 커널 트리 `i2c-pwm-pca9685a,addr=0x40` | PWM sysfs, `/dev/i2c-1` 읽기 |
| 카메라 IMX219 | 업스트림 `imx219`·unicam | `VIDEO_IMX219=m`, `VIDEO_BCM2835_UNICAM=m` | `RASPBERRYPI_CAMERA_V2=1` → `dtoverlay=imx219` | libcamera·rpicam |
| 마이크 INMP441 | 직접 작성 `inmp441_codec`(6.6 그대로 경고 0) | `SND_BCM2835_SOC_I2S=m`, `SND_SIMPLE_CARD=m` | 직접 작성 `inmp441-study` | ALSA 카드 `inmp441study` |
| 초음파 HC-SR04 | 업스트림 `srf04` | **`# CONFIG_SRF04 is not set`** → `bsp-study.cfg`에 `CONFIG_SRF04=m` | 직접 작성 `srf04` (GPIO23/24) | IIO `in_distance_raw` |
| 모터 PWM | 업스트림 `pwm-bcm2835` | `PWM_BCM2835=m` | 커널 트리 `pwm-2chan,pin=12,func=4,pin2=13,func2=4` | PWM sysfs |

- `/dev/i2c-1`: `ENABLE_I2C = "1"`은 `dtparam=i2c1=on`·`i2c_arm=on`(rpi-config L158~161)과 `kernel-module-i2c-dev` 설치(rpi-base.inc L131 `MACHINE_ESSENTIAL_EXTRA_RRECOMMENDS`)까지만 한다. 6.6은 `CONFIG_I2C_CHARDEV=m`이고 자동 적재 설정은 없어서 meta-raspberrypi `docs/extra-build-config.md` L238~240 안내대로 `KERNEL_MODULE_AUTOLOAD += "i2c-dev"`를 `bsp-study-devices.inc`에 넣는다(`i2c-bcm2708`은 6.6에서 `I2C_BCM2835=y` 내장이라 넣지 않음).
- 이미지에서 제외(소스는 유지): `hello`·`hello_module`·`char_device`·`platform_device`, 옛 `mpu6050`(같은 센서를 동시에 제어하게 됨), `pca9685_simple`, `mpu6050_iio_app`·`mic_capture`(`devcheck`가 대신함), Python 스크립트.
- 모터는 신호까지, 그래프 없이 숫자 출력만 한다. 17단계(`feat/motor-pwm`)는 아직 계획이라 PWM 기준값은 17단계 결과가 나온 뒤 비교한다.

### 2. compatible 점검

| 소스 | compatible | 6.6 `kernel-source` 같은 문자열 | 판단 |
| --- | --- | --- | --- |
| `examples/mpu6050_iio/mpu6050_iio.c` L392, overlay L30 | `nakta,mpu6050-iio-study` | 없음(`drivers/`·`sound/`·`arch/arm/boot/dts/` grep). I2C id `mpu6050-iio-study`(L398)도 업스트림 `inv_mpu_i2c.c` L176의 `"mpu6050"`과 다름 | **유지** |
| `examples/mic_codec/inmp441_codec.c` L45, overlay L19 | `nakta,inmp441` | 없음 | **유지** |
| `examples/ultrasonic/srf04-overlay.dts` L11 | `devantech,srf04` | `drivers/iio/proximity/srf04.c` L237 | 업스트림 사용이 목적 |

- 업스트림 `inv_mpu6050`은 `"invensense,mpu6050"`(`inv_mpu_i2c.c` L198)만 매칭한다. 6.6 `.config`에서 `CONFIG_INV_MPU6050_I2C=m`·`CONFIG_INV_MPU6050_IIO=m`이라 `kernel-modules`로 이미지에 **들어가지만**, compatible·I2C id가 달라 0x68에 붙지 않는다(보드에서 아래 확인).
- `nakta`는 6.6 `vendor-prefixes.yaml`에 등록되어 있지 않다. 등록 키 780개가 모두 `"^<소문자·숫자·하이픈>,.*"` 형식이고 `nakta`도 이 형식에 맞는다. **두 compatible은 저장소 안에서만 쓰는 학습용(사내용) 바인딩이고 binding 문서·upstream 등록은 하지 않는다.**
- 6.18(`external/linux`)에서 같은 문자열이 없는지는 노트북에서 확인한다(진행 2).

### 3. 사용자 공간 패키지

| 넣을 것 | 패키지 | 레시피 위치 |
| --- | --- | --- |
| libcamera(`rpi/vc4`) | `libcamera` | meta-multimedia `recipes-multimedia/libcamera/libcamera_0.4.0.bb` + meta-raspberrypi `dynamic-layers/multimedia-layer/.../libcamera_%.bbappend`(`-Dpipelines=rpi/vc4 -Dipas=rpi/vc4`, `:rpi`) |
| rpicam-apps | **`libcamera-apps`** v1.4.2 | meta-raspberrypi `dynamic-layers/multimedia-layer/recipes-multimedia/libcamera-apps/libcamera-apps_git.bb`(`PACKAGECONFIG ??= "drm"`) |
| 진단 | `v4l-utils`, `media-ctl` | meta-oe `recipes-multimedia/v4l2apps/v4l-utils_1.26.1.bb` |
| 대안용 GStreamer | `gstreamer1.0`, `gstreamer1.0-plugins-base-meta`, `gstreamer1.0-plugins-good-meta`, `libcamera-gst` | poky `meta/recipes-multimedia/gstreamer/*_1.22.12.bb`. `libcamera-gst`는 `PACKAGECONFIG ??= ""`라 우리 `libcamera_%.bbappend`에서 `gst`를 켬 |

- 넣지 않음: RTSP(`gstreamer1.0-rtsp-server`·앱), plugins-bad, x264(plugins-ugly `LICENSE_FLAGS = "commercial"`).
- 하드웨어 코덱·ISP는 6.6 `.config`에서 이미 켜짐: `VIDEO_CODEC_BCM2835=m`, `VIDEO_ISP_BCM2835=m`, `BCM_VC_SM_CMA=m`, `BCM2835_VCHIQ=y`, `DMABUF_HEAPS_CMA=y`. CMA는 `vc4-kms-v3d`가 `cma-overlay`로 256MB(6.6 `cma-overlay.dts` L19).
- 레이어 의존: meta-multimedia → `core openembedded-layer meta-python`. 우리 레이어도 `libcamera_%.bbappend` 때문에 multimedia-layer에 의존한다.

### 4. HDMI 미리보기 근거 (rpicam-apps `9ae39f85`)
- `preview/preview.cpp` L17~62: `-n`이 없으면 X/EGL(레시피에서 꺼짐) → DRM 순으로 시도하고, 둘 다 실패하면 `Preview window unavailable`만 찍고 **미리보기 없이 계속 실행**한다. 종료 코드로는 판단할 수 없다.
- `preview/drm_preview.cpp`: L247 `drmOpen("vc4")`(`CONFIG_DRM_VC4=y`, `dtoverlay=vc4-kms-v3d`), L260 DRM master 필요(데스크톱·compositor 없음), L86~135 이미 CRTC가 켜진 커넥터만 선택(`DRM_FBDEV_EMULATION=y`·`FRAMEBUFFER_CONSOLE=y`로 부팅 콘솔이 HDMI 모드를 켬) → **HDMI 모니터를 부팅 전에 연결**한다.

### 5. 이미지 크기
- 현재 rootfs 84M, `.img` 282M. poky `image.bbclass` L541: rootfs = 사용량 × 1.3 + `IMAGE_ROOTFS_EXTRA_SPACE`.
- `IMAGE_ROOTFS_EXTRA_SPACE = "524288"`(512MB) → 예상 rootfs 0.8~0.9GB, `.img` 약 0.9~1.0GB. 32GB 카드 문제없음. `/tmp`는 tmpfs가 아니라 rootfs 디렉터리라 사진은 이 여유 공간에 저장된다.
- 통합 빌드 첫 실행 추가 시간 **1.5~3시간 예상**(boost·gst base/good·libcamera·gnutls·커널).

### 6. 실패 대안 (미리 정함)

| 상황 | 대안 |
| --- | --- |
| `libcamera-apps` 빌드 실패(libcamera 0.4.0 + apps 1.4.2 조합, 확인 필요) | 이미지에서 `libcamera-apps`만 빼고 GStreamer로 확인. 이때만 plugins-bad bbappend `PACKAGECONFIG:append = " kms"`(기본 목록에 없음, `gstreamer1.0-plugins-bad_1.22.12.bb` L62)와 패키지 `gstreamer1.0-plugins-bad-kms` 추가. 기본 PACKAGECONFIG의 `rsvg`가 librsvg(Rust)를 끌어오므로 줄일지는 그때 결정 |
| ↳ 미리보기 | `gst-launch-1.0 libcamerasrc ! video/x-raw,width=1280,height=720 ! videoconvert ! kmssink driver-name=vc4` |
| ↳ 사진 | `gst-launch-1.0 libcamerasrc ! video/x-raw,width=1920,height=1080 ! videoconvert ! jpegenc snapshot=true ! filesink location=/tmp/test.jpg` (`jpegenc`은 good `jpeg` 기본 켜짐 L84. 첫 프레임 노출·`driver-name`은 보드 확인) |
| `Preview window unavailable` | HDMI 부팅 전 연결 여부, `cat /sys/class/drm/card*-HDMI-A-1/status`, `dmesg \| grep -i vc4`. 안 되면 미리보기 실패로 기록하고 사진 확인은 계속 |
| `rpicam-still` 실패 | `rpicam-hello --list-cameras`, `media-ctl -p`, `dmesg \| grep -i -E 'imx219\|unicam'`로 13단계와 비교 |
| `1-0068/driver`가 `inv-mpu6050-i2c` | compatible 충돌. 진행을 멈추고 고유 compatible(예: `bspstudy,mpu6050-iio`)로 드라이버·overlay·6.18 카드를 함께 바꾸는 계획을 다시 세움 |
| 직접 작성 모듈이 자동 적재되지 않음 | `modinfo`의 alias와 `/sys/firmware/devicetree/base/...compatible` 비교 후 `KERNEL_MODULE_AUTOLOAD` 추가 검토 |

### 7. 백업·복구
- 32GB(Yocto) 카드만 다시 쓴다. 이전 이미지는 deploy에 `bsp-study-image-raspberrypi4-64.rootfs-20261113131931.wic.bz2`로 남아 있다(2026-11-15 확인, 링크 대상). 문제가 있으면 이 파일로 12단계 5단계처럼 다시 쓴다.
- 64GB 카드는 `config.txt`를 `config.txt.bak-18`로 백업한 뒤 dtoverlay 줄만 추가하고, 확인 후 백업으로 되돌린다. 부팅 실패 시 카드 리더로 `config.txt.bak-18`을 `config.txt`로 복사한다.

## 진행 순서

### 1. [노트북] 64GB 카드 rpicam 설치 여부 (Debian 13)

```bash
# [Pi] ~ — 64GB 카드
command -v rpicam-hello rpicam-still
dpkg -l | grep -E 'rpicam|libcamera' | awk '{print $1, $2, $3}'
```

확인: **[붙여 주세요]** 둘 다 있으면 6단계에서 카메라 기준값을 같은 명령으로 얻는다. 없으면 카메라 기준값은 13단계 결과(`imx219 [3280x2464 10-bit RGGB]` 인식, 13·13-2 캡처 결과)로만 비교하고 설치는 하지 않는다.

### 2. [노트북] 6.18 compatible·config 확인

```bash
# [WSL] ~/bspworkspace/bsp-study
grep -rn '"nakta,' external/linux/drivers external/linux/sound | head
grep -rn '"invensense,mpu6050"' external/linux/drivers | head -3
grep -nE 'CONFIG_(INV_MPU6050_I2C|SRF04|PWM_PCA9685|PWM_BCM2835)=' out/kernel-rpi4/.config
```

확인: **[붙여 주세요]** `"nakta,` 출력 없음, `"invensense,mpu6050"`은 `inv_mpu_i2c.c`에만 있음.

### 3. [데스크톱] meta-openembedded 추가

빌드 셸은 VSCode 통합 터미널이 아니라 **Windows Terminal의 WSL 창 → tmux 세션 `yocto`** 안에서 연다(VSCode를 닫으면 빌드도 끝나기 때문. 12단계와 같음).

```bash
# [데스크톱 Windows Terminal, WSL 창] — 세션이 있으면 들어가고 없으면 만든다
tmux new -A -s yocto
```

```bash
# [데스크톱 WSL, tmux yocto] ~/yocto — 빌드 셸에서만 Windows PATH(/mnt/...)를 뺀다
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
echo "$PATH" | tr ':' '\n' | grep -c '^/mnt/'   # 0이면 됨
cd ~/yocto
git clone -b scarthgap https://git.openembedded.org/meta-openembedded
git -C meta-openembedded checkout 2db6aece32c7f8ebf6015b468e1b4e21de072e3b
source poky/oe-init-build-env build
bitbake-layers add-layer ../meta-openembedded/meta-oe
bitbake-layers add-layer ../meta-openembedded/meta-python
bitbake-layers add-layer ../meta-openembedded/meta-multimedia
bitbake-layers show-layers
```

확인: **[붙여 주세요]** `show-layers`에 `meta-oe`·`meta-python`·`meta-multimedia`가 보이고 오류가 없다. 이 단계는 Codex A의 레이어 변경 전에 해야 한다(우리 레이어가 multimedia-layer에 의존하게 됨).

주의:
- 첫 줄 PATH export는 주석만 붙여 넣으면 실행되지 않는다. `grep -c` 결과가 0인지 본다. `add-layer`는 이번처럼 PATH를 정리하지 않아도 동작했다. 7단계 `bitbake`는 12단계 결정대로 Windows PATH(공백이 있는 `/mnt/c/...` 경로)를 뺀 셸에서 실행해, 빌드 도구가 Windows 쪽 실행 파일을 찾지 않게 한다(poky sanity 검사가 공백을 직접 막는지는 확인하지 않음. `sanity.bbclass` L803~805는 `.`·빈 항목만 검사).
- VSCode 터미널에서 `oe-init-build-env`를 실행하면 `You had no .../poky/.vscode configuration` 메시지와 함께 `poky/.vscode`가 생긴다. 편집기 설정일 뿐이고 빌드와 저장소에 영향이 없어 그대로 둔다.
- `add-layer`는 `build/conf/bblayers.conf`에 절대 경로(`/home/nakta/yocto/meta-openembedded/...`)로 쓴다. 이 빌드 디렉터리에서는 문제없고, 새 빌드 디렉터리용 상대 경로는 Codex A가 템플릿에 넣는다.

### 4. Codex A(데스크톱)·Codex B(노트북) 구현
[Codex 지시서](18-integration.codex.md)의 A·B를 병렬로 진행한다. A와 B는 파일이 겹치지 않는다. 각자 리뷰 통과 후 같은 브랜치에 push하고 상대 PC에서 pull한다.

### 5. [노트북] 6.18 재컴파일 (A push 후)

```bash
# [WSL] ~/bspworkspace/bsp-study — git pull 후
make -C examples/mpu6050_iio modules
```

확인: **[붙여 주세요]** 경고 0. 6.18 본문이 포팅 전과 같은 동작인지는 A 리뷰에서 diff로 확인한다.

### 6. [노트북] 64GB 카드 기준값

```bash
# [WSL] ~/bspworkspace/bsp-study — 모듈·devcheck·dtbo 전송
scp examples/mpu6050_iio/mpu6050_iio.ko examples/mic_codec/inmp441_codec.ko pi:bsp/modules/
scp examples/devcheck/devcheck pi:bsp/apps/

# [Pi] ~ — 64GB 카드. dtbo가 overlay_prefix 폴더에 있는지 먼저 확인
ls /boot/firmware/overlays-bsp-study01/ | grep -E 'mpu6050-iio|inmp441-study|srf04|i2c-pwm-pca9685a|pwm-2chan'
# examples Makefile은 mpu6050-iio-overlay.dtbo를 만들고 Yocto 커널 규칙은 mpu6050-iio.dtbo를 만든다.
# 두 카드의 config.txt 줄을 같게 하려고 64GB 카드에는 Yocto 이름으로 복사한다
sudo cp ~/bsp/overlays/mpu6050-iio-overlay.dtbo /boot/firmware/overlays-bsp-study01/mpu6050-iio.dtbo
grep -n dtoverlay /boot/firmware/config.txt
```

확인: **[붙여 주세요]** dtbo `ls` 결과와 기존 `dtoverlay` 줄. 이미 있는 overlay는 아래에서 다시 넣지 않는다(이름으로 비교).

```bash
# [Pi] ~ — 64GB 카드. 없는 overlay만 덧붙인다
C=/boot/firmware/config.txt
sudo cp $C $C.bak-18
for l in dtoverlay=mpu6050-iio dtoverlay=i2c-pwm-pca9685a,addr=0x40 dtoverlay=inmp441-study dtoverlay=srf04 \
         dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4; do
  n=${l%%,*}
  grep -qE "^${n}(,|$)" $C && echo "skip $n" || echo "$l" | sudo tee -a $C
done
grep -n dtoverlay $C
sudo reboot
```

```bash
# [Pi] ~ — 재부팅 후. 6.18 카드는 직접 작성 모듈을 insmod로 올림
sudo modprobe industrialio_triggered_buffer   # insmod는 의존 모듈을 올리지 않음(9단계와 같음)
sudo insmod ~/bsp/modules/mpu6050_iio.ko
sudo insmod ~/bsp/modules/inmp441_codec.ko
readlink /sys/bus/i2c/devices/1-0068/driver
sudo ~/bsp/apps/devcheck all
rpicam-hello -t 10000 -v 1 2>&1 | grep -v '^#'   # HDMI 연결. 프레임마다 찍히는 '#N (fps)' 줄은 거름
rpicam-still -n -o /tmp/test.jpg 2>&1 | tail -2 && ls -l /tmp/test.jpg
```

확인: **[붙여 주세요]** (없던 dtbo는 해당 단계 산출물에서 복사), `driver` → `mpu6050-iio-study`, `devcheck all` 전체 출력, rpicam 로그의 미리보기 종류(`Made X/EGL` 또는 `Made DRM`).

```bash
# [Pi] ~ — 원복
sudo cp /boot/firmware/config.txt.bak-18 /boot/firmware/config.txt
sudo reboot
```

### 7. [데스크톱] 통합 빌드 (A·B pull 후)

3단계와 같은 tmux 창에서 실행한다. 새 WSL 창·새 tmux 창(`Ctrl-b c`)·분할 창(`Ctrl-b %`)은 PATH export·`source`가 안 된 상태이므로 아래 첫 블록부터 다시 한다. 빌드 전에 Windows 설정 → 전원에서 절전 모드가 "안 함"인지 확인한다(절전에 들어가면 WSL도 멈춤. 화면 꺼짐은 상관없음).

```bash
# [데스크톱 Windows Terminal, WSL 창] — 3단계 세션으로 다시 들어가기
tmux attach -t yocto            # 세션이 없으면 tmux new -A -s yocto
```

```bash
# [데스크톱 WSL, tmux yocto] — 새 창이면 다시 실행. 3단계 창 그대로면 건너뜀
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
cd ~/yocto && source poky/oe-init-build-env build
cd ~/bspworkspace/bsp-study && git pull && cd ~/yocto/build   # A·B가 push한 레이어·devcheck 받기
```

```bash
# [데스크톱 WSL, tmux yocto] ~/yocto/build
grep -n 'bsp-study-devices' conf/local.conf || echo 'require conf/bsp-study-devices.inc' >> conf/local.conf
bitbake -e bsp-study-image | grep -E '^(IMAGE_INSTALL|IMAGE_ROOTFS_EXTRA_SPACE|RPI_EXTRA_CONFIG)='
bitbake -e virtual/kernel | grep -E '^RPI_KERNEL_DEVICETREE_OVERLAYS='
bitbake -e libcamera | grep -E '^PACKAGECONFIG='
time bitbake bsp-study-image
```

확인: **[붙여 주세요]** `PACKAGECONFIG`에 `raspberrypi gst`, overlay 목록에 직접 작성 3개와 `i2c-pwm-pca9685a`·`pwm-2chan`, 빌드 시간. 실패하면 첫 ERROR 블록을 붙인다(`libcamera-apps`면 결정 6 대안).

- 빌드 중 빠져나오기: `Ctrl-b`를 누르고 뗀 뒤 `d`(빌드는 계속됨). 다시 들어가기: `tmux attach -t yocto`. Windows Terminal의 WSL 창 하나는 빌드가 끝날 때까지 열어 둔다.

```bash
# [WSL] ~/yocto/build
D=tmp/deploy/images/raspberrypi4-64
grep -nE 'dtparam|dtoverlay' $D/bootfiles/config.txt
bitbake -e virtual/kernel | grep -E '^KERNEL_MODULE_AUTOLOAD='
ls $D | grep -E 'mpu6050-iio|inmp441-study|srf04|pca9685a|pwm-2chan'
oe-pkgdata-util list-pkg-files libcamera-apps | grep -E 'rpicam-(hello|still|vid)$'
oe-pkgdata-util list-pkg-files libcamera | grep ipa_rpi_vc4
grep -n 'CONFIG_SRF04' tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git/linux-raspberrypi4_64-standard-build/.config
du -sh tmp/work/raspberrypi4_64-poky-linux/bsp-study-image/1.0/rootfs
ls -lL $D/bsp-study-image-raspberrypi4-64.rootfs.wic.bz2
```

확인: **[붙여 주세요]** `config.txt`에 `dtparam=i2c1=on`·`i2c_arm=on`·`dtoverlay=imx219`와 **dtoverlay 5줄이 각각 별도 줄**(한 줄에 붙거나 앞에 공백이 생기지 않음), `KERNEL_MODULE_AUTOLOAD`에 `i2c-dev`, dtbo 5개, rpicam 3개, `ipa_rpi_vc4`, `CONFIG_SRF04=m`(fragment 병합 확인), rootfs 크기.

### 8. [데스크톱 Windows] 카드 쓰기
12단계 5단계와 같다(`bzcat … > /mnt/d/bsp-images/bsp-study-image.img` → Imager "Use custom" → **32GB 카드**).

확인: `.img` 크기(예상 약 0.9~1.0GB).

### 9. [노트북] Yocto 카드 확인 — HDMI 모니터를 연결한 뒤 부팅

```bash
# [Pi] root@raspberrypi4-64 — Yocto 카드
uname -r
lsmod | grep -E '^(mpu6050_iio|inmp441_codec|pwm_pca9685|srf04|pwm_bcm2835|imx219|inv_mpu6050|i2c_dev)'
ls -l /dev/i2c-1
readlink /sys/bus/i2c/devices/1-0068/driver
grep CmaTotal /proc/meminfo
devcheck all
```

확인: **[붙여 주세요]** `6.6.63-v8`, 직접 작성 모듈·`i2c_dev` 자동 적재, `/dev/i2c-1` 존재, `driver` → `mpu6050-iio-study`(`inv_mpu6050` 미적재 또는 미바인딩), `devcheck all` 전체 출력.

```bash
# [Pi] Yocto 카드 — 카메라
rpicam-hello --list-cameras
rpicam-hello -t 10000                 # HDMI에 10초 미리보기
rpicam-still -o /tmp/test.jpg && ls -l /tmp/test.jpg

# [노트북 PowerShell 또는 WSL]
scp root@<IP>:/tmp/test.jpg .
```

확인: **[붙여 주세요]** `imx219` 인식, 로그 `Made DRM preview window`(+ HDMI 화면 확인), `Preview window unavailable` 없음, 사진 크기, 노트북에서 열림.

**선택 확인(TCP 스트림):** 보드 `rpicam-vid -n -t 0 --inline --listen -o tcp://0.0.0.0:8888` → 노트북 VLC `tcp/h264://<IP>:8888`. 재생 여부와 보드 `top` CPU 사용률만 적는다.

## 완료 기준
- [ ] `mpu6050_iio`가 6.6(Codex A)과 6.18(5단계) 모두 `W=1` 경고 0, 6.18 본문 동작 변경 없음
- [ ] 통합 빌드 1회로 이미지 생성, 부트 파티션에 dtbo 5개와 `config.txt` 항목, `libcamera-apps`·`ipa_rpi_vc4` 포함
- [ ] Yocto 카드에서 `1-0068/driver` → `mpu6050-iio-study`, 직접 작성 모듈 자동 적재
- [ ] `devcheck all`의 IMU·PCA9685·마이크·초음파 수치가 64GB 카드 기준값과 맞음(PWM은 17단계 이후 비교)
- [ ] HDMI 미리보기 10초(`Made DRM preview window`)와 `rpicam-still` 사진 scp 성공
- [ ] 장치를 모두 켠 상태에서 I2C 주소(0x40·0x68)·핀 충돌 없음

## 결과
- 1 64GB 카드 rpicam: `rpicam-hello`·`rpicam-still` 있음. `rpicam-apps` 1.13.0, `libcamera0.7` 0.7.2+rpt20260817(`libcamera-ipa`·`libcamera-v4l2`·`python3-libcamera` 포함) → 6단계 카메라 기준값을 같은 명령으로 얻음. Yocto 쪽은 libcamera 0.4.0 + `libcamera-apps` 1.4.2라 버전이 다르므로 로그 형식·옵션 차이는 버전 차이로 구분해 비교
- 2 6.18 compatible·config(Claude 읽기 확인 후 사용자 실행으로 같은 출력 확인): `"nakta,` 출력 없음. `"invensense,mpu6050"`은 `drivers/iio/imu/inv_mpu6050/inv_mpu_i2c.c` L202에만 있음. `CONFIG_INV_MPU6050_I2C=m`·`SRF04=m`·`PWM_BCM2835=m`·`PWM_PCA9685=m`
- 5 6.18 재컴파일(A pull 후, `6ad11bb`): `make -C examples/mpu6050_iio modules` 경고 없이 `CC [M]` → `MODPOST` → `LD [M] mpu6050_iio.ko`. 6.6 compat 변경 뒤에도 6.18 빌드 유지
- 6 64GB 카드 dtbo·config: `overlays-bsp-study01/`에 `i2c-pwm-pca9685a`·`inmp441-study`·`pwm-2chan`·`srf04` dtbo 있음, `mpu6050-iio-overlay.dtbo`를 `mpu6050-iio.dtbo`로 복사. 기존 `dtoverlay` 줄은 `vc4-kms-v3d`·`dwc2,dr_mode=host`·`nospi10`뿐 → 5줄 모두 추가 대상
- 6 64GB 카드 기준값(1차): `config.txt` 63~67행에 5줄 각각 추가. `mpu6050_iio.ko` insmod가 `Unknown symbol in module` → IIO 의존 모듈(`industrialio_triggered_buffer`)을 먼저 올리지 않음(9단계 절차 누락) → 노트 명령에 `modprobe` 추가, imu 재확인 필요
  - pca: `pwmchip1`, ch0 20000000/1500000 enable=1, MODE1 `0x20`, PRE_SCALE `0x79`, LED0 `0x00,0x00,0x33,0x01`(15단계와 같음) → PASS
  - cam: `/dev/media0` `imx219 10-0010`, unicam `/dev/video0`, bcm2835-codec 5노드, `rpicam-still` 1,679,569 B, FFD8 → PASS
  - mic: card 3 `inmp441study`, 48000/S32_LE/2ch, frames 48000, rms 90337.6, xrun 0 → PASS
  - sonar: `iio:device0`, 121·122·121·120·121(mm) → PASS
  - pwm: `pwmchip0`, ch0·ch1 1000000/500000 enable=1 → PASS
  - rpicam-hello: `Failed to create egl preview`·`Failed to create drm preview`·`Preview window unavailable`. 영상 자체는 1640x1232 약 29.98fps, 노출 33287µs·게인 8.53로 계속 들어옴. 로그가 긴 것은 프레임마다 `#N (fps)` 줄을 찍기 때문
  - imu(재확인, `modprobe industrialio_triggered_buffer` 후 insmod): driver `mpu6050-iio-study`, `iio:device1`, 20샘플 dt 평균 10.015ms(최소 10.011, 최대 10.019) → PASS. 9단계 평균 10.015ms와 같음
  - HDMI 미리보기: 64GB 카드에서는 HDMI를 연결하지 않고 진행해 미확인. 완료 기준의 미리보기는 9단계 Yocto 카드에서 확인함
- 6 64GB 카드 기준값(재작업 devcheck 580줄, 2차): 6개 모두 PASS. imu dt 평균 10.016ms(10.011~10.023), pca `pwmchip1` MODE1 `0x20`·PRE_SCALE `0x79`·LED0 `00 00 33 01`, cam JPEG 1,681,569 B·FFD8, mic rms 90241.2·xrun 0, sonar 120~121mm, pwm ch0·ch1 1ms/0.5ms enable → 1차와 같은 값. **이 줄을 9단계 비교 기준으로 씀**
- 6 원복: `config.txt.bak-18` 복사 후 `dtoverlay`는 `vc4-kms-v3d`·`dwc2,dr_mode=host`·`nospi10` 3줄만 남음, 재부팅
- 3 meta-openembedded: `2db6aece32`(python3-grpcio: update gRPC repo roots)로 checkout, `show-layers`에 `openembedded-layer`·`meta-python`·`multimedia-layer`(priority 5)가 기존 4개 레이어와 함께 오류 없이 표시. `add-layer`는 `build/conf/bblayers.conf`에 절대 경로로 기록(템플릿의 상대 경로는 Codex A가 새 빌드 디렉터리용으로 추가)
- 미검증:
