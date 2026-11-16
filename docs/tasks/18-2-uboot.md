# 18-2 통합 Yocto 이미지에 U-Boot와 직접 작성 카메라 드라이버 넣기

상태: 계획 · 브랜치: `feat/uboot-yocto` · Codex 지시서: [18-2-uboot.codex.md](18-2-uboot.codex.md)
진행 장소: 데스크톱 WSL(빌드·카드 쓰기) ↔ 노트북(32GB Yocto 카드 확인)

## 목표
18단계 통합 이미지의 부팅을 `firmware → U-Boot 2024.01 → boot.scr → Linux 6.6.63 → rootfs`로 바꾼다. meta-raspberrypi의 `RPI_USE_U_BOOT`가 무엇을 바꾸는지, U-Boot를 넣어도 DT·bootargs·장치 동작은 그대로인지 전후 비교로 설명할 수 있게 한다. 12단계(T-B02)에서 미룬 "U-Boot 포함"을 채우는 작업이다.

같은 빌드에서 카메라를 mainline `imx219` 대신 13-3·13-4에서 직접 작성한 `imx219_study`로 바꾼다. 6.6.63에서 빌드되게 하고, Yocto 카드에서 자동 적재, libcamera 0.4.0 인식, HDMI 미리보기까지 확인한다. 빌드와 카드 쓰기는 한 번이고, 확인은 U-Boot와 카메라로 나눠 원인을 가릴 수 있게 한다.

## 알아둘 개념
1. **`RPI_USE_U_BOOT = "1"`이 바꾸는 것:** 부트 파티션 파일이 `Image;kernel8.img`에서 `Image`, `u-boot.bin;kernel8.img`, `boot.scr`로 바뀐다(meta-raspberrypi `conf/machine/include/rpi-base.inc` L166~170). firmware는 기본 이름 `kernel8.img`를 읽으므로 `config.txt`에 `kernel=`을 넣지 않아도 U-Boot가 실행된다. Pi 4 64비트는 U-Boot 유무와 관계없이 `KERNEL_IMAGETYPE`이 `Image`라 커널은 다시 빌드하지 않는다(`raspberrypi4-64.conf` L34~37).
2. **DT는 firmware가 만들고 U-Boot는 넘긴다:** firmware가 `config.txt`의 dtparam·dtoverlay(장치 overlay 5줄 + 카메라 overlay)를 적용한 DT를 메모리에 두고 주소를 U-Boot에 준다. boot 스크립트는 `fdt addr ${fdt_addr}`로 이 DT를 쓰고 `/chosen/bootargs`(= `cmdline.txt`)를 복사한 뒤 `booti ${kernel_addr_r} - ${fdt_addr}`로 커널을 시작한다. U-Boot가 `booti` 직전에 고치는 노드(`/chosen` 등)는 6단계 DT diff로 확인한다.
   - boot.scr과 saveenv: meta-raspberrypi 기본 `boot.cmd.in`(4줄)은 첫 부팅에 `uboot.env`가 없으면 `saveenv`로 환경을 카드에 저장한다. 11단계처럼 카드에 상태를 남기지 않으려고 이 한 줄만 뺀 파일을 우리 레이어 bbappend로 쓴다.
3. **Yocto에서 카메라 드라이버 바꾸기:** `RASPBERRYPI_CAMERA_V2`가 넣던 `dtoverlay=imx219`를 빼고 `dtoverlay=imx219-study`를 넣는다. 그러면 DT에 `sony,imx219` 노드가 없어 mainline `imx219.ko`는 이미지에 있어도 붙지 않는다. `nakta,imx219-study` modalias로 udev가 `imx219_study`를 올리고, `depmod`가 끝난 이미지라 `v4l2_cci` 등 의존 모듈도 함께 올라온다. libcamera는 엔티티 이름 `imx219 10-0010`으로 모델을 정하므로 튜닝 파일 `imx219.json`을 그대로 쓴다(13-4).

