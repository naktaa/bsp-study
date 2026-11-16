# Yocto: meta-bsp-study 레이어와 bsp-study-image

## 1. 요약
- **역할:** scarthgap poky + meta-raspberrypi 위에 직접 만든 레이어 하나로 Yocto 카드 이미지를 만든다. 커널 config·overlay·`config.txt`·직접 작성 모듈·U-Boot·카메라 스택·Wi-Fi가 모두 레시피로 들어가고, 카드를 쓰고 부팅하면 장치가 자동으로 올라온다.
- **상태:** `bsp-study-image` 빌드·카드 부팅, Wi-Fi SSH, 6개 장치 자동 적재(`devcheck all` 6개 PASS), U-Boot 2024.01 경유 확인. 다른 PC에서 처음부터 빌드하는 재현은 미검증([12 6장](../learning/12-yocto-minimal.md#6-트러블슈팅과-한계), [18 6장](../learning/18-integration.md#6-트러블슈팅과-한계)).
- **담당 파일:** [yocto/meta-bsp-study/](../../yocto/meta-bsp-study/) 전체. 빌드 디렉터리·`downloads`·`sstate-cache`·`wifi-secret.conf`는 저장소 밖 `~/yocto`
- **학습 문서:** [12 최소 이미지](../learning/12-yocto-minimal.md), [18 통합](../learning/18-integration.md)

## 2. 구성
흐름: `TEMPLATECONF → build/conf → 레이어 4개 + MACHINE → bitbake bsp-study-image → .wic.bz2 → 32GB 카드 → firmware → U-Boot → 커널 → 자동 적재`

| 레이어 파일 | 하는 일 |
| --- | --- |
| `conf/templates/bsp-study/local.conf.sample` | `MACHINE = "raspberrypi4-64"`, `DL_DIR`·`SSTATE_DIR`(`~/yocto` 아래), `require conf/bsp-study-devices.inc`, `include wifi-secret.conf` |
| `conf/bsp-study-devices.inc` | overlay 목록(`RPI_KERNEL_DEVICETREE_OVERLAYS`), `config.txt` 줄(`RPI_EXTRA_CONFIG`), `ENABLE_I2C`, `i2c-dev` 자동 적재, `RPI_USE_U_BOOT` |
| `recipes-kernel/linux/linux-raspberrypi_%.bbappend` | config 조각 `bsp-study.cfg`(`CONFIG_SRF04=m`), 직접 작성 overlay `.dts` 4개를 커널 트리에 복사 |
| `recipes-kernel/{mpu6050-iio,inmp441-codec,imx219-study}` | `inherit module`로 외부 모듈 빌드·설치 |
| `recipes-bsp/rpi-u-boot-scr` | `saveenv`를 뺀 `boot.cmd.in` |
| `recipes-multimedia/libcamera` | `PACKAGECONFIG:append = " gst"` |
| `recipes-apps/devcheck` | 확인 앱 |
| `recipes-connectivity/bsp-study-wifi`, `recipes-core/init-ifupdown` | wlan0 대기 스크립트, `interfaces` 교체 |
| `recipes-core/images/bsp-study-image.bb` | core-image-minimal + dropbear(비밀번호 없는 root) + 위 패키지, Wi-Fi 설정을 rootfs 후처리로 씀 |

| 항목 | 값 |
| --- | --- |
| poky / meta-raspberrypi | scarthgap 5.0.21 / `018d444` |
| 커널 | `linux-raspberrypi` 6.6.63(`rpi-default-versions.inc`의 `6.6.%`) |
| 소스 위치 | 모듈·overlay·devcheck 레시피가 `FILESEXTRAPATHS`로 `examples/`를 직접 가져감. 드라이버 수정은 `examples/` 한 곳 |

## 3. 빌드·배포
선행 조건: `~/yocto`에 poky·meta-raspberrypi·meta-openembedded, `~/yocto/wifi-secret.conf`(권한 600). 처음 만드는 절차는 [12 5장](../learning/12-yocto-minimal.md#5-재현).

```bash
# [WSL, tmux yocto] — 새 셸마다. 공백 있는 Windows PATH(/mnt/...)를 빌드 셸에서만 뺀다
tmux new -A -s yocto
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
cd ~/yocto && source poky/oe-init-build-env build
time bitbake bsp-study-image
```

```bash
# [데스크톱 WSL] ~/yocto/build — 카드 쓰기용 .img (쓴 뒤 삭제: Wi-Fi psk가 들어 있음)
bzcat tmp/deploy/images/raspberrypi4-64/bsp-study-image-raspberrypi4-64.rootfs.wic.bz2 > /mnt/d/bsp-images/bsp-study-image.img
```

Raspberry Pi Imager → "Use custom" → **32GB 카드**(64GB 카드 아님) → 커스터마이즈 "아니요". 카드 전체를 덮어쓰므로 이전 이미지는 `~/yocto/backup/`에 `.wic.bz2`로 둔다.

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| 빌드 변수 | `bitbake -e bsp-study-image \| grep -E '^(RPI_USE_U_BOOT\|IMAGE_BOOT_FILES)='` | `"1"`, 끝에 `Image u-boot.bin;kernel8.img boot.scr` |
| `config.txt` | `grep -n dtoverlay tmp/deploy/images/raspberrypi4-64/bootfiles/config.txt` | 장치 overlay 6줄(`mpu6050-iio`·`i2c-pwm-pca9685a`·`inmp441-study`·`srf04`·`pwm-2chan`·`imx219-study`) |
| 모듈 패키지 | `oe-pkgdata-util list-pkg-files mpu6050-iio` | `/lib/modules/6.6.63-v8/updates/*.ko.xz` |
| 부팅 | `[Yocto] uname -r` | `6.6.63-v8` |
| 네트워크 | 노트북 `ssh yocto` | 비밀번호 없이 root 접속 |
| 장치 | `[Yocto] devcheck all` | 6개 PASS |

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| 직접 작성 드라이버 수정 | `examples/<장치>/*.c` | 6.6에 없는 API면 compat 헤더 | `bitbake <모듈 레시피>` 경고 0 |
| overlay 추가 | `examples/<장치>/<이름>-overlay.dts` | bbappend의 `FILESEXTRAPATHS`·`SRC_URI`·`do_patch` 한 줄씩, `bsp-study-devices.inc`의 `RPI_KERNEL_DEVICETREE_OVERLAYS`·`RPI_EXTRA_CONFIG` | 4장 `config.txt` 줄, deploy에 `<이름>.dtbo` |
| 커널 config 추가 | `recipes-kernel/linux/files/bsp-study.cfg` | 64GB용 `configs/bsp-study.config`에 같은 줄([kernel](kernel.md)) | 커널 빌드 트리 `.config` grep |
| 새 장치 추가 | 위 overlay 항목 + 모듈 레시피(`mpu6050-iio.bb` 형식, 레시피용 Makefile에 `clean` 포함) | `bsp-study-image.bb`의 `IMAGE_INSTALL`, `devcheck.c`에 `check_*` 함수와 `main` 호출 | `devcheck <이름>` |
| 패키지 추가 | `bsp-study-image.bb`의 `IMAGE_INSTALL:append` | rootfs 여유(`IMAGE_ROOTFS_EXTRA_SPACE` 512MB) | `oe-pkgdata-util` |
| Wi-Fi 바꾸기 | `~/yocto/wifi-secret.conf`(저장소 밖) | rootfs만 다시 빌드, 카드 다시 쓰기 | 4장 네트워크 |
| 템플릿 변경 | `conf/templates/bsp-study/*.sample` | 이미 만든 `build/conf/local.conf`에는 직접 같은 줄 추가 | `bitbake -e` |

## 6. 트러블슈팅
먼저 볼 계층: 1 빌드(`tmp/work/.../temp/log.do_*`) → 2 부팅.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 1 빌드 | 외부 모듈 레시피 `do_configure`가 `No rule to make target 'clean'` | 레시피용 `files/Makefile`에 `clean:`이 있는지 | 해시가 바뀐 레시피는 `base_do_configure`가 `oe_runmake clean`을 먼저 실행(poky `base.bbclass` L335~341) | `clean` 타깃 추가(현재 폴더만 `rm -f`) |
| 1 빌드 | `do_package`가 pseudo `inode mismatch`·`path mismatch`로 abort(exit 134) | pseudo 로그 `tmp/work/.../<레시피>/1.0/pseudo/pseudo.log` | `clean`이 Kbuild clean(`M=${WORKDIR}`)을 불러 `image/`·`package/`의 `.ko.xz`를 pseudo 밖에서 지움 | [조치 절차: pseudo abort](#조치-절차-pseudo-abort) |
| 1 빌드 | 템플릿에 넣은 줄이 `bitbake -e`에 안 보임 | `build/conf/local.conf` | `build/conf`는 처음 한 번만 템플릿에서 복사됨 | `build/conf/local.conf`에 직접 추가 |
| 1 빌드 | 첫 빌드 `Failed to fetch URL` 경고(v4l-utils 등) | 빌드가 계속 진행되는지 | 원래 주소 접속 실패, 미러에서 받음 | 조치 없음 |
| 1 빌드 | `.img`가 예상(0.9~1.0GB)보다 큰 1.2GB | `fdisk -l`의 ext4 크기 | wic가 `ROOTFS_SIZE`(이미 ×1.3 + 512MB)에 기본 overhead 1.3을 한 번 더 곱함 | 32GB 카드라 그대로 둠 |
| 1 빌드 | 1차 이미지에 `/lib/modules`가 없음 | `IMAGE_INSTALL` | `kernel-modules`가 `MACHINE_EXTRA_RRECOMMENDS`에만 있고 core-image-minimal은 이를 쓰지 않음 | 이미지 레시피에 `kernel-modules` 추가 |
| 8 사용자 공간 | wlan0은 있는데 연결 안 됨, `udhcpc`가 discover만 반복 | `iw dev wlan0 scan \| grep SSID:`와 설정 SSID | `wifi-secret.conf`의 SSID 대소문자 오류. 이미지에는 hex로 들어가 눈으로 비교가 어려움 | 수정 → rootfs 재빌드(약 2분) → 카드 다시 쓰기 |
| 8 사용자 공간 | `logread`가 비어 있음 | `/var/log/messages` | busybox syslog가 파일에 씀 | `grep bsp-study-wifi /var/log/messages` |

### 조치 절차: pseudo abort
1. 레시피용 Makefile의 `clean`이 하위 폴더를 지우지 않는지 본다(`rm -f *.o *.ko ...` 형식).
2. pseudo DB가 이미 꼬였으므로 해당 레시피 작업 폴더를 지운다. sstate·다운로드·다른 레시피는 그대로 남는다.
3. 이미지를 다시 빌드한다.

```bash
# [WSL, tmux yocto] ~/yocto/build
bitbake -c clean mpu6050-iio inmp441-codec imx219-study
time bitbake bsp-study-image
```

### 조치 절차: 이전 이미지로 카드 복구
1. `~/yocto/backup/`의 이전 `.wic.bz2`를 `.img`로 푼다.
2. Imager로 32GB 카드에 다시 쓴다. 다 쓰면 `.img`를 지운다.
3. 부팅해 `devcheck all`을 본다.

```bash
# [데스크톱 WSL]
bzcat ~/yocto/backup/<이전 이미지>.wic.bz2 > /mnt/d/bsp-images/bsp-study-image.img
```

준비만 했고 실제 복구는 하지 않았다. U-Boot나 카메라만 되돌릴 때는 카드 리더로 부트 파티션만 고친다([uboot](uboot.md#조치-절차-u-boot-빼고-직접-부팅), [camera](camera.md#6-트러블슈팅)).

**점검 포인트**
- 빌드 셸에는 Windows PATH(`/mnt/c/...`, 공백 포함)를 넣지 않기로 했다(12 작업 노트 결정, `wsl.conf`는 그대로). 남은 채로 빌드해 본 적은 없다. tmux 새 창·분할 창마다 3장 첫 블록을 다시 실행한다.
- Windows가 절전에 들어가면 WSL도 멈춘다. 긴 빌드 전에 절전 "안 함"을 확인한다.
- Yocto 카드는 busybox라 `ip -br`·`head -3` 같은 옵션이 없고, `sudo`·i2c-tools도 없다(root로 실행).

## 7. 알려진 제한
- 이미지와 카드에 Wi-Fi `psk`(PBKDF2 값)가 들어 있고 root SSH에 비밀번호가 없다. 학습용 보드라 보안은 고려하지 않았다.
- 커널이 6.6.63이라 64GB 카드(6.18.59)와 API가 다르다. 직접 작성 드라이버는 compat 헤더로 맞춘다.
- 부트 파티션은 `/boot`에 이미 마운트되어 있다(따로 `mount`하지 않는다).
