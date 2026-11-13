# U-Boot: firmware 다음 단계에서 커스텀 커널 부팅

- [1. 한눈에 보기](#1-한눈에-보기)
- [2. 구조](#2-구조)
- [3. 알아야 할 개념](#3-알아야-할-개념)
- [4. 구현 흐름](#4-구현-흐름)
- [5. 재현](#5-재현)
- [6. 트러블슈팅과 한계](#6-트러블슈팅과-한계)

## 1. 한눈에 보기

- **목적:** 일반 임베디드 SoC처럼 부트로더 단계를 직접 구성해, 부팅 시 커널이 무엇을 어떻게 받는지 계층별로 설명할 수 있게 한다. 12단계 Yocto 부팅 경로의 기반이다.
- **기능:** Pi 4 firmware가 커널 대신 U-Boot(`u-boot.bin`)를 실행하게 하고, U-Boot가 [boot.cmd](../../boot/uboot/boot.cmd)로 만든 `boot.scr`에 따라 기존 커스텀 커널 `6.18.59-v8-bsp-study01`과 initramfs를 올린다. firmware가 `config.txt`의 `dtparam`·`dtoverlay`를 적용해 만든 DT를 U-Boot가 **바꾸지 않고** 커널에 넘긴다.
- **결과:** U-Boot 경유 부팅, `/chosen/u-boot,version` = `2026.10`, cmdline 동일, overlay 노드 전달, 원복을 보드에서 확인했다([6장](#6-트러블슈팅과-한계)).
- **범위:** U-Boot `v2026.10`, `rpi_arm64_defconfig`, 소스 수정 없음, `saveenv` 없음(SD 카드에 환경을 쓰지 않음). 부트 파티션에 `u-boot.bin`·`boot.scr`를 추가하고 `config.txt`의 `kernel=` 한 줄만 바꾼다. 네트워크 부팅·부팅 메뉴·시리얼 콘솔은 하지 않았다.
- **관련 파일:** [boot.cmd](../../boot/uboot/boot.cmd), [Makefile](../../boot/uboot/Makefile), 진행 기록 [T-B01](../tasks/T-B01-uboot-boot.md). U-Boot 소스는 `external/u-boot`(Git 대상 아님)

## 2. 구조

**흐름:** firmware(`config.txt` → DT + overlay) → U-Boot(`bootflow scan` → `boot.scr`) → `booti`(Image + initramfs + 같은 DT) → Linux

```text
 +---------------------+     +----------------------------+     +------------------------+
 | firmware start4.elf |     | U-Boot (u-boot.bin)        |     | Linux 6.18.59          |
 | reads config.txt    |     | bootstd: bootflow scan     |     | /proc/device-tree      |
 | DTB + dtoverlay     |---->|  -> finds boot.scr         |---->|  /chosen/bootargs      |
 | kernel=u-boot.bin   | DT  | fdt addr ${fdt_addr}       |booti|  /chosen/u-boot,version|
 | auto_initramfs: --  | ptr | load Image, initramfs      |     | initramfs -> rootfs    |
 +---------------------+     +----------------------------+     +------------------------+
```

| 단계 | 누가 | 무엇을 읽고 | 다음 단계에 넘기는 것 |
| --- | --- | --- | --- |
| firmware | `start4.elf` | `config.txt`, `bcm2711-rpi-4-b-bsp-study01.dtb`, `overlays-bsp-study01/*.dtbo`, `cmdline.txt` | `u-boot.bin` 실행 + overlay가 적용된 DT의 주소. bootargs는 DT의 `/chosen/bootargs`에 들어 있음 |
| U-Boot | `u-boot.bin` | `boot.scr`, `kernel8-bsp-study01.img`, `initramfs8-bsp-study01` | 커널 Image 실행 + initramfs 주소·크기 + 같은 DT의 주소 |
| 커널 | `Image` | DT, initramfs | initramfs에서 rootfs(`PARTUUID=...-02`)로 전환 |

## 3. 알아야 할 개념

### 왜 U-Boot를 넣는가

Pi는 firmware가 커널까지 직접 올릴 수 있어서 U-Boot가 없어도 부팅된다. 그래도 U-Boot를 넣는 이유는 다음과 같다.

- 대부분의 임베디드 SoC(i.MX, TI, Rockchip 등)에서 커널·DT·initramfs를 고르고 올리는 역할을 U-Boot가 맡는다. 이 단계를 직접 구성해 봐야 "커널이 무엇을 어떻게 받는가"를 계층별로 설명할 수 있다.
- 부트 스크립트로 커널 주소·initramfs·DT·bootargs를 명시하게 되므로, firmware가 알아서 하던 일이 눈에 보인다.
- 12단계 Yocto(meta-raspberrypi)도 U-Boot를 이미지에 넣는 구성을 지원한다. 그 전에 수동으로 동작을 확인해 둔다.

### 왜 DTB 파일을 다시 읽지 않고 firmware DT를 쓰는가

Pi는 firmware가 `config.txt`의 `dtparam`·`dtoverlay`를 적용해 DT를 완성한다. DTB 파일을 다시 읽으면 그 결과가 모두 빠진다. 그래서 firmware DT 주소(`fdt_addr`)를 그대로 넘기고, overlay 노드와 phandle(`interrupt-parent` = `0x07`)이 커널에 남는 것으로 확인했다([5-5](#5-5-firmware-overlay가-커널까지-오는지)).

강의(`VEDA/임베디드리눅스/강의자료/u-boot에서라즈베리파이 커널 부팅.pdf`)는 U-Boot 프롬프트에서 `setenv bootargs ...`로 인자를 직접 쓰고, `fatload ... ${fdt_addr_r} bcm2711-rpi-4-b.dtb`로 **DTB 파일을 다시 읽어** `booti ${kernel_addr_r} - ${fdt_addr_r}`로 부팅한 뒤 `saveenv`로 저장한다.

| 항목 | 강의 | 이 실습 | 이유 |
| --- | --- | --- | --- |
| DT | DTB 파일을 `fdt_addr_r`에 다시 로드 | firmware DT(`fdt_addr`) 그대로 | 파일을 다시 읽으면 `config.txt`의 `dtparam`·`dtoverlay`가 빠진다 |
| bootargs | 직접 입력(`root=/dev/mmcblk0p2`) | firmware 값 복사 | `cmdline.txt`의 `PARTUUID` 등과 어긋나지 않는다 |
| initramfs | 없음(`-`) | 로드해서 전달 | 현재 카드는 initramfs로 부팅한다 |
| 설정 저장 | `saveenv` | 저장소의 `boot.cmd` → `boot.scr` | SD 카드에 쓰지 않고, Git으로 재현할 수 있다 |

### 왜 bootargs를 `/chosen`에서 복사하는가

`booti` 직전에 `fdt_chosen()`이 DT의 `/chosen`을 손본다(`boot/fdt_support.c` L337).
- env `bootargs`가 있으면 `/chosen/bootargs`를 그 값으로 덮어쓰고, 없으면 firmware 값을 그대로 둔다(L332, L376).
- `/chosen/u-boot,version`을 항상 추가한다(L390). 리눅스에서 이 속성이 보이면 U-Boot를 거쳐 부팅한 것이다.

`cmdline.txt`는 firmware가 DT `/chosen/bootargs`에 넣는다. 스크립트에서 이 값을 env `bootargs`로 복사하면 커널이 받는 인자의 출처가 드러나고, 전환 전후 `/proc/cmdline`을 diff해 같음을 확인할 수 있다.

### U-Boot를 거쳤는지 리눅스에서 어떻게 아는가

U-Boot는 `booti` 직전 `fdt_chosen()`에서 `/chosen/u-boot,version`을 항상 넣는다(`boot/fdt_support.c` L390). U-Boot 경유 부팅에서는 이 값이 `2026.10`이었고 원복 후에는 없었다. 화면 출력이 아니라 리눅스 쪽 증거로 판정한다.

### 왜 SD 카드 한 장으로도 안전하게 시험할 수 있었는가

변경을 부트 파티션 파일 2개 추가와 `config.txt` 한 줄로 제한했다. 시작 전에 부트 파티션 전체를 WSL로, `config.txt`를 따로 백업했다. U-Boot 프롬프트에서 쓰기 명령(`saveenv`·`fatwrite`·`mmc write`·`gpt write`)은 쓰지 않았고, 마지막에 백업으로 원복해 기존 부팅을 확인했다.

## 4. 구현 흐름

### 4-1. firmware → U-Boot: DT 주소

firmware는 `kernel=`로 지정한 이미지를 실행하면서 DT 주소를 레지스터로 넘긴다. U-Boot의 rpi 보드 코드는 이 값을 `fw_dtb_pointer`에 저장하고, 부팅 중 `fdt_addr` 환경변수에 넣는다(`external/u-boot/board/raspberrypi/rpi/rpi.c` L40, L399, L572).

### 4-2. U-Boot가 `boot.scr`를 찾는 방법

`rpi_arm64_defconfig`는 `CONFIG_BOOTSTD_DEFAULTS=y`이고 기본 `bootcmd`는 `bootflow scan`이다. `boot_targets=mmc usb pxe dhcp` 순서로 장치를 뒤지고, script bootmeth가 부트 파티션에서 `boot.scr`를 찾으면 `devtype`·`devnum`·`distro_bootpart`를 설정한 뒤 실행한다(`board/raspberrypi/rpi/rpi.env` L80, `boot/bootmeth_script.c` L200). 그래서 스크립트에 `mmc 0:1`을 적지 않아도 된다.

### 4-3. 부트 스크립트

[boot.cmd](../../boot/uboot/boot.cmd)는 다섯 줄이다.

```text
fdt addr ${fdt_addr}
fdt get value bootargs /chosen bootargs
load ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} kernel8-bsp-study01.img
load ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} initramfs8-bsp-study01
booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr}
```

- [L2](../../boot/uboot/boot.cmd#L2) `fdt addr ${fdt_addr}`: firmware가 넘긴 DT를 작업 대상으로 정한다. DTB 파일을 다시 읽지 않는다.
- [L5](../../boot/uboot/boot.cmd#L5) `fdt get value bootargs ...`: firmware가 `cmdline.txt`로 만든 `/chosen/bootargs`를 env `bootargs`에 복사한다. env가 비어 있으면 이 줄이 없어도 결과는 같다. 커널이 받는 인자의 출처를 스크립트에서 드러내려고 넣었다.
- [L7](../../boot/uboot/boot.cmd#L7)·[L8](../../boot/uboot/boot.cmd#L8) `load`: 커널 Image와 initramfs를 메모리로 읽는다. 읽기만 하고 SD 카드에 쓰지 않는다. `load`는 마지막에 읽은 파일 크기를 `filesize`에 남기므로 initramfs를 나중에 읽는다.
- [L11](../../boot/uboot/boot.cmd#L11) `booti`: 비압축 arm64 Image용 부팅 명령이다(`kernel8-bsp-study01.img`는 `file` 결과 `Linux kernel ARM64 boot executable Image`). initrd는 `주소:크기`로 넘긴다. 세 번째 인자로 같은 DT 주소를 넘긴다.

### 4-4. 메모리 배치

`rpi.env` 기본값을 그대로 쓴다(`board/raspberrypi/rpi/rpi.env` L70~78). 실제 파일 크기로 겹치지 않는 것을 계산했다.

| 대상 | 주소 | 크기 | 끝 |
| --- | --- | --- | --- |
| 커널 Image | `kernel_addr_r=0x00080000` | 29,182,464 B, 헤더 `image_size=0x1c60000`(28.4 MiB, `text_offset=0`) | booti가 2MB 경계로 옮겨도 약 `0x1E60000` |
| `boot.scr` | `scriptaddr=0x05400000` | 642 B | — |
| initramfs | `ramdisk_addr_r=0x05700000` | 18,544,173 B | 약 `0x068B0000` |
| 상한 | `bootm_size=0x20000000` | 512 MiB | 모든 이미지가 이 안에 있음 |

firmware DT는 firmware가 정한 위치(RAM 상단 쪽)에 있고 `fdt_addr`로만 참조한다.

커널 헤더 flags의 bit 3이 켜져 있어(`0xa`) `booti`는 커널을 2MB 경계(`0x200000`)로 옮긴다(`arch/arm/lib/image.c`). 끝 주소가 `scriptaddr`·initramfs 영역보다 아래라 겹치지 않는다.

## 5. 재현

### 5-0. 준비

| 항목 | 값 |
| --- | --- |
| U-Boot 소스 | `~/bspworkspace/bsp-study/external/u-boot`, 태그 `v2026.10`(커밋 `5508406`). Git 대상 아님 |
| 빌드 출력 | `out/uboot-rpi4/` (`u-boot.bin`, `boot.scr`, `tools/mkimage`) |
| WSL 패키지 | `aarch64-linux-gnu-gcc`, `bison`, `flex`, `libssl-dev`, `libgnutls28-dev`, `bc`, `device-tree-compiler` |
| 보드 | Pi 4, 커스텀 커널 부팅 파일(`kernel8-bsp-study01.img`, `initramfs8-bsp-study01`, DTB, `overlays-bsp-study01/`)이 있는 카드 ([커널 구축](07-kernel-build.md)) |
| 화면 | HDMI 모니터. USB 키보드는 U-Boot 프롬프트 입력용(부팅 시 `usb start` 한 번만 하므로 미리 꽂아 둠) |
| 복구 | PC SD 리더 |

```bash
# [WSL] ~/bspworkspace/bsp-study — 소스 받기 (한 번만)
git clone --depth 1 --branch v2026.10 https://source.denx.de/u-boot/u-boot.git external/u-boot
```

### 5-1. 빌드

**목적:** `u-boot.bin`과 `boot.scr`를 만든다.

```bash
# [WSL] ~/bspworkspace/bsp-study/boot/uboot
make
ls -l ../../out/uboot-rpi4/u-boot.bin ../../out/uboot-rpi4/boot.scr
../../out/uboot-rpi4/tools/dumpimage -l ../../out/uboot-rpi4/boot.scr
```

- [Makefile#L13](../../boot/uboot/Makefile#L13): `.config`가 없을 때만 `rpi_arm64_defconfig`를 적용한다. `O=`로 빌드해 소스 폴더에 산출물을 만들지 않는다.
- [Makefile#L16](../../boot/uboot/Makefile#L16): U-Boot 본체 빌드. 이 과정에서 `tools/mkimage`도 만들어진다.
- [Makefile#L20](../../boot/uboot/Makefile#L20): `mkimage -T script`로 `boot.cmd`에 헤더를 붙여 `boot.scr`를 만든다. mkimage가 먼저 있어야 하므로 `uboot`를 order-only 의존성으로 둔다.

**확인 포인트:** 종료 코드 0, `u-boot.bin` 약 944 KB, `dumpimage` 결과 `AArch64 Linux Script (uncompressed)`.

### 5-2. 백업

**목적:** 부팅이 안 될 때 되돌릴 기준을 만든다.

```bash
# [WSL] ~/bspworkspace/bsp-study — 부트 파티션 전체 (FAT는 일반 사용자도 읽을 수 있어 sudo 없이)
mkdir -p out/boot-backup-tb01
ssh pi 'tar -C /boot/firmware -cf - .' | tar -C out/boot-backup-tb01 -xf -
ssh pi 'find /boot/firmware -type f | wc -l'
find out/boot-backup-tb01 -type f | wc -l         # 위와 같아야 함
```

```bash
# [Pi] ~
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-tb01
cat /proc/cmdline > ~/bsp/cmdline-before-tb01.txt
```

`ssh pi 'sudo ...'`는 비밀번호를 입력할 터미널이 없어 실패한다. 부트 파티션 tar는 sudo 없이 받는다.

### 5-3. 배포·전환

```bash
# [WSL] ~/bspworkspace/bsp-study
scp out/uboot-rpi4/u-boot.bin out/uboot-rpi4/boot.scr pi:bsp/
```

```bash
# [Pi] ~
sudo cp ~/bsp/u-boot.bin ~/bsp/boot.scr /boot/firmware/
sudo sed -i 's/^kernel=kernel8-bsp-study01.img$/kernel=u-boot.bin/' /boot/firmware/config.txt
diff /boot/firmware/config.txt.bak-tb01 /boot/firmware/config.txt   # kernel= 한 줄만
sudo reboot
```

`[pi4]` 블록의 `device_tree=`·`overlay_prefix=`는 그대로 둔다. firmware가 이 값으로 DT를 만들어 U-Boot에 넘기기 때문이다.

**HDMI에서 보이는 것:** firmware 단계의 initramfs 관련 메시지([6-1](#6-1-firmware-단계의-no-match-initramfs-메시지)) → U-Boot의 `Hit any key to stop autoboot`(키를 누르지 않음) → 리눅스 부팅 → 데스크톱.

### 5-4. U-Boot 경유 확인

```bash
# [Pi] ~
uname -r                                                    # 6.18.59-v8-bsp-study01
tr -d '\0' < /proc/device-tree/chosen/u-boot,version; echo  # 2026.10
diff ~/bsp/cmdline-before-tb01.txt /proc/cmdline && echo same
```

- `u-boot,version`은 U-Boot의 `fdt_chosen()`만 추가하므로 U-Boot를 거쳤다는 증거다.
- cmdline이 같으면 firmware가 만든 bootargs가 그대로 전달된 것이다.

### 5-5. firmware overlay가 커널까지 오는지

**목적:** firmware가 적용한 overlay가 U-Boot를 거쳐도 남는지 확인한다. 기존 rpi `i2c-sensor` overlay를 확인용으로 쓴다.

```bash
# [Pi] ~
echo 'dtoverlay=i2c-sensor,mpu6050,int_pin=17' | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] ~ — 재부팅 후
tr -d '\0' < /proc/device-tree/chosen/u-boot,version; echo
n=/proc/device-tree/soc/i2c@7e804000/mpu6050@68
ls "$n"
od -An -tx1 "$n/interrupt-parent"                          # 00 00 00 07 (gpio phandle)
readlink -e /sys/bus/i2c/devices/1-0068/driver             # .../inv-mpu6050-i2c
```

**확인 포인트 (실측):** `u-boot,version` = `2026.10`, `mpu6050@68` 노드와 `interrupt-parent` = `00 00 00 07`, `inv-mpu6050-i2c` 바인딩. overlay 노드와 phandle 해결 결과가 그대로 커널에 왔다. 강의처럼 DTB 파일을 다시 읽었다면 이 노드는 없다.

같은 0x68 센서에 mainline 드라이버가 붙으므로, 확인 후 5-6으로 이 줄을 없앤다.

### 5-6. 원복

```bash
# [Pi] ~
sudo cp /boot/firmware/config.txt.bak-tb01 /boot/firmware/config.txt
grep -n '^kernel=' /boot/firmware/config.txt               # kernel=kernel8-bsp-study01.img
sudo reboot
# 재부팅 후
uname -r
ls /proc/device-tree/chosen/ | grep u-boot                 # 출력 없음
diff ~/bsp/cmdline-before-tb01.txt /proc/cmdline && echo same
```

`u-boot.bin`·`boot.scr`는 부트 파티션에 남아 있어도 `config.txt`가 가리키지 않으면 쓰이지 않는다. 다시 U-Boot로 부팅하려면 5-3의 `sed` 한 줄만 다시 실행한다.

**부팅이 안 될 때:** 전원을 분리하고 SD 카드를 PC 리더에 꽂는다. Windows에서 `bootfs`의 `config.txt.bak-tb01`을 `config.txt`로 덮어쓴 뒤 다시 부팅한다. 부트 파티션이 손상되면 5-2의 WSL 백업으로 복원한다. U-Boot 프롬프트에서는 `saveenv`·`fatwrite`·`mmc write`·`gpt write`를 쓰지 않는다.

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| `kernel=u-boot.bin` 전환 후 부팅 초기 "no match initramfs" 메시지 | `auto_initramfs=1`이 `u-boot.bin`과 짝이 되는 initramfs 이름을 찾지 못함 | 조치 없음. U-Boot가 initramfs를 직접 올림([6-1](#6-1-firmware-단계의-no-match-initramfs-메시지)) |
| 부팅이 멈춤 | (이번에는 발생하지 않음) | PC SD 리더로 `config.txt.bak-tb01`을 `config.txt`로 덮어씀([5-6](#5-6-원복)) |

### 6-1. firmware 단계의 "no match initramfs" 메시지

**증상:** `kernel=u-boot.bin`으로 바꾼 뒤 부팅 초기 HDMI에 initramfs 짝을 찾지 못했다는 취지의 메시지가 나왔다. 이후 U-Boot와 리눅스는 정상 부팅했다.

**원인:** `config.txt`의 `auto_initramfs=1`은 `kernel=` 파일 이름에 대응하는 initramfs를 firmware가 찾아 함께 올리게 한다. `kernel8-bsp-study01.img`일 때는 `initramfs8-bsp-study01`이 짝이었지만, `u-boot.bin`에는 짝이 되는 이름이 없다.

**조치:** 없음. 이 구성에서는 U-Boot가 [boot.cmd#L8](../../boot/uboot/boot.cmd#L8)로 initramfs를 직접 올린다. 원복하면 firmware가 다시 짝을 찾는다.

### 6-2. 실측과 미검증

- **실측(사용자 실행):** U-Boot 경유 부팅, `u-boot,version` = `2026.10`, cmdline 동일, firmware overlay 노드·`interrupt-parent` 전달과 드라이버 바인딩, 원복 후 직접 부팅. HDMI에서 `Hit any key to stop autoboot`를 확인해 현재 화면 설정(`vc4-kms-v3d`, `disable_fw_kms_setup=1`)에서도 U-Boot 화면 출력이 됨을 봤다.
- **간접 근거:** `boot.scr` 실행은 화면이 아니라, firmware가 initramfs를 올리지 않았는데 initramfs로 부팅됐다는 점으로 판단했다.
- **미검증:** PC SD 리더로 `config.txt`를 되돌리는 복구(이번에는 Pi 안에서 원복), U-Boot 프롬프트에서 USB 키보드 입력, 시리얼 콘솔, U-Boot 경유 상태에서 장시간 운용과 IIO 버퍼 수집.
- **한계:** 커널·initramfs 파일 이름이 `boot.cmd`에 고정되어 있다. 커널 이름을 바꾸면 스크립트도 바꿔야 한다. 부팅 실패 시 화면만으로는 로그를 남기기 어렵다. 반복 시험이 많아지면 3.3V USB-UART 시리얼 콘솔이 낫다.
- **VEDA 강의와 이 실습의 범위:** 강의에서는 U-Boot 빌드와 프롬프트의 `setenv`·`fatload`·`booti`·`saveenv`로 기본 커널을 부팅했다. 이 실습은 스크립트(`boot.scr`) 방식, firmware DT·bootargs 유지, initramfs 로드, U-Boot 경유·DT 전달·원복을 리눅스 쪽 증거로 검증한 것이다.