## 결정과 근거
| 결정 | 내용 | 근거 |
| --- | --- | --- |
| U-Boot 넣는 방법 | `bsp-study-devices.inc`에 `RPI_USE_U_BOOT = "1"` | 이미 `local.conf`가 require하므로 빌드 디렉터리를 고치지 않음 |
| boot 스크립트 | 우리 레이어 `rpi-u-boot-scr.bbappend` + `files/boot.cmd.in`(기본에서 saveenv 줄만 제거) | 11단계 결정(`saveenv` 안 함) 유지, 나머지는 레시피 그대로 |
| U-Boot 버전 | poky `u-boot_2024.01.bb`(SRCREV `866ca972`), `UBOOT_MACHINE = "rpi_arm64_config"` | 11단계 v2026.10과 다름. 차이는 4단계에서 확인 |
| 단계 번호 | 18-2(roadmap 표는 그대로, 18 본문에 링크) | 같은 이미지를 보완하는 작업 |
| 학습 문서 | U-Boot는 [11 문서](../learning/11-uboot-boot.md), 카메라 교체는 [18 문서](../learning/18-integration.md)와 [13 문서](../learning/13-camera.md)에 추가 | U-Boot·통합·카메라 문서를 각각 하나로 유지 |
| 카메라 드라이버 | `imx219_study`를 모듈 레시피로 넣고 overlay를 커널 트리에서 빌드. mainline `imx219.ko`는 복구용으로 이미지에 남김 | 18단계 `mpu6050-iio`와 같은 방식 |
| 6.6 API 차이 | 6.6에 없는 API만 compat 헤더로 처리하고 6.18 동작은 바꾸지 않음 | 18단계 `mpu6050_compat.h`와 같은 방식 |
| 빌드 횟수 | U-Boot와 카메라를 한 번에 빌드·카드 쓰기 | overlay 추가로 커널을 어차피 다시 빌드함 |
| 모드 | 3280×2464 한 모드 그대로(binning 없음) | 13-4 결정. Yocto 결과를 보고 판단 |

VEDA 강의와 비교: Yocto 강의(`임베디드리눅스2.pdf`, `주요 Yocto 환경변수과모듈레시피.pdf`)는 U-Boot를 이미지에 넣는 방법을 다루지 않는다. U-Boot 강의(`u-boot에서라즈베리파이 커널 부팅.pdf` p1)는 프롬프트에서 `setenv bootargs`·`fatload ... bcm2711-rpi-4-b.dtb`·`saveenv`로 설정한다. 이번에는 11단계처럼 DT를 다시 읽지 않고(firmware `fdt_addr`), `saveenv` 대신 `boot.scr`를 쓴다. 강의에서 가져오는 것은 bbappend로 레시피 파일을 바꾸는 방법(p22)과 `bitbake -e`로 변수를 확인하는 방법(p24)이다.

## 백업·복구
- **빌드 전:** 현재 18 이미지 `bsp-study-image-raspberrypi4-64.rootfs-20261115034338.wic.bz2`를 `~/yocto/backup/`에 복사한다(2단계).
- **빠른 원복(다시 쓰지 않음):** 카드 리더로 Windows에서 부트 파티션을 연다. `kernel8.img`(U-Boot)를 `u-boot.bin`으로 복사해 둔 뒤 `Image`를 `kernel8.img`로 덮어쓴다. 그러면 firmware가 커널을 직접 부팅하고 `boot.scr`는 쓰이지 않는다. U-Boot로 돌아가려면 `u-boot.bin`을 `kernel8.img`로 다시 덮어쓴다.
- **카메라만 되돌리기:** 카드 리더로 부트 파티션 `config.txt`의 `dtoverlay=imx219-study`를 `dtoverlay=imx219`로 바꾼다. mainline `imx219.ko`와 `imx219.dtbo`는 이미지에 남아 있다.
- **완전 원복:** 백업한 `.wic.bz2`를 18단계 8단계 명령으로 `.img`로 풀어 Imager로 32GB 카드에 다시 쓴다.

## 진행 순서

### 1. [노트북, 지금의 18 Yocto 카드] U-Boot 전 기준값
HDMI 모니터를 연결해 부팅한다. `yocto`는 노트북 `~/.ssh/config`의 Yocto 카드 별칭이다.

```bash
# [Pi] root@raspberrypi4-64 — 기준값을 /tmp/before에 모음
mkdir -p /tmp/before
cat /proc/cmdline > /tmp/before/cmdline.txt
devcheck all > /tmp/before/devcheck.txt 2>&1; echo "devcheck=$?"
ls /proc/device-tree/chosen/ > /tmp/before/chosen-list.txt
tar -C /sys/firmware/devicetree/base -cf /tmp/before/dt.tar .
B=$(awk '$1=="/dev/mmcblk0p1" {print $2}' /proc/mounts); echo "boot=$B"   # Yocto는 부트 파티션을 이미 마운트함(18-2 1단계 실측)
ls -l $B > /tmp/before/bootpart-ls.txt
cp $B/config.txt /tmp/before/boot-config.txt
cp $B/cmdline.txt /tmp/before/boot-cmdline.txt
ls -l /tmp/before
```

