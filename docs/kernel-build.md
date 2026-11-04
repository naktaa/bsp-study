# Pi 4 커널 크로스 빌드·설치

[VEDA 커널프로그래밍 PDF](../../VEDA/리눅스커널/강의자료/사물인터넷12장_커널프로그래밍.pdf) 10쪽(소스), 29쪽(설정·빌드), 30~31쪽(설치)을 기준으로 진행했다.
선택한 소스·설정과 현재 재현 절차를 중심으로 설명한다. 과거 실행 결과와 남은 확인은 [검증 현황](#검증-현황), 기술적 장애는 [실제 트러블슈팅](#실제-트러블슈팅)에 모았다.

목차:

- [목적과 전체 흐름](#목적핵심-개념)
- [환경·빌드 인자·산출물](#환경과-선행-조건)
- [소스 확보부터 복구까지의 재현 절차](#실행-순서)
- [검증 현황](#검증-현황)
- [실제 트러블슈팅](#실제-트러블슈팅)
- [공식 자료](#공식-자료)

## 목적·핵심 개념

WSL에서 Pi용 커널·모듈·Device Tree를 만들고 기존 Debian 루트 파일시스템을 유지하며 설치한다. 커널 부팅과 SSH 접속만으로 화면 등 기존 기능까지 정상이라고 판정하지 않는다.

```text
WSL: 도구·커널 소스 확보와 커밋 고정
  → bcm2711_defconfig → 커스텀 버전 설정 → 생성 설정 준비
  → Image·modules·dtbs 크로스 빌드
기존 Pi 부팅 파일을 WSL에 백업
WSL: 모듈·DT 설치 목록으로 배포 트리 구성 → Pi로 전송
Pi: 새 버전 모듈·별도 이름의 부팅 파일 설치
  → depmod → 새 커널용 initramfs 생성
  → config.txt에서 새 커널·DT·overlay 선택
  → 재부팅 → 커널·루트·모듈·SSH·화면 확인
문제가 생기면 보존한 파일·config.txt로 기존 커널 선택 복구
```

| 선택 | 이유와 의존성 |
| --- | --- |
| Raspberry Pi의 6.18 계열 소스 | 기존 Pi 커널도 6.18 계열이고 Pi 4 지원 설정·DT·펌웨어용 overlay 자료를 함께 사용하기 위함 |
| 브랜치 대신 커밋 고정 | 브랜치가 이동해도 같은 소스에서 학습을 재현하기 위함 |
| bcm2711_defconfig·ARM64 GCC | Pi 4의 BCM2711과 64비트 실행 환경에 맞춤 |
| 소스와 O= 빌드 출력 분리 | 설정·산출물·외부 모듈의 참조 경로를 분명히 유지 |
| 별도 버전·부팅 파일 이름 | 기존 커널과 모듈을 덮어쓰지 않고 선택·복구할 수 있게 구성 |
| Pi에서 새 initramfs 생성 | 대상 배포판의 도구·설정과 새 버전 모듈로 초기 사용자 공간을 구성. 커널 컴파일 위치와 구분 |
| os_prefix 없이 개별 파일 선택 | 이번 보드에서 기존 파일을 보존할 수 있는 단순한 선택. os_prefix는 필수 설정이 아님 |

### 환경과 선행 조건

- 호스트: WSL2 Ubuntu 22.04.5, x86_64. 타깃: Raspberry Pi 4 Model B Rev 1.5, Debian 13, ARM64.
- 소스: `https://github.com/raspberrypi/linux.git`, `rpi-6.18.y` 계열, 커밋 **`b3495ea401d709af0701a85babcba5e3d788a602`** (6.18.59).
- 설정: `bcm2711_defconfig` + `CONFIG_LOCALVERSION="-v8-bsp-study01"` + `CONFIG_LOCALVERSION_AUTO=n`.
- 도구: ARM64 GCC 11.4.0, binutils 2.38, Make 4.3. 선택 소스의 최소 요구사항은 ARM64 GCC 8.1, binutils 2.30, Make 4.0이다.
- 경로는 WSL `~/bspworkspace/bsp-study`, 접속은 SSH 별칭 `pi` 기준이다. 커널 배포 묶음·설정은 Pi의 `~/bsp/kernel/`에 두고 시스템 설치 위치는 기존대로 유지한다.
- Pi에서 SSH·sudo·tar·kmod·initramfs-tools를 사용할 수 있어야 한다. 컴파일은 WSL, initramfs 생성은 Pi에서 한다.
- 각 명령 블록에 환경과 `cd`를 명시했다. 필요한 임시 변수는 해당 블록에서 정의한다. 오류가 나면 다음 블록으로 넘어가지 않는다.
- WSL 자체 커널을 교체하는 작업이 아니다. Pi의 커널을 바꾸고 기존 Debian 루트 파일시스템을 계속 사용한다.
- 이미 완료한 설치를 문서 확인 목적으로 재실행할 필요는 없다. 현재 환경은 [bringup](bringup.md) 참고.

### 빌드 인자와 산출물

| 지정 | 역할 |
| --- | --- |
| `-C external/linux` | 커널 소스의 Makefile 사용 |
| `O=.../out/kernel-rpi4` | 설정·생성 헤더·산출물 분리. 이후 모듈의 KDIR도 이 경로 |
| `ARCH=arm64` | 대상 커널 아키텍처 |
| `CROSS_COMPILE=aarch64-linux-gnu-` | WSL에서 실행하며 ARM64 코드를 만드는 도구 접두사 |
| `CONFIG_LOCALVERSION` | 커스텀 커널 및 `/lib/modules/<버전>` 구분 |
| `LOCALVERSION=` | 추가 접미사를 비워 Git 상태에 따른 `+` 방지. 이후 명령에서도 유지 |

| 산출물 | 역할 |
| --- | --- |
| `out/kernel-rpi4/arch/arm64/boot/Image` | 커널 본체 |
| 출력 트리의 `.ko` | 설정에서 `=m`으로 선택한 모듈 |
| `out/kernel-rpi4/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb` | Pi 4 하드웨어 기술 |
| `out/kernel-rpi4/arch/arm64/boot/dts/overlays/` | DTBO 외에 overlay_map.dtb 등 펌웨어가 사용하는 보조 파일 포함 |
| `out/kernel-rpi4/Module.symvers` | 외부 모듈 빌드에 쓰는 커널 심볼·버전 정보 |

## 실행 순서

### 1. 도구와 소스 준비

새 WSL 환경에서 도구가 없다면 설치한다. 저장소의 설정과 실제 도구 버전은 함께 확인한다.

**WSL · 작업 디렉터리: 저장소 루트**

```bash
cd ~/bspworkspace/bsp-study
sudo apt update
sudo apt install git build-essential crossbuild-essential-arm64 bc bison flex \
    libssl-dev libelf-dev libncurses-dev dwarves python3 cpio kmod xz-utils zstd file
make --version
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-ld --version
```

처음 받을 때만 clone한다. 브랜치는 움직이므로 재현 기준은 커밋이다. 이미 소스가 있으면 clone은 건너뛰고, 수정 사항이 없는지 확인한 뒤 해당 커밋을 선택한다.

**WSL · 작업 디렉터리: 저장소 루트**

```bash
cd ~/bspworkspace/bsp-study
mkdir -p external
git clone --depth=1 --branch rpi-6.18.y \
    https://github.com/raspberrypi/linux.git external/linux
git -C external/linux fetch --depth=1 origin \
    b3495ea401d709af0701a85babcba5e3d788a602
git -C external/linux switch --detach \
    b3495ea401d709af0701a85babcba5e3d788a602
git -C external/linux rev-parse HEAD
```

수업의 `rpi-6.12.y` 대신 현재 보드와 같은 6.18 계열을 선택했다. `/usr/src`로 옮기지 않고 sudo 없이 관리한다. `external/`과 `out/`은 기존 `.gitignore`로 제외된다.

### 2. Pi 4 설정과 버전명

`defconfig`는 기본 기능 선택으로 `.config`를 만든다. 기존에 조정한 설정이 있다면 다시 실행하기 전에 보존한다.

**WSL · 작업 디렉터리: 저장소 루트**

```bash
cd ~/bspworkspace/bsp-study
mkdir -p out/kernel-rpi4
make -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
external/linux/scripts/config --file out/kernel-rpi4/.config \
    --set-str LOCALVERSION "-v8-bsp-study01" --disable LOCALVERSION_AUTO
make -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= olddefconfig
make -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= prepare
make -s -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= kernelrelease
```

여기서 확인할 버전은 `6.18.59-v8-bsp-study01`이다.

### 3. 커널·모듈·Device Tree 크로스 빌드

**WSL · 작업 디렉터리: 저장소 루트**

```bash
cd ~/bspworkspace/bsp-study
make -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= \
    -j4 Image modules dtbs
echo $?
```

`-j4`는 최대 4개 작업 병렬 실행이다. 성공 기준은 종료 상태 0과 Image·모듈·DT 파일 생성이다. 개별 파일 존재만으로 전체 배포 구성이 완전하다고 판단하지 않는다.

### 4. 설치 전 백업

Pi의 기존 정상 부팅 상태에서 실행한다. 지금 다시 백업하면 현재 커스텀 설정도 포함되므로 **최초 백업을 덮어쓰지 않는다**.

**WSL · 작업 디렉터리: 저장소 루트**

```bash
cd ~/bspworkspace/bsp-study
bsp_backup="$PWD/out/boot-backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$bsp_backup"
scp -r pi:/boot/firmware "$bsp_backup/"
ls -lh "$bsp_backup/firmware"/{config.txt,cmdline.txt,kernel8.img,initramfs8}
echo "$bsp_backup"
```

확인된 최초 백업: `out/boot-backup-20261102-222418/firmware/`. 이 안의 config.txt는 변경 전 파일이다. 복구는 아래 절차 참고.
수업은 SD카드 안에서 커널 이미지를 복사했다. 이번에는 부트 파티션 전체를 WSL로 복사하고 기존 파일을 Pi에도 유지했다. 루트 파일시스템 전체 백업은 아니다. 카드 리더기는 SSH 불가 시 복구에 사용한다.

### 5. 배포 디렉터리 구성·SSH 전송

modules_install은 빌드된 모듈을 설치 구조로 복사·압축하고 의존성 정보를 정리한다. `INSTALL_MOD_PATH`는 목적지 앞에 붙는 경로이며 WSL 시스템의 `/lib/modules`에 설치하지 않는다.

**아래는 화면 장애 이후 보완한 정상 배포 절차다.** 새 `dtbs_install` 단계는 선택 커밋의 Kbuild가 만든 `arch/arm64/boot/dts/dtbs-list`에 따라 DT 파일을 `out/pi4-dtbs`로 모은다. 이 목록에는 overlay_map.dtb·hat_map.dtb와 보드별 DTBO가 포함된다. 확장자만 보고 골랐던 최초 명령 대신 이 목록을 사용한다. README는 DT 빌드 목록 밖이므로 같은 소스에서 별도로 복사한다. 실행 근거와 보완 절차의 확인 범위는 검증 현황에 구분한다.

**WSL · 작업 디렉터리: 저장소 루트**

```bash
cd ~/bspworkspace/bsp-study
make -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= \
    INSTALL_MOD_PATH="$PWD/out/pi4-install" modules_install
make -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= \
    INSTALL_DTBS_PATH="$PWD/out/pi4-dtbs" dtbs_install
mkdir -p out/pi4-install/boot/firmware/overlays-bsp-study01
cp out/kernel-rpi4/arch/arm64/boot/Image \
    out/pi4-install/boot/firmware/kernel8-bsp-study01.img
cp out/kernel-rpi4/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb \
    out/pi4-install/boot/firmware/bcm2711-rpi-4-b-bsp-study01.dtb
cp -a out/pi4-dtbs/overlays/. \
    out/pi4-install/boot/firmware/overlays-bsp-study01/
cp external/linux/arch/arm64/boot/dts/overlays/README \
    out/pi4-install/boot/firmware/overlays-bsp-study01/
ls -lh out/pi4-install/boot/firmware/overlays-bsp-study01/{overlay_map.dtb,hat_map.dtb,vc4-kms-v3d.dtbo,vc4-kms-v3d-pi4.dtbo}
tar -C out/pi4-install -czf out/pi4-study01.tar.gz .
ssh pi 'mkdir -p ~/bsp/kernel'
scp out/pi4-study01.tar.gz pi:bsp/kernel/pi4-study01.tar.gz
scp out/kernel-rpi4/.config \
    pi:bsp/kernel/config-6.18.59-v8-bsp-study01
```

tar는 디렉터리 구조와 심볼릭 링크를 보존하며, 링크 대상인 WSL 커널 트리 전체를 따라가서 전송하지 않는다. `lib/modules/<버전>/build`는 WSL 경로를 가리키므로 Pi에서 빌드용으로 사용하지 않는다. 컴파일은 계속 WSL에서 한다.

### 6. Pi에 설치·새 initramfs 생성

수업의 SD 마운트 경로 복사 대신 SSH 전송 후 Pi에서 설치한다. 아래 작업은 새 버전 모듈 디렉터리와 별도 이름의 부팅 파일을 추가하며 기존 커널·DT·overlay·initramfs를 보존한다.

**Pi · 작업 디렉터리: `~/bsp/kernel` · 새 묶음과 .config 전송 완료 후**

```bash
cd ~/bsp/kernel
mkdir -p pi4-study01
tar -xzf pi4-study01.tar.gz -C pi4-study01
sudo cp -a --no-preserve=ownership pi4-study01/lib/modules/. /lib/modules/
sudo cp -a --no-preserve=ownership pi4-study01/boot/firmware/. /boot/firmware/
sudo install -m 644 config-6.18.59-v8-bsp-study01 \
    /boot/config-6.18.59-v8-bsp-study01
sudo depmod -a 6.18.59-v8-bsp-study01
sudo mkinitramfs -o /boot/firmware/initramfs8-bsp-study01 \
    6.18.59-v8-bsp-study01
echo $?
```

initramfs는 루트 파일시스템을 사용하기 전의 초기 사용자 공간이다. mkinitramfs에 새 버전을 명시해야 현재 실행 중인 기존 커널용으로 잘못 생성하지 않는다. 컴파일이 아니라 설치용 초기 환경 구성 작업이다. 기존 initramfs를 재사용하거나 WSL 빌드를 이유로 제거하지 않았다.

**Pi · 작업 디렉터리: 홈**

```bash
cd ~
ls -lh /boot/firmware/kernel8-bsp-study01.img \
    /boot/firmware/bcm2711-rpi-4-b-bsp-study01.dtb \
    /boot/firmware/overlays-bsp-study01/overlay_map.dtb \
    /boot/firmware/overlays-bsp-study01/vc4-kms-v3d.dtbo \
    /boot/firmware/overlays-bsp-study01/vc4-kms-v3d-pi4.dtbo \
    /boot/firmware/initramfs8-bsp-study01
ls -d /lib/modules/6.18.50+rpt-rpi-v8 /lib/modules/6.18.59-v8-bsp-study01
lsinitramfs /boot/firmware/initramfs8-bsp-study01 \
    | grep '6.18.59-v8-bsp-study01' | tail -3
```

이번 복구는 기존 overlay_map.dtb 복사였지만, 위 정상 절차에서는 **선택한 6.18.59 소스의 빌드 결과**를 설치한다. 필요한 파일이 없다면 다른 버전에서 조용히 가져오지 말고 배포 목록·빌드 출력을 확인한다.

### 7. 부팅 선택·검증

2026-11-04 사용자 제공 `cat /boot/firmware/config.txt` **전문**과 `uname -r`로 화면 복구 후 설정·커널 버전을 확인했다. 아래는 해당 파일에서 주석·빈 줄을 제외한 활성 설정이다. [pi4] 선택 블록 뒤의 [all]과 마지막 [all]도 실제 파일에 존재한다.

**Pi · 작업 디렉터리: 홈, 읽기 확인**

```bash
cd ~
cat /boot/firmware/config.txt
```

```ini
[pi4]
kernel=kernel8-bsp-study01.img
device_tree=bcm2711-rpi-4-b-bsp-study01.dtb
overlay_prefix=overlays-bsp-study01/

[all]
dtparam=i2c_arm=on
dtparam=spi=on
dtparam=audio=on
camera_auto_detect=1
display_auto_detect=1
auto_initramfs=1
dtoverlay=vc4-kms-v3d
max_framebuffers=2
disable_fw_kms_setup=1
arm_64bit=1
disable_overscan=1
arm_boost=1

[cm4]
otg_mode=1
[cm5]
dtoverlay=dwc2,dr_mode=host
[pi5]
dtoverlay=nospi10
[all]
enable_uart=1
```

재현 환경에서 편집이 필요할 때만 Pi 홈에서 `sudo nano /boot/firmware/config.txt`로 기존 설정을 유지하며 커스텀 선택 블록을 추가한다. overlay_prefix는 기존 dtoverlay보다 앞에 둔다. [all]은 그 아래부터 공통 설정으로 되돌리며 위로 소급하지 않는다. auto_initramfs=1은 커널 이름에 대응하는 initramfs8-bsp-study01을 자동 선택하도록 한다. os_prefix 지시문은 최종 파일에 없다. 설정 파일의 참조와 실제 기능 확인은 구분하며 확인 범위는 검증 현황을 따른다.

**Pi · 작업 디렉터리: 홈, 설정·설치 파일 확인 후 실행**

```bash
cd ~
sudo reboot
```

**WSL · 작업 디렉터리: 저장소 루트, 부팅 후 재접속**

```bash
cd ~/bspworkspace/bsp-study
ssh pi
```

**Pi · 작업 디렉터리: 홈**

```bash
cd ~
uname -r
uname -m
findmnt /
ls -ld /lib/modules/"$(uname -r)"
```

이어서 **기존에 사용하던 화면이 실제로 출력되는지 사용자가 확인**하고, 변경의 영향을 받는 기능도 검사한다. uname만으로 DT·initramfs 선택이나 화면 정상까지 증명하지 않는다. 커널 버전·SSH·루트·모듈과 화면의 관측 결과는 검증 현황에 구분한다.

### 8. 기존 커널로 복구

아래는 화면 매핑 누락의 실제 복구와 구분하는 전체 커널 롤백 계획이다. 기존 `kernel8.img`, `initramfs8`, DTB·overlays와 `/lib/modules/6.18.50+rpt-rpi-v8`를 보존했다.

1. SSH가 안 되면 Pi 전원을 끈 뒤 SD카드를 카드 리더기로 노트북에 연결한다.
2. Windows 탐색기에서 SD카드의 FAT 부트 파티션을 연다. 이 파티션에서는 `config.txt`가 최상위에 있다.
3. WSL의 `~/bspworkspace/bsp-study/out/boot-backup-20261102-222418/firmware/config.txt`를 FAT 파티션의 `config.txt`로 복원한다. WSL 파일은 탐색기의 `\\wsl.localhost\<WSL 배포판 이름>\home\nakta\bspworkspace\bsp-study\out\boot-backup-20261102-222418\firmware`에서 접근할 수 있다. 드라이브 문자·WSL 배포판 등록 이름은 복구할 노트북의 실제 값을 사용한다.
4. 안전하게 분리해 Pi에 다시 넣고 부팅한다. 원래 파일을 보존한 이번 설치에서는 config.txt 복원으로 원래 선택을 되돌리는 계획이다. 다른 부팅 파일까지 변경했다면 해당 백업도 복원해야 한다.

**WSL · 작업 디렉터리: 저장소 루트, 복구 후 접속**

```bash
cd ~/bspworkspace/bsp-study
ssh pi
```

**Pi · 작업 디렉터리: 홈 · 기존 설정 복구 후 부팅 판정용**

```bash
cd ~
uname -r
findmnt /
```

기대값은 기존 커널 `6.18.50+rpt-rpi-v8`과 기존 ext4 루트다. 현재의 실제 실행 커널은 커스텀 버전이며 이 기대값을 복구 성공 관측으로 혼동하지 않는다.

## 검증 현황

| 구분 | 관측·근거 |
| --- | --- |
| 빌드 | 사용자 make 종료 상태 0. ARM64 Image 약 28MiB, 모듈 1,917개, DT 파일·Module.symvers 확인 |
| 설치 | 새 이름의 커널·DTB·overlay, initramfs 약 18MiB 및 새 모듈 정보, 기존·새 모듈 디렉터리 공존 확인 |
| 부팅 | 사용자 uname: `6.18.59-v8-bsp-study01`, aarch64. `/dev/mmcblk0p2` ext4 루트·SSH 재접속 확인 |
| 모듈 동작 | [hello 모듈](../examples/hello_module/kernel-module.md)의 실제 로드·해제·로그 확인 |
| 화면 | 최초 커스텀 부팅 시 미출력. 매핑 파일 복사 후 재부팅하여 **사용자가 실제 정상 화면 확인** |
| 최종 설정·복구 후 버전 | 2026-11-04 사용자 제공 config.txt 전문과 uname으로 6.18.59-v8-bsp-study01 재확인 |

과거 실제 배포는 직접 IP 주소로 연결해 Pi 홈에서 묶음을 풀었고, 위 안내는 현재 별칭 pi·~/bsp/kernel 경로에 맞췄다. 검증 결과 자체를 새 경로로 실행한 결과처럼 바꾸지 않는다.

남은 확인과 근거의 한계:

- dtbs_install을 사용하는 보완 정상 배포 절차의 전체 재실행과 ~/bsp/kernel 경로 재현은 미수행이다. 현재 Pi의 overlay_map.dtb는 아래 장애 복구 때 기존 디렉터리에서 가져온 파일이다.
- 기존 커널 전체 롤백은 계획·백업·복구 수단을 준비했으며 실제 복귀 시험은 하지 않았다.
- 설정 파일과 uname·화면은 확인했지만 런타임에 사용된 DT·initramfs 파일 자체를 별도로 추출해 식별하지 않았다. 센서 등 미시험 기능도 이 결과로 정상 판정하지 않는다.
- 화면 오류는 사용자 제공 로그 발췌와 복구 보고가 근거다. 전체 오류 로그·수집 명령·당시 전체 셸 이력은 직접 확보하지 않았다. 최초 배포 누락은 안내 명령과 남아 있던 스테이징·아카이브·빌드 출력 대조에 근거한다.
- 도구 설치 목록은 새 WSL 환경의 준비 안내다. 실습 당시 주요 패키지는 이미 설치돼 있었고, 이 문서의 apt 명령 전체를 실행한 이력이라고 간주하지 않는다.
- 이번 문서 정리에서는 빌드·설치·보드 재부팅을 실행하지 않았다.

## 실제 트러블슈팅

### 설정은 바뀌었는데 kernelrelease가 이전 값

관측한 문제: scripts/config는 `.config`를 바꿨지만 olddefconfig는 `No change`를 출력했고 kernelrelease는 이전 `6.18.59-v8`이었다. 변경이 실패한 것이 아니라 버전 생성에 쓰이는 `include/config/auto.conf`가 이전 값이었다. `prepare`로 동기화한 후 해결했다. HOSTCC/HOSTLD는 WSL용 보조 도구 생성이며 prepare에서는 일부 ARM64 코드도 빌드한다. 전체 커널 빌드와는 구분한다.

### SSH는 되지만 커스텀 커널 화면이 나오지 않음

**증상·구성:** 커스텀 커널로 부팅하고 SSH는 가능했지만 화면이 없었다. `overlay_prefix=overlays-bsp-study01/`, `dtoverlay=vc4-kms-v3d`를 사용하는 구성이다. 새 디렉터리에 vc4-kms-v3d.dtbo·vc4-kms-v3d-pi4.dtbo는 있었지만 overlay_map.dtb는 없었다는 사용자 확인을 받았다.

**사용자 제공 로그 발췌:**

```text
dterror: can't find symbol 'i2c2'
Failed to resolve overlay 'vc4-kms-v3d'
```

**원인과 소스 근거:** 보드별 overlay 선택에 필요한 매핑 파일이 배포에서 누락됐다. 선택한 커밋의 `arch/arm/boot/dts/overlays/overlay_map.dts`에는 아래 선택 규칙이 있다. Pi 4의 BCM2711에서 일반 이름을 Pi 4용 overlay로 연결하는 정보다. 두 DTBO 파일이 있다는 것만으로 이 선택 정보까지 배포됐다고 볼 수 없다.

```dts
vc4-kms-v3d {
    bcm2835;
    bcm2711 = "vc4-kms-v3d-pi4";
    bcm2712 = "vc4-kms-v3d-pi5";
};
```

**최초 배포 명령을 확인한 결과:** 아래는 당시 안내·기록된 명령 발췌다. 정상 재현용 명령이 아니며 재실행하지 않는다.

```text
WSL / ~/bspworkspace/bsp-study
cp out/kernel-rpi4/arch/arm64/boot/dts/overlays/*.dtbo \
    out/pi4-install/boot/firmware/overlays-bsp-study01/
cp external/linux/arch/arm64/boot/dts/overlays/README \
    out/pi4-install/boot/firmware/overlays-bsp-study01/
```

이 명령은 .dtbo와 README만 포함하고 .dtb인 overlay_map.dtb를 복사하지 않는다. 읽기 전용으로 확인한 로컬 `out/pi4-install/.../overlays-bsp-study01`과 `out/pi4-study01.tar.gz`에도 매핑 파일이 없고 두 KMS DTBO는 있었다. 반면 `out/kernel-rpi4/arch/arm64/boot/dts/overlays/overlay_map.dtb`는 이미 생성돼 있었다. 안내 명령·남아 있는 배포 결과·빌드 출력이 일치하므로 **컴파일 누락이 아니라 배포 파일 선택에서 누락된 것**으로 판단한다. 근거의 범위는 검증 현황에 구분했다.

**이번에 사용한 복구:** 기존 `/boot/firmware/overlays/overlay_map.dtb`를 새 디렉터리에 복사한 뒤 재부팅했다. 다음은 사용자 보고 작업을 재현하는 명령 표기다.

**Pi · 작업 디렉터리: 홈, 당시 장애 복구용 (정상 배포용 아님)**

```bash
cd ~
sudo cp /boot/firmware/overlays/overlay_map.dtb \
    /boot/firmware/overlays-bsp-study01/overlay_map.dtb
sudo reboot
```

**검증:** 사용자가 실제 화면의 정상 출력을 확인했다. 이 해결을 위해 커널을 재빌드하지 않았다. 기존 배포판의 매핑 파일을 빌린 이번 복구와 달리, 앞으로는 5단계의 dtbs_install로 **선택 소스·빌드 결과의 매핑 파일을 함께 배포**한다. 기존 디렉터리 파일 복사는 상시 정상 절차로 사용하지 않는다.

**재발 방지 검토:** 선택 커밋의 `scripts/Makefile.dtbinst`는 빌드의 dtbs-list를 설치 목록으로 사용한다. 실제 목록에 overlay_map.dtb·hat_map.dtb·보드별 KMS DTBO가 있음을 읽기 확인했다. 그 목록에 맞춰 정상 배포 명령과 설치 파일 확인 목록을 함께 고쳤다. 실행 범위는 검증 현황과 구분한다.

## 공식 자료

- [Raspberry Pi 커널 빌드](https://www.raspberrypi.com/documentation/computers/linux_kernel.html)
- [부팅 설정과 auto_initramfs](https://www.raspberrypi.com/documentation/computers/config_txt.html)
- [선택 커밋의 overlay 매핑 소스](https://github.com/raspberrypi/linux/blob/b3495ea401d709af0701a85babcba5e3d788a602/arch/arm/boot/dts/overlays/overlay_map.dts)
- [Debian mkinitramfs](https://manpages.debian.org/trixie/initramfs-tools-core/mkinitramfs.8.en.html)
