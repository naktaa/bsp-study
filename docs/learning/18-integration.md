# 장치 통합: Yocto 6.6 이미지 하나에 6개 장치 넣기

## 1. 한눈에 보기
- **목적:** 13~16단계에서 6.18.59 카드에 하나씩 수동으로 올린 장치를 12단계 Yocto 이미지(커널 6.6.63)에 한 번에 넣는다. 부팅만 하면 드라이버가 자동으로 붙고, 같은 테스트 앱으로 두 카드를 비교할 수 있게 한다.
- **기능:**
  - `meta-bsp-study` 레이어 하나에 커널 config 조각, overlay 5개, `config.txt` 줄, 직접 작성 모듈 레시피 2개, 테스트 앱 `devcheck`, libcamera·rpicam-apps를 넣었다.
  - 직접 작성 `mpu6050_iio`는 compat 헤더로 6.6과 6.18 모두에서 빌드되게 했다.
  - 18-2: 같은 이미지에 U-Boot 2024.01을 넣고(`RPI_USE_U_BOOT`, 상세는 [11 문서](11-uboot-boot.md#4-5-yocto-이미지의-u-boot18-2)), 카메라를 mainline `imx219` 대신 13-3·13-4에서 직접 작성한 `imx219_study`로 바꿨다.
- **직접 작성 / 가져다 쓴 것:** 레이어 내용(커널 bbappend·config 조각, `bsp-study-devices.inc`, 모듈 레시피 3개, `rpi-u-boot-scr` bbappend, libcamera bbappend, 이미지 레시피), [mpu6050_compat.h](../../examples/mpu6050_iio/mpu6050_compat.h), [imx219_compat.h](../../examples/camera_sensor/imx219_compat.h), [devcheck.c](../../examples/devcheck/devcheck.c)는 직접 작성했다. 업스트림 드라이버(`pwm-pca9685`, `srf04`, `pwm-bcm2835`, 복구용으로 이미지에 남긴 `imx219`), poky U-Boot 2024.01, meta-openembedded의 libcamera 0.4.0, meta-raspberrypi의 `libcamera-apps` 1.4.2는 가져다 썼다.
- **결과(실측):**
  - Yocto 카드에서 6개 장치 모두 자동 적재됐고 `devcheck all` 6개가 PASS했다.
  - IMU 샘플 간격 10.016ms와 마이크 rms는 64GB 카드 기준값과 맞았다.
  - SSH에서 실행한 `rpicam-hello`가 HDMI 모니터에 DRM 미리보기를 그렸고, 사진을 노트북으로 가져왔다.
  - 18-2: U-Boot 경유(`u-boot,version` = `2024.01`)로 부팅해도 cmdline·장치 overlay 노드가 같았고 `devcheck all` 6개가 PASS했다. `imx219_study`가 자동 적재됐고 libcamera 0.4.0이 `imx219`로 인식해 HDMI 미리보기와 사진이 바른 방향·정상 색으로 나왔다.
- **남은 것:** 모터 PWM은 sysfs 설정값까지만 확인했다(17단계 제외, 6장 미검증).
- **관련 파일:**
  - 작업 노트·지시서: [18-integration.md](../tasks/18-integration.md), [18-integration.codex.md](../tasks/18-integration.codex.md), [18-2-uboot.md](../tasks/18-2-uboot.md), [18-2-uboot.codex.md](../tasks/18-2-uboot.codex.md)
  - 장치 설정: [bsp-study-devices.inc](../../yocto/meta-bsp-study/conf/bsp-study-devices.inc)
  - 레시피: [linux-raspberrypi_%.bbappend](../../yocto/meta-bsp-study/recipes-kernel/linux/linux-raspberrypi_%25.bbappend), [mpu6050-iio.bb](../../yocto/meta-bsp-study/recipes-kernel/mpu6050-iio/mpu6050-iio.bb), [inmp441-codec.bb](../../yocto/meta-bsp-study/recipes-kernel/inmp441-codec/inmp441-codec.bb), [imx219-study.bb](../../yocto/meta-bsp-study/recipes-kernel/imx219-study/imx219-study.bb), [devcheck.bb](../../yocto/meta-bsp-study/recipes-apps/devcheck/devcheck.bb), [bsp-study-image.bb](../../yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb)
  - 코드: [mpu6050_compat.h](../../examples/mpu6050_iio/mpu6050_compat.h), [imx219_compat.h](../../examples/camera_sensor/imx219_compat.h), [devcheck.c](../../examples/devcheck/devcheck.c)

## 2. 구조
빌드부터 확인까지의 흐름:
- 빌드: `bitbake bsp-study-image` → 커널(fragment + overlay) · 외부 모듈 3개 · U-Boot·`boot.scr` · devcheck · libcamera/rpicam → `.wic.bz2` → 32GB 카드
- 부팅: firmware(`config.txt` dtoverlay 6줄 적용) → U-Boot(`boot.scr`) → 커널 → DT 노드 → compatible로 모듈 자동 적재 → probe
- 확인: `devcheck all` → sysfs·IIO·ALSA·PWM·rpicam으로 장치별 수치 출력 → 64GB 카드 값과 비교

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 레이어 설정 | `bsp-study-devices.inc` | overlay 목록, `config.txt` 줄, `ENABLE_I2C`, `i2c-dev` 자동 적재, `RPI_USE_U_BOOT` |
| 부트 | `rpi-u-boot-scr.bbappend`, `boot.cmd.in` | `saveenv` 줄을 뺀 boot 스크립트 |
| 커널 | `linux-raspberrypi_%.bbappend`, `bsp-study.cfg` | `CONFIG_SRF04=m` 추가, 직접 작성 overlay `.dts` 4개를 커널 트리에 넣어 dtbo 빌드 |
| 외부 모듈 | `mpu6050-iio.bb`, `inmp441-codec.bb`, `imx219-study.bb` | `inherit module`로 6.6 커널에 맞춰 `.ko` 빌드·설치 |
| 사용자 공간 | `devcheck`, `libcamera`·`libcamera-apps`, `v4l-utils` | 장치 확인 앱, 카메라 앱 |
| 이미지 | `bsp-study-image.bb` | 위 패키지 설치, rootfs 여유 512MB |

| 장치 | 드라이버 | overlay | 확인 경로 |
| --- | --- | --- | --- |
| IMU MPU6050 | 직접 작성 `mpu6050_iio` | 직접 작성 `mpu6050-iio` | IIO 버퍼 |
| PCA9685 | 업스트림 `pwm-pca9685` | 커널 트리 `i2c-pwm-pca9685a,addr=0x40` | PWM sysfs + `/dev/i2c-1` 읽기 |
| 카메라 IMX219 | 직접 작성 `imx219_study`(18-2, 18은 업스트림 `imx219`) + unicam | 직접 작성 `imx219-study` (18은 `RASPBERRYPI_CAMERA_V2`의 `imx219`) | `rpicam-still` |
| 마이크 INMP441 | 직접 작성 `inmp441_codec` | 직접 작성 `inmp441-study` | ALSA 1초 녹음 |
| 초음파 HC-SR04 | 업스트림 `srf04` | 직접 작성 `srf04` | IIO `in_distance_raw` |
| 모터 PWM | 업스트림 `pwm-bcm2835` | 커널 트리 `pwm-2chan` (GPIO12/13) | PWM sysfs |

## 3. 알아야 할 개념

### 왜 config·overlay를 커널 bbappend로 넣는가
- meta-raspberrypi 커널 레시피는 `SRC_URI`의 `.cfg` 조각을 커널 config에 병합한다. 그래서 6.6 기본 config에서 꺼져 있던 `CONFIG_SRF04`를 [bsp-study.cfg](../../yocto/meta-bsp-study/recipes-kernel/linux/files/bsp-study.cfg) 한 줄로 켰다.
- overlay `.dts`는 [bbappend L12](../../yocto/meta-bsp-study/recipes-kernel/linux/linux-raspberrypi_%25.bbappend#L12)에서 커널 트리 `arch/arm/boot/dts/overlays/`에 복사한다. 그러면 커널의 `%.dtbo: %-overlay.dts` 규칙으로 빌드된다.
- [bsp-study-devices.inc](../../yocto/meta-bsp-study/conf/bsp-study-devices.inc)의 `RPI_KERNEL_DEVICETREE_OVERLAYS`가 dtbo를 부트 파티션에 넣고, `RPI_EXTRA_CONFIG`가 `config.txt` 줄을 쓴다.
- 6.18 카드에서 손으로 하던 `scp dtbo` → `config.txt` 편집 → `insmod`를 빌드 설정으로 옮긴 것이다.

### 직접 작성 모듈은 어떻게 자동으로 올라오는가
- overlay가 만든 DT 노드의 compatible(`nakta,mpu6050-iio-study`, [mpu6050_iio.c#L392](../../examples/mpu6050_iio/mpu6050_iio.c#L392))이 모듈의 `MODULE_DEVICE_TABLE(of)` modalias와 같으면, udev가 부팅 중에 모듈을 올린다.
- 레시피는 `inherit module`로 `.ko`를 `/lib/modules/6.6.63-v8/`에 설치하고 `depmod`까지 한다. 의존 모듈(`industrialio_triggered_buffer`)도 함께 올라온다. 64GB 카드에서 `insmod`를 쓸 때는 의존 모듈을 직접 먼저 올려야 했다.
- `i2c-dev`는 매칭할 DT 노드가 없어서 `KERNEL_MODULE_AUTOLOAD`로 따로 올린다.
- 업스트림 `inv_mpu6050`도 이미지에 들어 있지만 compatible이 `"invensense,mpu6050"`이라 0x68에 붙지 않았다(보드에서 `lsmod`에 없음).

### 한 소스로 6.6과 6.18을 어떻게 함께 빌드하는가
- 6.18에서 쓰던 IIO API 중 일부가 6.6에는 없다. [mpu6050_compat.h](../../examples/mpu6050_iio/mpu6050_compat.h)가 커널 버전으로 갈라 6.6에서만 같은 이름을 정의한다.
  - `iio_device_claim_direct()`: 6.15부터 있음
  - `IIO_DECLARE_BUFFER_WITH_TS()`, `iio_push_to_buffers_with_ts()`: 6.16부터 있음
  - `<linux/unaligned.h>`: 6.12부터 이 경로
- 본문([mpu6050_iio.c#L192](../../examples/mpu6050_iio/mpu6050_iio.c#L192), [L206](../../examples/mpu6050_iio/mpu6050_iio.c#L206))은 6.18 API 그대로라서 6.18 동작은 바뀌지 않는다.
- 6.6의 구 API는 버퍼 크기를 받지 않으므로, 선언 매크로가 timestamp 공간을 미리 확보한다.
- 카메라(18-2)는 6.6에 없는 API가 `devm_v4l2_sensor_clk_get()`(6.18부터) 하나라, [imx219_compat.h](../../examples/camera_sensor/imx219_compat.h)가 6.6에서만 `devm_clk_get()`으로 대신한다.
- 커널 차이는 overlay에도 있었다. 6.6 unicam은 DT `brcm,media-controller`가 있을 때만 media controller 모드가 되므로 [imx219-study-overlay.dts#L67](../../examples/camera_sensor/imx219-study-overlay.dts#L67)에 한 줄을 더했다. 6.18은 compatible의 match data로 항상 켠다(`bcm2835-unicam.c` L3303).

### SSH에서 실행한 카메라가 왜 HDMI 모니터에 나오는가
- 화면 출력은 터미널과 상관없는 커널 DRM 장치(vc4)가 맡는다. SSH 세션에는 `DISPLAY`가 없으므로 `rpicam-hello`는 X 대신 DRM 미리보기를 고른다.
- 이미지에 데스크톱이 없어 화면 제어권(DRM master)을 경쟁하는 프로그램이 없다. 로그인 콘솔은 커널 fbcon이 그린 글자일 뿐이라, 그 위에 바로 그리고 끝나면 콘솔로 돌아온다.
- 부팅 콘솔이 켜 둔 HDMI 출력만 쓰기 때문에 모니터는 부팅 전에 연결한다.

### 두 카드를 같은 앱으로 비교할 수 있는 이유
- [devcheck.c](../../examples/devcheck/devcheck.c)는 장치 번호를 고정하지 않고 sysfs에서 이름으로 찾는다.
  - [find_iio L79](../../examples/devcheck/devcheck.c#L79): IIO `name`으로 찾는다.
  - [find_pwm L101](../../examples/devcheck/devcheck.c#L101): 드라이버 경로 아래 `pwmchip*`를 찾는다.
- 그래서 PCA9685가 64GB 카드에서는 `pwmchip1`, Yocto 카드에서는 `pwmchip2`여도 같은 바이너리로 동작했다.
- 64GB 카드는 크로스 빌드 바이너리로, Yocto는 레시피로 같은 소스를 빌드한다.

## 4. 구현 흐름
1. **6.6 포팅:** compat 헤더를 추가하고 6.6 빌드 트리에서 `W=1` 경고 0을 확인했다. 6.18에서도 다시 빌드해 경고 0을 확인했다.
2. **레이어:**
   - meta-openembedded(scarthgap `2db6aece32`)의 `meta-oe`·`meta-python`·`meta-multimedia`를 추가했다. `meta-bsp-study`는 `libcamera_%.bbappend`(`gst` 켬) 때문에 multimedia-layer에 의존한다([layer.conf L11](../../yocto/meta-bsp-study/conf/layer.conf#L11)).
   - 이미지에서 옛 `mpu6050`과 `pca9685_simple`은 뺐다. 같은 장치를 두 드라이버가 제어하지 않게 하려는 것이다.
3. **devcheck:**
   - `main`([L545](../../examples/devcheck/devcheck.c#L545))은 인자 `all`이면 6개를 모두, 장치 이름이면 하나만 실행한다.
   - 각 검사는 끝나면 장치 상태를 검사 전으로 되돌린다. IMU는 scan·버퍼 설정을 원복하고, PWM은 disable·unexport한다.
   - PCA9685 레지스터는 드라이버가 바인딩되어 있으므로 읽기만 한다([read_pca L228](../../examples/devcheck/devcheck.c#L228)).
   - 카메라 검사는 `rpicam-still -n`을 실행해 JPEG 헤더만 본다([L504](../../examples/devcheck/devcheck.c#L504)).
4. **기준값:** 64GB 카드(6.18)에 같은 overlay 5줄을 임시로 넣고 `devcheck all` 기준값을 얻은 뒤 `config.txt`를 원복했다.
5. **통합 빌드 → 카드 쓰기 → 확인:** 데스크톱에서 빌드하고 32GB 카드에 썼다. 노트북에서 HDMI를 연결해 부팅하고 확인했다.
   - 빌드: `time bitbake bsp-study-image` real 83분 7초, 태스크 6639개. rootfs 203M, `.wic.bz2` 100MB, `.img` 1.2GB(FAT boot 130M, ext4 root 1014M).
   - 부트 파티션 `config.txt`에 `dtparam=i2c1=on`·`i2c_arm=on`, `dtoverlay=imx219`, dtoverlay 5줄이 각각 별도 줄로 들어갔다.
6. **U-Boot·카메라 교체(18-2):** 레이어에 `RPI_USE_U_BOOT`·boot 스크립트 bbappend·`imx219-study` 레시피를 넣고, `RASPBERRYPI_CAMERA_V2`를 빼 `dtoverlay=imx219-study`를 장치 overlay 끝에 추가했다.
   - 증분 빌드는 3번 만에 성공했다(레시피 Makefile `clean` 문제, 6장). 마지막 빌드 3분 32초.
   - 전후 비교: `/proc/cmdline`·`cmdline.txt` 같음, `config.txt`는 카메라 overlay만 다름, 장치 overlay 노드 속성 같음. `imx219@10`은 compatible이 `nakta,imx219-study`로 바뀌고 vcm(`ad5398@c`)이 빠졌다.
   - mainline `imx219.ko`와 stock `imx219.dtbo`는 이미지에 남아 있어 `config.txt` 한 줄로 되돌릴 수 있다(되돌리기는 실행하지 않음).

| 항목 | Yocto 6.6.63 | 64GB 6.18.59 |
| --- | --- | --- |
| imu dt 평균 | 10.016ms | 10.016ms |
| pca PRE_SCALE / LED0 (1.5ms) | `0x79` / `00 00 34 01` | `0x79` / `00 00 33 01` |
| pca MODE1 | `0x00` | `0x20` (AI) |
| mic rms, xrun | 92051.9, 0 | 90241.2, 0 |
| sonar | 물체 위치에 따라 29~55mm | 120~121mm |
| pwm ch0·ch1 | 1ms / 0.5ms | 같음 |

- PCA9685 1.5ms는 4096 × 1.5/20 = 307.2카운트다. 6.18 드라이버는 버림(307 = `0x133`), 6.6은 308(`0x134`)로 썼다. 주기 설정은 같다.
- 6.6.63 `drivers/pwm/pwm-pca9685.c`(Yocto `tmp/work-shared/raspberrypi4-64/kernel-source`, `e442e5c1ab6b`)는 L409에서 `DIV_ROUND_UP_ULL`로 올림해 308을 쓰고, `MODE1_AI` 정의가 없어 probe(L558)에서 ALLCALL·SUB 비트만 지운다(MODE1 `0x00`). 6.18은 probe에서 `MODE1_AI`를 켠다(`external/linux/drivers/pwm/pwm-pca9685.c` L477). LED0 1카운트와 MODE1 차이는 드라이버 버전 차이다.

## 5. 재현

<details><summary>재현 절차 (펼치기)</summary>

선행 조건: 12단계 빌드 디렉터리 `~/yocto/build`(데스크톱), 레이어 추가와 meta-openembedded는 [작업 노트 3단계](../tasks/18-integration.md#3-데스크톱-meta-openembedded-추가).

```bash
# [데스크톱 WSL, tmux yocto] ~/yocto/build — Windows PATH를 뺀 셸
grep -n 'bsp-study-devices' conf/local.conf || echo 'require conf/bsp-study-devices.inc' >> conf/local.conf
bitbake bsp-study-image
ls -lL tmp/deploy/images/raspberrypi4-64/bsp-study-image-raspberrypi4-64.rootfs.wic.bz2
```

카드 쓰기는 12단계와 같다(`bzcat` → Raspberry Pi Imager "Use custom" → 32GB 카드). HDMI 모니터를 연결한 뒤 부팅한다.

```bash
# [Pi] root@raspberrypi4-64 — Yocto 카드 (노트북에서 ssh yocto)
uname -r                                          # 6.6.63-v8
readlink /sys/bus/i2c/devices/1-0068/driver       # .../mpu6050-iio-study
tr -d '\0' < /proc/device-tree/chosen/u-boot,version; echo   # 2024.01 (18-2)
readlink /sys/bus/i2c/devices/10-0010/driver      # .../imx219-study (18-2)
rpicam-hello --list-cameras                       # imx219 [3280x2464 10-bit RGGB], 모드 1개
devcheck all                                      # 6개 PASS
devcheck sonar                                    # 장치 하나만: imu|pca|cam|mic|sonar|pwm
rpicam-hello -t 10000                             # 로그 'Made DRM preview window', HDMI에 10초 영상
rpicam-still -o /tmp/test.jpg && ls -l /tmp/test.jpg
```

```bash
# [노트북 WSL] ~/bspworkspace/bsp-study — Pi 셸이 아니라 노트북 터미널에서
mkdir -p out/cam18
scp yocto:/tmp/test.jpg out/cam18/
explorer.exe "$(wslpath -w out/cam18)"
```

64GB 카드 기준값 측정과 원복 절차는 [작업 노트 6단계](../tasks/18-integration.md#6-노트북-64gb-카드-기준값)에 있다.

</details>

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| 64GB 카드 `insmod mpu6050_iio.ko`가 `Unknown symbol in module` | `insmod`는 의존 모듈을 올리지 않음 | `sudo modprobe industrialio_triggered_buffer` 후 `insmod`. Yocto는 `depmod`된 자동 적재라 필요 없음 |
| `rpicam-hello` 로그가 매우 김 | 프레임마다 `#N (fps)` 줄을 찍음 | `rpicam-hello -t 10000 -v 1 2>&1 \| grep -v '^#'` |
| 64GB 카드 `Preview window unavailable` | HDMI를 연결하지 않고 진행함 | 미리보기는 Yocto 카드에서 HDMI를 연결해 확인. 64GB 카드 미리보기는 확인하지 않음 |
| `scp yocto:/tmp/test.jpg`가 `Error resolving 'yocto'` | Pi 셸에서 실행. `yocto`는 노트북 `~/.ssh/config` 별칭 | 노트북 WSL 터미널에서 실행 |
| `rpicam` 경고 `Mismatch between Unicam and CamHelper for embedded data usage!` | imx219가 embedded data 패드를 내지 않음(13-2와 같음) | 영상·사진 정상이라 그대로 둠 |
| `.img`가 예상(0.9~1.0GB)보다 큰 1.2GB | wic(`scripts/lib/wic/partition.py` L280 `get_rootfs_size()`)가 bitbake `ROOTFS_SIZE`(798720KB, 이미 ×1.3 + 여유 512MB)를 받은 뒤 기본 `--overhead-factor` 1.3(`ksparser.py` L136)을 한 번 더 곱함 → ext4 1014MiB | 32GB 카드라 그대로 둠 |
| 빌드 중 WARNING: v4l-utils `git.linuxtv.org` 받기 실패 | 원본 저장소 접속 실패 | 미러에서 받아 빌드 계속됨. 조치 없음 |
| 18-2 증분 빌드에서 `mpu6050-iio`·`inmp441-codec` `do_configure`가 `No rule to make target 'clean'` | 해시가 바뀐 레시피는 `base_do_configure`가 `oe_runmake clean`을 먼저 부름(poky `base.bbclass` L335~341). 18단계 첫 빌드는 이전 configure 기록이 없어 드러나지 않음 | 레시피용 Makefile에 `clean` 타깃 추가 |
| 위 수정 뒤 `do_package`가 pseudo `inode mismatch`·`path mismatch`로 abort(exit 134) | `clean`이 Kbuild clean(`M=${WORKDIR}`)을 불러 하위 `image/`·`package/`의 `.ko.xz`까지 pseudo 밖에서 지움 | `clean`을 현재 폴더만 지우는 `rm -f`로 바꿈([Makefile#L11](../../yocto/meta-bsp-study/recipes-kernel/imx219-study/files/Makefile#L11)), 세 레시피 `bitbake -c clean` 후 다시 빌드 |
| Yocto 카드에서 `mount /dev/mmcblk0p1 /mnt/boot`가 `Device or resource busy` | Yocto rootfs가 부트 파티션을 이미 `/boot`에 마운트함 | `/proc/mounts`에서 경로를 찾아 씀 |

- 미검증:
  - 초음파를 자로 잰 거리와 비교하지 않았다. 거리 변화를 따라가는 것만 확인했다.
  - TCP 스트림 선택 확인은 실행하지 않았다.
  - 18-2: 시리얼 콘솔의 U-Boot 로그, 카메라만 되돌리기·`kernel8.img` 교체 원복, Yocto 카드에서 직접 작성 카메라 드라이버의 연속 프레임 간격.
  - 모터 PWM은 두 카드의 sysfs 값(ch0·ch1 1ms/0.5ms enable)이 같은 것만 확인했다. debugfs·핀 기능(ALT0)·방향 GPIO·disable 후 출력·온보드 오디오 충돌은 확인하지 않았다(17단계 제외).