```bash
# [노트북 WSL] ~/bspworkspace/bsp-study — 가져와서 dts로 변환
mkdir -p out/uboot18/before/dt
scp 'yocto:/tmp/before/*' out/uboot18/before/
tar -xf out/uboot18/before/dt.tar -C out/uboot18/before/dt
DTC=$(command -v dtc || echo out/kernel-rpi4/scripts/dtc/dtc)
$DTC -I fs -O dts -o out/uboot18/before/dt.dts out/uboot18/before/dt 2>/dev/null
grep -c . out/uboot18/before/dt.dts; cat out/uboot18/before/cmdline.txt
```

확인: **[붙여 주세요]** `devcheck=0`과 6개 PASS 줄, `cmdline.txt`, `bootpart-ls.txt`(현재 `kernel8.img` 크기), `dt.dts` 줄 수. `chosen-list.txt`에 `u-boot,version`이 없어야 한다.

### 2. [데스크톱 WSL] 18 이미지 백업

```bash
# [WSL] ~/yocto/build
mkdir -p ~/yocto/backup
cp -L tmp/deploy/images/raspberrypi4-64/bsp-study-image-raspberrypi4-64.rootfs-20261115034338.wic.bz2 ~/yocto/backup/
ls -l ~/yocto/backup/
```

### 3. Codex 구현과 리뷰
[Codex 지시서](18-2-uboot.codex.md)의 A(U-Boot, 파일 3개)와 B(카메라)를 데스크톱에서 구현한다. B는 6.6 빌드 트리로 확인해야 하므로 데스크톱에서 한다. 각각 리뷰 통과 후 push한다.

```bash
# [노트북 WSL] ~/bspworkspace/bsp-study — B push 후 6.18 재빌드(동작 변경 없음 확인)
git pull && make -C examples/camera_sensor W=1
```

확인: **[붙여 주세요]** 경고 없이 `LD [M] imx219_study.ko`.

### 4. [데스크톱 WSL, tmux yocto] 증분 빌드와 U-Boot 소스 확인
18단계 7단계처럼 PATH를 정리하고 `source`한 창에서 실행한다.

```bash
# [WSL] ~/yocto/build — 빌드 전 변수 확인
cd ~/bspworkspace/bsp-study && git pull && cd ~/yocto/build
bitbake -e bsp-study-image | grep -E '^(RPI_USE_U_BOOT|KERNEL_IMAGETYPE|IMAGE_BOOT_FILES|RASPBERRYPI_CAMERA_V2)='
bitbake -e virtual/kernel | grep -E '^RPI_KERNEL_DEVICETREE_OVERLAYS='
time bitbake bsp-study-image
```

```bash
# [WSL] ~/yocto/build — 빌드 산출물과 U-Boot 2024.01 소스·설정 확인
D=tmp/deploy/images/raspberrypi4-64
ls -l $D/u-boot.bin $D/boot.scr
strings $D/boot.scr | grep -E 'fdt|load|boot|saveenv'           # saveenv 없음
strings $D/u-boot.bin | grep -m1 'U-Boot 2024.01'
UB=$(ls -d tmp/work/raspberrypi4_64-poky-linux/u-boot/*/ | head -1); echo "$UB"
grep -n 'u-boot,version' $UB/git/boot/fdt_support.c              # 판정 근거 1
grep -E '^CONFIG_(BOOTSTD|BOOTSTD_DEFAULTS|DISTRO_DEFAULTS|BOOTCOMMAND|ENV_IS_IN_FAT|ENV_FAT_FILE|BOOTDELAY|VIDEO_BCM2835|SYS_BOOTM_LEN)=' $UB/build/.config
ls $UB/git/board/raspberrypi/rpi/
grep -rn 'kernel_addr_r\|fdt_addr_r\|ramdisk_addr_r\|scriptaddr' $UB/git/board/raspberrypi/rpi/ $UB/git/include/configs/rpi.h | head
```

```bash
# [WSL] ~/yocto/build — 카메라 산출물
grep -nE 'dtoverlay=imx219' $D/bootfiles/config.txt                # imx219-study만, 단독 imx219 없음
ls $D | grep -E 'imx219'                                           # imx219-study.dtbo(그리고 stock imx219.dtbo)
oe-pkgdata-util list-pkg-files imx219-study | grep '\.ko'
grep -n 'CONFIG_V4L2_CCI' tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git/linux-raspberrypi4_64-standard-build/.config
```

