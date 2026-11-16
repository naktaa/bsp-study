# Pi 4 커널: 소스 고정 → 크로스 빌드 → 별도 이름으로 설치 → 부팅·복구

## 1. 한눈에 보기
- **목적:** WSL에서 Pi 4용 커널·모듈·Device Tree를 직접 빌드하고, 기존 Debian 루트 파일시스템과 기존 커널을 보존한 채 새 커널로 부팅한다. 이후 모든 외부 모듈이 이 빌드 출력(`out/kernel-rpi4`)을 `KDIR`로 쓴다.
- **기능:** Raspberry Pi `rpi-6.18.y`의 커밋 `b3495ea401`(6.18.59)를 `bcm2711_defconfig` + `LOCALVERSION="-v8-bsp-study01"`로 빌드한다. `kernel8-bsp-study01.img`·전용 DTB·전용 overlay 디렉터리·전용 initramfs를 설치하고 `config.txt` `[pi4]` 블록에서 고른다.
- **직접 작성 / 가져다 쓴 것:** 커널 소스·`bcm2711_defconfig`·Kbuild·Pi 펌웨어·`initramfs-tools`는 가져다 썼다. 직접 정한 것은 버전 접미사, 배포 파일 구성(`dtbs_install` 사용), 설치 이름, `config.txt` 선택 블록, 백업·복구 절차다.
- **결과(실측):** `uname -r` → `6.18.59-v8-bsp-study01`, ext4 루트·SSH·외부 모듈 로드 정상. 화면은 `overlay_map.dtb` 누락으로 처음에 안 나왔고, 복구 후 정상. 상세는 [6장](#6-트러블슈팅과-한계).
- **범위 밖:** WSL 자체 커널 교체, 루트 파일시스템 교체, `os_prefix`, U-Boot(→ [11](11-uboot-boot.md)).
- **관련 파일:** 커널 소스 `external/linux`(Git 대상 아님), 빌드 출력 `out/kernel-rpi4`, 환경은 [bringup](../bringup.md)

## 2. 구조

**흐름:** 소스 커밋 고정 → `bcm2711_defconfig` → 버전 설정 → `prepare` → `Image modules dtbs` → 부트 파티션 백업 → `modules_install`·`dtbs_install`로 배포 트리 → Pi 설치·`depmod`·`mkinitramfs` → `config.txt` 선택 → 재부팅 → 확인

```text
 +----------------+  O=out/kernel-rpi4  +-------------------+  tar + scp  +-------------------------+
 | external/linux |-------------------->| Image, *.ko, dtbs |------------>| Pi /boot/firmware       |
 | b3495ea401     |                     | Module.symvers    |             | kernel8-bsp-study01.img |
 +----------------+                     +-------------------+             | overlays-bsp-study01/   |
                                                                          | initramfs8-bsp-study01  |
                                                                          +-------------------------+
```

컴파일은 WSL에서, initramfs 생성은 Pi에서 한다. 기존 `kernel8.img`·`initramfs8`·`overlays/`·`/lib/modules/6.18.50+rpt-rpi-v8`은 그대로 두고 새 이름 파일을 옆에 추가한다.

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 펌웨어 | `start4.elf`, `config.txt` `[pi4]` | `kernel=`·`device_tree=`·`overlay_prefix=`로 새 파일 선택, `auto_initramfs=1`로 짝 initramfs 로드 |
| 커널 | `kernel8-bsp-study01.img` | 6.18.59-v8-bsp-study01 |
| DT | `bcm2711-rpi-4-b-bsp-study01.dtb`, `overlays-bsp-study01/` | 보드 기술 + overlay와 `overlay_map.dtb` |
| 초기 사용자 공간 | `initramfs8-bsp-study01` | 새 버전 모듈로 루트 마운트 전 준비 |
| 루트 | 기존 Debian 13 ext4 | `/lib/modules/6.18.59-v8-bsp-study01` 추가 |

## 3. 알아야 할 개념

### 왜 소스를 브랜치가 아니라 커밋으로 고정하는가
`rpi-6.18.y` 브랜치는 계속 움직인다. 모듈은 빌드한 커널과 같은 소스·설정이어야 로드되므로 커밋 `b3495ea401d709af0701a85babcba5e3d788a602`로 고정해 언제든 같은 커널을 다시 만들 수 있게 한다. 기존 Pi 커널(6.18.50)과 같은 6.18 계열을 골라 DT·overlay 자료를 그대로 쓸 수 있게 했다.

### 왜 버전 접미사와 설치 이름을 따로 두는가
`CONFIG_LOCALVERSION="-v8-bsp-study01"`이 `/lib/modules/<버전>`을 기존 것과 분리하고, 부팅 파일도 `-bsp-study01` 이름으로 추가한다. 그래서 기존 커널을 덮어쓰지 않고 `config.txt` 한 곳만 되돌리면 복구된다. 빌드 명령마다 `LOCALVERSION=`(빈 값)을 넘겨 Git 상태에 따른 `+` 접미사가 붙지 않게 한다.

### 왜 O=로 빌드 출력을 분리하는가
소스 트리를 깨끗하게 두고 `.config`·생성 헤더·`Module.symvers`·산출물을 `out/kernel-rpi4`에 모은다. 외부 모듈 Makefile의 `KDIR`이 이 경로를 가리키므로([02](02-kernel-module.md#왜-커널-소스가-아니라-빌드-출력을-kdir로-쓰는가)) 위치가 분명해야 한다.

### 왜 initramfs를 Pi에서 새로 만드는가
initramfs는 루트를 마운트하기 전의 초기 사용자 공간이고, 안에 그 커널 버전의 모듈이 들어간다. 배포판 도구(`initramfs-tools`)와 설정이 Pi에 있으므로 Pi에서 `mkinitramfs -o ... 6.18.59-v8-bsp-study01`로 **버전을 명시해** 만든다. 버전을 빼면 실행 중인 기존 커널용이 만들어진다.

### 왜 overlay_map.dtb까지 배포해야 하는가
`dtoverlay=vc4-kms-v3d`는 일반 이름이고, Pi 4에서는 `overlay_map.dtb`가 이것을 `vc4-kms-v3d-pi4`로 바꿔 준다. 이 파일이 없으면 Pi 4에 맞지 않는 overlay가 적용돼 화면이 안 나온다. `.dtbo`만 골라 복사하면 빠지므로 Kbuild의 `dtbs-list`를 따르는 `dtbs_install`로 모은다.

## 4. 구현 흐름

### 4-1. 설정과 버전 확정
`bcm2711_defconfig`로 `.config`를 만들고 `scripts/config`로 `LOCALVERSION`·`LOCALVERSION_AUTO=n`을 바꾼 뒤 `olddefconfig`로 의존성을 맞춘다. **`prepare`까지 해야** 버전 생성에 쓰이는 `include/config/auto.conf`가 갱신되고, `kernelrelease`가 `6.18.59-v8-bsp-study01`을 출력한다.

### 4-2. 빌드 인자와 산출물

| 지정·산출물 | 역할 |
| --- | --- |
| `-C external/linux O=$PWD/out/kernel-rpi4` | 소스의 Makefile 사용, 출력 분리 |
| `ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-` | WSL에서 ARM64 커널 생성 |
| `-j4 Image modules dtbs` | 커널 본체·`=m` 모듈·DT를 병렬 빌드 |
| `arch/arm64/boot/Image` | 커널 본체(약 28MiB) |
| `arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb` | Pi 4 하드웨어 기술 |
| `arch/arm64/boot/dts/overlays/` | `.dtbo`와 `overlay_map.dtb`·`hat_map.dtb` |
| `Module.symvers` | 외부 모듈 빌드에 쓰는 커널 심볼 버전 |

### 4-3. 배포 트리 구성
`modules_install INSTALL_MOD_PATH=out/pi4-install`은 모듈을 `/lib/modules/<버전>` 구조로 모으고 의존성 정보를 만든다(WSL 시스템에는 설치하지 않음). `dtbs_install INSTALL_DTBS_PATH=out/pi4-dtbs`는 `dtbs-list` 목록대로 DT 파일을 모은다. 여기에 Image·DTB·overlay를 새 이름으로 복사해 tar로 묶는다. `lib/modules/<버전>/build` 링크는 WSL 경로를 가리키므로 Pi에서는 빌드에 쓰지 않는다.

### 4-4. 설치와 부팅 선택
Pi에서 묶음을 풀어 `/lib/modules`·`/boot/firmware`에 복사하고 `depmod -a <버전>`, `mkinitramfs`를 실행한다. `config.txt`의 `[pi4]` 블록에 `kernel=`·`device_tree=`·`overlay_prefix=`를 넣는다. `overlay_prefix`는 기존 `dtoverlay` 줄보다 앞에 있어야 하고, 뒤의 `[all]`은 그 아래부터 공통 설정으로 되돌린다.

### 4-5. VEDA 수업 자료와의 차이
기준은 VEDA `리눅스커널/강의자료/사물인터넷12장_커널프로그래밍.pdf` 10쪽(소스)·29쪽(설정·빌드)·30~31쪽(설치)이다(저장소 밖이라 텍스트로 적음).

| 수업 | 이번 실습 | 이유 |
| --- | --- | --- |
| `rpi-6.12.y`, `/usr/src` | `rpi-6.18.y` 커밋 고정, 저장소 `external/` | 보드 커널과 같은 계열, sudo 없이 관리 |
| SD 카드를 마운트해 커널 이미지 덮어쓰기 | SSH 전송, 새 이름으로 추가 | 기존 커널 보존과 config.txt 한 곳 복구 |
| initramfs 언급 없음 | Pi에서 새 버전용 생성 | 새 모듈로 초기 사용자 공간 구성 |

## 5. 재현

선행 조건: WSL2 Ubuntu 22.04.5(ARM64 GCC 11.4.0, binutils 2.38, Make 4.3), Pi 4 Model B Rev 1.5 + Debian 13, SSH 별칭 `pi`, Pi에 `sudo`·`kmod`·`initramfs-tools`. 오류가 나면 다음 블록으로 넘어가지 않는다. 이미 끝난 설치를 확인 목적으로 다시 실행할 필요는 없다.

### 1. 도구와 소스 준비

```bash
# [WSL] ~/bspworkspace/bsp-study — 도구(새 환경에서만)·소스 커밋 고정
sudo apt install git build-essential crossbuild-essential-arm64 bc bison flex \
    libssl-dev libelf-dev libncurses-dev dwarves python3 cpio kmod xz-utils zstd file
git clone --depth=1 --branch rpi-6.18.y https://github.com/raspberrypi/linux.git external/linux
git -C external/linux fetch --depth=1 origin b3495ea401d709af0701a85babcba5e3d788a602
git -C external/linux switch --detach b3495ea401d709af0701a85babcba5e3d788a602
```

### 2. 설정과 버전명

```bash
# [WSL] ~/bspworkspace/bsp-study — 기존 .config를 조정했다면 먼저 보존
K="make -C external/linux O=$PWD/out/kernel-rpi4 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-"
mkdir -p out/kernel-rpi4
$K bcm2711_defconfig
external/linux/scripts/config --file out/kernel-rpi4/.config \
    --set-str LOCALVERSION "-v8-bsp-study01" --disable LOCALVERSION_AUTO
$K LOCALVERSION= olddefconfig
$K LOCALVERSION= prepare
$K -s LOCALVERSION= kernelrelease      # 6.18.59-v8-bsp-study01
```

### 3. 빌드

```bash
# [WSL] ~/bspworkspace/bsp-study
make -C external/linux O="$PWD/out/kernel-rpi4" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
    LOCALVERSION= -j4 Image modules dtbs; echo $?     # 0
```

### 4. 설치 전 백업
기존 정상 부팅 상태에서 한 번만 한다. 최초 백업 `out/boot-backup-20261102-222418/firmware/`를 덮어쓰지 않는다. 부트 파티션만 백업하며 루트 파일시스템 전체 백업은 아니다.

```bash
# [WSL] ~/bspworkspace/bsp-study
bsp_backup="$PWD/out/boot-backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$bsp_backup" && scp -r pi:/boot/firmware "$bsp_backup/"
ls -lh "$bsp_backup/firmware"/{config.txt,cmdline.txt,kernel8.img,initramfs8}
```

### 5. 배포 트리 구성·전송

```bash
# [WSL] ~/bspworkspace/bsp-study — modules_install·dtbs_install로 모으고 새 이름으로 복사
K="make -C external/linux O=$PWD/out/kernel-rpi4 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="
F=out/pi4-install/boot/firmware
$K INSTALL_MOD_PATH="$PWD/out/pi4-install" modules_install
$K INSTALL_DTBS_PATH="$PWD/out/pi4-dtbs" dtbs_install
mkdir -p $F/overlays-bsp-study01
cp out/kernel-rpi4/arch/arm64/boot/Image $F/kernel8-bsp-study01.img
cp out/kernel-rpi4/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb $F/bcm2711-rpi-4-b-bsp-study01.dtb
cp -a out/pi4-dtbs/overlays/. $F/overlays-bsp-study01/
cp external/linux/arch/arm64/boot/dts/overlays/README $F/overlays-bsp-study01/
ls $F/overlays-bsp-study01/{overlay_map.dtb,hat_map.dtb,vc4-kms-v3d.dtbo,vc4-kms-v3d-pi4.dtbo}
tar -C out/pi4-install -czf out/pi4-study01.tar.gz .
ssh pi 'mkdir -p ~/bsp/kernel'
scp out/pi4-study01.tar.gz pi:bsp/kernel/
scp out/kernel-rpi4/.config pi:bsp/kernel/config-6.18.59-v8-bsp-study01
```

### 6. Pi에 설치·initramfs 생성

```bash
# [Pi] ~/bsp/kernel — 새 파일 추가만 하고 기존 파일은 덮어쓰지 않음
mkdir -p pi4-study01 && tar -xzf pi4-study01.tar.gz -C pi4-study01
sudo cp -a --no-preserve=ownership pi4-study01/lib/modules/. /lib/modules/
sudo cp -a --no-preserve=ownership pi4-study01/boot/firmware/. /boot/firmware/
sudo install -m 644 config-6.18.59-v8-bsp-study01 /boot/config-6.18.59-v8-bsp-study01
sudo depmod -a 6.18.59-v8-bsp-study01
sudo mkinitramfs -o /boot/firmware/initramfs8-bsp-study01 6.18.59-v8-bsp-study01; echo $?   # 0
ls -d /lib/modules/6.18.50+rpt-rpi-v8 /lib/modules/6.18.59-v8-bsp-study01   # 둘 다 있음
```

필요한 파일이 없으면 다른 버전에서 가져오지 말고 배포 목록과 빌드 출력을 확인한다.

### 7. 부팅 선택·검증

2026-11-04 사용자 제공 `config.txt` 전문에서 이번 변경과 관련된 줄만 옮겼다. 실제 파일에는 `dtparam=spi=on`·`display_auto_detect=1`·`max_framebuffers=2` 등 기존 설정과 `[cm4]`·`[cm5]`·`[pi5]` 블록, 마지막 `[all] enable_uart=1`이 그대로 있다.

```ini
[pi4]
kernel=kernel8-bsp-study01.img
device_tree=bcm2711-rpi-4-b-bsp-study01.dtb
overlay_prefix=overlays-bsp-study01/

[all]
dtparam=i2c_arm=on
dtparam=audio=on
camera_auto_detect=1
auto_initramfs=1
dtoverlay=vc4-kms-v3d
```

편집이 필요할 때만 `sudo nano /boot/firmware/config.txt`로 기존 설정을 유지하며 `[pi4]` 블록을 추가한다.

```bash
# [Pi] 재부팅 → [WSL] ssh pi → [Pi] 확인
sudo reboot
uname -r; uname -m                # 6.18.59-v8-bsp-study01 / aarch64
findmnt /                         # /dev/mmcblk0p2 ext4
ls -ld /lib/modules/"$(uname -r)"
```

이어서 기존에 쓰던 화면이 실제로 나오는지 직접 확인한다. `uname`만으로 DT·initramfs 선택이나 화면까지 확인됐다고 보지 않는다.

### 8. 기존 커널로 복구
기존 커널·DTB·overlay·initramfs·모듈은 모두 그대로 있으므로 `config.txt`만 되돌리면 된다.

1. SSH가 안 되면 Pi 전원을 끄고 SD 카드를 노트북 카드 리더기에 꽂는다.
2. Windows 탐색기에서 FAT 부트 파티션을 연다(`config.txt`가 최상위에 있다).
3. `\\wsl.localhost\<WSL 배포판 이름>\home\nakta\bspworkspace\bsp-study\out\boot-backup-20261102-222418\firmware\config.txt`를 부트 파티션의 `config.txt`로 복사한다.
4. 안전하게 분리해 다시 부팅하고 `uname -r`이 `6.18.50+rpt-rpi-v8`인지 확인한다.

## 6. 트러블슈팅과 한계

트러블슈팅: [kernel 6장](../components/kernel.md#6-트러블슈팅). 아래는 그중 화면 장애의 사례 분석이다.

### 사례 분석: SSH는 되지만 커스텀 커널 화면이 나오지 않음
- **증상:** `overlay_prefix=overlays-bsp-study01/`, `dtoverlay=vc4-kms-v3d` 구성에서 화면 없음. 새 디렉터리에 `vc4-kms-v3d.dtbo`·`vc4-kms-v3d-pi4.dtbo`는 있고 `overlay_map.dtb`만 없었다. 사용자 제공 로그 발췌: `dterror: can't find symbol 'i2c2'`, `Failed to resolve overlay 'vc4-kms-v3d'`.
- **원인:** 선택 커밋의 `arch/arm/boot/dts/overlays/overlay_map.dts`에 `vc4-kms-v3d { bcm2711 = "vc4-kms-v3d-pi4"; ... }` 규칙이 있다. 최초 배포 명령이 `*.dtbo`와 README만 복사해 이 매핑 파일이 빠졌다. 빌드 출력에는 `overlay_map.dtb`가 이미 있었으므로 **컴파일 누락이 아니라 배포 파일 선택에서 빠진 것**이다.
- **당시 복구:** 기존 `/boot/firmware/overlays/overlay_map.dtb`를 `overlays-bsp-study01/`로 복사하고 재부팅 → 사용자가 정상 화면을 확인했다. 커널은 다시 빌드하지 않았다.
- **재발 방지:** `scripts/Makefile.dtbinst`가 `dtbs-list`를 설치 목록으로 쓰고, 그 목록에 `overlay_map.dtb`·`hat_map.dtb`·보드별 KMS DTBO가 있음을 읽기 확인했다. 그래서 5단계를 `dtbs_install` 방식으로 바꿨다.

**현재 버전 검증 (2026-11-04)**

| 항목 | 결과 |
| --- | --- |
| 빌드 | make 종료 상태 0, Image 약 28MiB, 모듈 1,917개, DT·`Module.symvers` |
| 설치 | 새 이름 커널·DTB·overlay, initramfs 약 18MiB, 기존·새 모듈 디렉터리 공존 |
| 부팅 | `6.18.59-v8-bsp-study01`, aarch64, `/dev/mmcblk0p2` ext4 루트, SSH 재접속 |
| 모듈 | [hello 모듈](02-kernel-module.md) 로드·해제 |
| 화면 | 최초 미출력 → `overlay_map.dtb` 복사 후 정상 |
| 최종 설정 | `config.txt` 전문과 `uname`으로 재확인 |

- 미검증: `dtbs_install` 방식 5단계 전체 재실행(현재 Pi의 `overlay_map.dtb`는 기존 디렉터리에서 복사한 파일), 별칭 `pi`·`~/bsp/kernel` 경로 재현(당시에는 IP 주소와 Pi 홈 사용), 기존 커널로의 실제 복귀 시험, 런타임에 쓰인 DT·initramfs 파일 식별.
- 한계: 백업은 부트 파티션만이라 루트 파일시스템이 깨지면 이 절차로 복구할 수 없다. 화면 장애 근거는 사용자 제공 로그 발췌와 남은 배포 파일 대조다.

**더 보기**
- `os_prefix`를 왜 안 썼나 → 필수가 아니고, 파일마다 `kernel=`·`device_tree=`·`overlay_prefix=`로 고르는 편이 기존 파일 보존에 단순했다
- 선택 소스의 최소 도구 요구는? → ARM64 GCC 8.1, binutils 2.30, Make 4.0
- tar로 묶는 이유는? → 디렉터리 구조와 심볼릭 링크를 보존하고, 링크 대상(WSL 커널 트리)을 따라가 보내지 않는다
- 공식 자료 → [Raspberry Pi 커널 빌드](https://www.raspberrypi.com/documentation/computers/linux_kernel.html), [config.txt·auto_initramfs](https://www.raspberrypi.com/documentation/computers/config_txt.html), [overlay_map.dts(선택 커밋)](https://github.com/raspberrypi/linux/blob/b3495ea401d709af0701a85babcba5e3d788a602/arch/arm/boot/dts/overlays/overlay_map.dts), [Debian mkinitramfs](https://manpages.debian.org/trixie/initramfs-tools-core/mkinitramfs.8.en.html)