확인: **[붙여 주세요]** 아래 항목의 출력과 빌드 시간을 붙여 주세요.
- 변수: `RPI_USE_U_BOOT="1"`, `KERNEL_IMAGETYPE="Image"`, `IMAGE_BOOT_FILES`에 `u-boot.bin;kernel8.img`·`boot.scr`, `RASPBERRYPI_CAMERA_V2`가 비었거나 "0", overlay 목록에 `imx219-study.dtbo`
- 카메라: `config.txt`에 `dtoverlay=imx219-study`만, `imx219_study.ko` 패키지, `CONFIG_V4L2_CCI_I2C=m`(또는 `y`)
- `boot.scr`: `saveenv`가 없어야 함
- `fdt_support.c`의 `u-boot,version` 줄 유무
- `.config` 줄
- 메모리 주소가 정의된 파일(`rpi.env`인지 `rpi.h`인지)
- 판정:
  - `u-boot,version` 코드가 없으면 완료 기준 2번을 대체 근거로 바꾼다. 대체 근거는 HDMI 배너, 그리고 부트 파티션 `kernel8.img`에 `strings`로 보이는 `U-Boot 2024.01`이다.
  - BOOTSTD·환경·주소의 출처가 11단계 v2026.10(`rpi.env`, `bootflow scan`)과 다르면 학습 문서에 버전 차이로 적는다.

### 5. [데스크톱 Windows] 카드 쓰기
18단계 8단계 명령 그대로(`bzcat … > /mnt/d/bsp-images/bsp-study-image.img` → `fdisk -l` → Imager "Use custom" → **32GB 카드** → `.img` 삭제).

### 6. [노트북] U-Boot 경유 부팅 확인 — HDMI 모니터를 연결한 뒤 부팅
HDMI에서 볼 것: `U-Boot 2024.01` 배너 → `Hit any key to stop autoboot`(키를 누르지 않음) → 리눅스 부팅. 사진이 있으면 좋다. Yocto 카드는 새로 써서 SSH 호스트 키가 바뀌므로, `REMOTE HOST IDENTIFICATION HAS CHANGED`가 나오면 `ssh-keygen -R <IP>`로 지운 뒤 접속한다(이 경고는 Yocto 카드 별칭에서만 남).

```bash
# [Pi] root@raspberrypi4-64 — U-Boot 후 같은 항목을 /tmp/after에 모음
mkdir -p /tmp/after
tr -d '\0' < /proc/device-tree/chosen/u-boot,version; echo     # 판정 근거 1(코드가 있을 때)
cat /proc/cmdline > /tmp/after/cmdline.txt
devcheck all > /tmp/after/devcheck.txt 2>&1; echo "devcheck=$?"
ls /proc/device-tree/chosen/ > /tmp/after/chosen-list.txt
tar -C /sys/firmware/devicetree/base -cf /tmp/after/dt.tar .
B=$(awk '$1=="/dev/mmcblk0p1" {print $2}' /proc/mounts); echo "boot=$B"   # Yocto는 부트 파티션을 이미 마운트함(18-2 1단계 실측)
ls -l $B > /tmp/after/bootpart-ls.txt
strings $B/kernel8.img | grep -m1 'U-Boot 2024.01'       # 대체 근거
ls $B/uboot.env                                           # 없어야 함
cp $B/config.txt /tmp/after/boot-config.txt
cp $B/cmdline.txt /tmp/after/boot-cmdline.txt
readlink /sys/bus/i2c/devices/1-0068/driver                       # mpu6050-iio-study
```

```bash
# [Pi] root@raspberrypi4-64 — 카메라(직접 작성 드라이버)
lsmod | grep -E '^(imx219|imx219_study|v4l2_cci) '                # imx219_study·v4l2_cci, imx219 없음
readlink /sys/bus/i2c/devices/10-0010/driver                      # .../imx219-study
dmesg | grep -i 'imx219'
rpicam-hello --list-cameras                                      # imx219 [3280x2464 10-bit RGGB], 모드 1개
rpicam-hello -t 10000 2>&1 | grep -E 'preview window|Sensor:|WARN|ERROR'   # HDMI 10초
rpicam-still -n --immediate -o /tmp/after/still.jpg 2>&1 | grep -E 'Sensor:|WARN|ERROR'; ls -l /tmp/after/still.jpg
```

```bash
# [노트북 WSL] ~/bspworkspace/bsp-study — 전후 비교
mkdir -p out/uboot18/after/dt
scp 'yocto:/tmp/after/*' out/uboot18/after/
tar -xf out/uboot18/after/dt.tar -C out/uboot18/after/dt
DTC=$(command -v dtc || echo out/kernel-rpi4/scripts/dtc/dtc)
$DTC -I fs -O dts -o out/uboot18/after/dt.dts out/uboot18/after/dt 2>/dev/null
cd out/uboot18
diff before/cmdline.txt after/cmdline.txt && echo cmdline-same
diff before/boot-config.txt after/boot-config.txt && echo config-same
diff before/boot-cmdline.txt after/boot-cmdline.txt && echo bootcmdline-same
diff before/bootpart-ls.txt after/bootpart-ls.txt
diff before/chosen-list.txt after/chosen-list.txt
diff -u before/dt.dts after/dt.dts > dt.diff; grep -c '^[-+][^-+]' dt.diff
grep -E '^[-+]' dt.diff | grep -vE '^(\+\+\+|---)' | head -40
grep -E 'mpu6050|pca9685|inmp441|srf04|pwm|imx219' after/dt.dts | head
```

확인: **[붙여 주세요]** 아래 출력과 HDMI에서 본 내용을 붙여 주세요.
- `u-boot,version` 값, `devcheck=0`과 6개 PASS
- `uboot.env` 없음, `strings`에 `U-Boot 2024.01`
- `cmdline-same`·`config-same`·`bootcmdline-same`, 부트 파티션 파일 목록 차이
- `chosen` 목록 차이, `dt.diff` 바뀐 줄 수와 앞 40줄, overlay 노드 grep
- 카메라: `lsmod`에 `imx219_study`(mainline `imx219` 없음), `10-0010` 드라이버 `imx219-study`, dmesg `chip ID: 0x0219`, 카메라 목록 모드 1개, `Made DRM preview window`와 HDMI에 바른 방향·정상 색 영상, 센서 포맷 `SBGGR10_1X10`, `still.jpg`를 노트북에서 확인(`scp 'yocto:/tmp/after/still.jpg' out/uboot18/after/`)
- 판정:
  - overlay 노드(`mpu6050`, `pca9685`, `inmp441`, `srf04`, `pwm`)는 diff에 없어야 한다. `imx219@10`은 compatible 변경(`sony,imx219` → `nakta,imx219-study`)과 vcm 노드 제거가 예상된 차이이고, 그 밖의 속성(reg·clocks·supply·endpoint·rotation)은 같아야 한다.
  - U-Boot가 바꾼 노드(`/chosen`, `/memory` 등)를 정리한다.
  - `rng-seed`·`kaslr-seed`처럼 부팅마다 바뀌는 값은 U-Boot 탓으로 보지 않고 이름만 비교한다.

선택: 시리얼 어댑터가 있으면 U-Boot 콘솔 로그를 본다.

### 7. 마무리
노트 결과, 11 학습 문서 보완(U-Boot), 18·13 학습 문서 보완(카메라 교체), 12·00 서술 갱신, main의 worklog·roadmap 링크 순서로 진행한다. 11 문서의 "DT를 바꾸지 않는다"는 6단계 diff 결과에 맞춰 고친다.

## 완료 기준
- [ ] Codex A 리뷰 통과(파일 3개, saveenv 없는 `boot.scr`)
- [ ] Codex B 리뷰 통과: 6.6 빌드 경고 0, 노트북 6.18 재빌드 경고 0(동작 변경 없음)
- [ ] U-Boot 경유 판정: `/proc/device-tree/chosen/u-boot,version` = `2024.01`. 4단계에서 해당 코드가 없으면 HDMI `U-Boot 2024.01` 배너와 부트 파티션 `kernel8.img`의 `strings` 결과로 바꿔 판정
- [ ] `/proc/cmdline`, 부트 파티션 `config.txt`·`cmdline.txt`가 U-Boot 전과 같음
- [ ] DT diff에서 장치 overlay 노드 유지(`imx219@10`은 compatible 변경만), U-Boot가 바꾼 노드를 목록으로 정리
- [ ] `devcheck all` 6개 PASS(18단계와 같음)
- [ ] 부트 파티션에 `uboot.env` 없음
- [ ] `imx219_study` 자동 적재, mainline `imx219` 미적재, `config.txt`에 `dtoverlay=imx219-study`만
- [ ] libcamera 0.4.0이 `imx219`로 인식, HDMI 미리보기와 `rpicam-still`이 바른 방향·정상 색

## 결과
대표 출력 몇 줄과 판단만 적는다.

- 
- 미검증:
