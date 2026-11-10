# T-B01 U-Boot 경유로 커스텀 커널 부팅

상태: 완료
관련 roadmap 단계: 11 (U-Boot 부팅)
유형: C 부팅·빌드 체계
트랙: 부팅(boot)

## 목표 (Claude)
Pi firmware → U-Boot → 커스텀 커널 6.18.59-v8-bsp-study01(initramfs 포함) 순서로 부팅함. firmware가 `config.txt`의 overlay까지 적용한 DT를 U-Boot가 그대로 커널에 넘기는 것을 `/proc/device-tree`로 확인함. 끝나면 부팅 계층별로 누가 무엇을 로드하고 무엇을 넘기는지 설명할 수 있음. 12단계 Yocto 부팅 경로의 기반.

## 배경 개념 (Claude)
1. **firmware가 DT를 만들어 넘김:** Pi 4 firmware는 `config.txt`를 처리해 `device_tree=` DTB에 `dtparam`·`dtoverlay`를 적용하고, `kernel=`로 지정한 이미지(여기서는 `u-boot.bin`)에 DT 주소를 넘김. U-Boot는 이 주소를 `fw_dtb_pointer`로 받아 `fdt_addr` 환경변수에 넣음. 근거: `external/u-boot/board/raspberrypi/rpi/rpi.c` L40, L399, L572.
2. **bootstd가 `boot.scr`를 찾아 실행:** `rpi_arm64_defconfig`는 `CONFIG_BOOTSTD_DEFAULTS=y`이고 `boot_targets=mmc usb pxe dhcp`임. script bootmeth가 부트 파티션의 `boot.scr`를 찾으면 `devtype`·`devnum`·`distro_bootpart`를 설정한 뒤 실행함. 근거: `configs/rpi_arm64_defconfig` L15, `board/raspberrypi/rpi/rpi.env` L80, `boot/bootmeth_script.c` L200~227.
3. **bootargs와 `/chosen`:** `booti` 직전 `fdt_chosen()`은 env `bootargs`가 있으면 `/chosen/bootargs`를 덮어쓰고, 없으면 firmware 값을 유지함. 항상 `/chosen/u-boot,version`을 추가함. 이 속성이 리눅스에서 U-Boot를 거쳤다는 증거가 됨. 근거: `boot/fdt_support.c` L332, L376~390.

## 확인한 사실 (Claude 읽기 확인, 보드 미확인)
| 항목 | 값 |
| --- | --- |
| U-Boot 소스 | `external/u-boot`, 태그 `v2026.10`, 커밋 `5508406582f6f7120dce0c4b3059819a41788db2` (Git 대상 아님) |
| defconfig | `rpi_arm64_defconfig` (VEDA 강의와 같음). `CONFIG_VIDEO_BCM2835=y`, `CONFIG_USB_KEYBOARD=y` |
| 메모리 배치 (`rpi.env`) | `kernel_addr_r=0x00080000`, `scriptaddr=0x05400000`, `fdt_addr_r=0x05600000`, `ramdisk_addr_r=0x05700000`, `bootm_size=0x20000000` |
| 커널 Image | `kernel8-bsp-study01.img` 29,182,464 B, 헤더 `text_offset=0`, `image_size=0x1c60000`(28.4 MiB). booti가 2MB 경계로 재배치해도 `scriptaddr` 아래에 들어감 |
| initramfs | `initramfs8-bsp-study01` 18,544,173 B → `0x05700000`~약 `0x068B0000`, `bootm_size` 안 |
| 현재 cmdline.txt | `console=serial0,115200 console=tty1 root=PARTUUID=2f906600-02 rootfstype=ext4 fsck.repair=yes rootwait quiet splash ...` (2026-11-10 SSH 읽기) |
| 빌드 의존 | `libgnutls28-dev` 설치 확인 |

## 범위 (Claude)
- 변경 파일: 없음
- 새로 만드는 파일: `boot/uboot/Makefile`, `boot/uboot/boot.cmd`
- 보드에서 바뀌는 것: `/boot/firmware/`에 `u-boot.bin`·`boot.scr`·`config.txt.bak-tb01` 추가, `config.txt`의 `[pi4]` 아래 `kernel=` 한 줄 변경(사용자 작업)
- 하지 않는 것: U-Boot 소스 수정, `saveenv`, 네트워크 부팅, 부팅 메뉴, 커널 재빌드, rootfs 변경, Yocto, 시리얼 콘솔
- 트랙 예외: SD 카드가 한 장(64GB)뿐이라 부팅 트랙 전용 카드 없이 기존 카드를 씀. 이 TASK 동안 센서 트랙 작업 없음

## 백업·복구 (Claude, 진행 전 확보)
| 단계 | 방법 | 상태 |
| --- | --- | --- |
| 백업 1 | WSL `out/boot-backup-tb01/`에 `/boot/firmware` 전체 tar 복사 | 완료 (Pi·WSL 파일 827개 일치, 사용자 실행) |
| 백업 2 | Pi `config.txt.bak-tb01`, 비교 기준 `~/bsp/cmdline-before-tb01.txt` | 완료 (사용자 실행) |
| 복구 (부팅 실패) | 전원 분리 → SD를 PC 리더에 연결 → Windows에서 bootfs의 `config.txt.bak-tb01`을 `config.txt`로 덮어쓰기 → 재부팅 | 절차 준비. 같은 백업 파일로 Pi 안에서 원복은 성공(7단계), PC 리더 경로는 미실행 |
| 최후 수단 | 부트 파티션 손상 시 백업 1의 파일을 리더로 복원 | 절차 준비 |

주의: U-Boot 프롬프트에서 `saveenv`·`fatwrite`·`mmc write`·`gpt write`를 입력하지 않음. 부팅이 멈추면 위 복구 절차를 씀.

## 완료 기준 (Claude)
- [x] 백업 1·2 완료
- [x] HDMI에 U-Boot 출력과 `boot.scr` 실행이 보이고, SSH로 `uname -r` = `6.18.59-v8-bsp-study01`
- [x] `/proc/device-tree/chosen/u-boot,version`이 `2026.10`으로 존재(U-Boot 경유 증거)
- [x] `/proc/cmdline`이 U-Boot를 쓰지 않을 때(`cmdline-before-tb01.txt`)와 같음
- [x] `dtoverlay=i2c-sensor,mpu6050,int_pin=17` 추가 후 U-Boot 경유 부팅에서도 `mpu6050@68` 노드와 `interrupt-parent` = `00 00 00 07`이 있음
- [x] `config.txt` 원복 후 U-Boot 없이 부팅되고 `u-boot,version`이 없음

## 검증 절차 (Claude)
선행: 백업 1 완료(위 표), U-Boot 빌드 완료(구현 보고). 출력은 각 단계의 **[붙여 주세요]**만 채팅에 붙이면 '실측 결과'에 기록함.

### 0. 준비
- HDMI 모니터와 USB 키보드를 Pi에 연결
- PC SD 리더를 가까이 둠(부팅 실패 시 복구용)

### 1. 파일 전송 (WSL)

```bash
# [WSL] ~/bspworkspace/bsp-study
scp out/uboot-rpi4/u-boot.bin out/uboot-rpi4/boot.scr pi:bsp/
```

### 2. 백업 2와 사전 확인 (Pi)

```bash
# [Pi] ssh pi
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-tb01
cat /proc/cmdline > ~/bsp/cmdline-before-tb01.txt
ls /boot/firmware/u-boot.bin /boot/firmware/boot.scr      # 둘 다 No such file
ls /proc/device-tree/chosen/ | grep u-boot                 # 출력 없음
ls -l /boot/firmware/config.txt.bak-tb01                   # 백업 생성 확인
```

### 3. 배포와 전환 (Pi)

```bash
# [Pi]
sudo cp ~/bsp/u-boot.bin ~/bsp/boot.scr /boot/firmware/
sudo sed -i 's/^kernel=kernel8-bsp-study01.img$/kernel=u-boot.bin/' /boot/firmware/config.txt
grep -n '^kernel=' /boot/firmware/config.txt               # kernel=u-boot.bin 한 줄
```

**[붙여 주세요]** `grep` 결과. `kernel=u-boot.bin`이 한 줄 나오면 재부팅함.

```bash
# [Pi]
sudo reboot
```

### 4. HDMI 화면 관찰
- `U-Boot 2026.10` 배너
- `Hit any key to stop autoboot: 2` → **키를 누르지 않고 기다림**
- `boot.scr` 실행, 커널·initramfs 로드 표시
- `Starting kernel ...` → 리눅스 부팅

사진을 찍어 두면 좋음. U-Boot 화면이 전혀 안 나와도 1~2분 기다린 뒤 5단계 SSH 접속을 시도함.

### 5. U-Boot 경유 부팅 확인 (Pi, SSH 재접속)

```bash
# [Pi]
uname -r                                                   # 6.18.59-v8-bsp-study01
tr -d '\0' < /proc/device-tree/chosen/u-boot,version; echo # 2026.10
diff ~/bsp/cmdline-before-tb01.txt /proc/cmdline && echo same
```

**[붙여 주세요]** 출력, 그리고 4단계에서 HDMI에 보인 내용(배너 여부, 어디까지 보였는지).

### 6. overlay 전달 확인 (Pi)

```bash
# [Pi]
echo 'dtoverlay=i2c-sensor,mpu6050,int_pin=17' | sudo tee -a /boot/firmware/config.txt
tail -2 /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후
tr -d '\0' < /proc/device-tree/chosen/u-boot,version; echo
n=/proc/device-tree/soc/i2c@7e804000/mpu6050@68
ls "$n"
od -An -tx1 "$n/interrupt-parent"                          # 00 00 00 07
readlink -e /sys/bus/i2c/devices/1-0068/driver             # .../inv-mpu6050-i2c
```

**[붙여 주세요]**

### 7. 원복 (Pi)

```bash
# [Pi]
sudo cp /boot/firmware/config.txt.bak-tb01 /boot/firmware/config.txt
grep -n '^kernel=' /boot/firmware/config.txt               # kernel=kernel8-bsp-study01.img
sudo reboot
```

```bash
# [Pi] 재부팅 후
uname -r
ls /proc/device-tree/chosen/ | grep u-boot                 # 출력 없음
diff ~/bsp/cmdline-before-tb01.txt /proc/cmdline && echo same
```

**[붙여 주세요]**

### 부팅이 안 될 때 (3~6단계 중)
1. Pi 전원 분리
2. SD 카드를 PC 리더에 꽂고 Windows에서 `bootfs` 드라이브 열기
3. `config.txt`를 지우고 `config.txt.bak-tb01`을 복사해 이름을 `config.txt`로 변경
4. 카드를 다시 꽂고 부팅
5. HDMI에서 멈춘 화면(마지막 몇 줄)을 사진이나 글로 전달

주의: U-Boot 프롬프트(`U-Boot>`)가 떠도 `saveenv`·`fatwrite`·`mmc write`·`gpt write`는 입력하지 않음. 정보 확인은 `printenv`처럼 읽기만 하는 명령으로 충분함.

### 빌드 재현 (WSL, 이미 완료)

```bash
# [WSL] ~/bspworkspace/bsp-study/boot/uboot
make
ls -l ../../out/uboot-rpi4/u-boot.bin ../../out/uboot-rpi4/boot.scr
```

## 구현 보고 (Claude)
- 변경 요약:
  - `boot/uboot/Makefile`: `external/u-boot`를 `out/uboot-rpi4`에 out-of-tree로 빌드(`rpi_arm64_defconfig`). U-Boot가 만든 `tools/mkimage`로 `boot.cmd` → `boot.scr` 생성. `make clean`은 `out/uboot-rpi4` 삭제
  - `boot/uboot/boot.cmd`: firmware DT(`${fdt_addr}`) 선택 → `/chosen/bootargs`를 env `bootargs`로 복사 → bootstd가 설정한 `${devtype} ${devnum}:${distro_bootpart}`에서 커널·initramfs 로드 → `booti`
  - U-Boot 소스는 수정하지 않음. 산출물·소스 모두 Git 제외 경로(`/out/`, `/external/`)
- 빌드 명령·결과:

```bash
# [WSL] ~/bspworkspace/bsp-study/boot/uboot
make
```

  - 종료 코드 0, 재실행 출력에 warning·error 없음
  - `u-boot.bin` 944,488 B, 문자열 `U-Boot 2026.10 (Nov 10 2026 - 15:48:59 +0900)`
  - `boot.scr` 642 B, `dumpimage -l` 결과 `AArch64 Linux Script (uncompressed)`
  - `.config`: `CONFIG_BOOTSTD_DEFAULTS=y`, `CONFIG_BOOTDELAY=2`, `CONFIG_CMD_BOOTI=y`, `CONFIG_CMD_FDT=y`, `CONFIG_CMD_FS_GENERIC=y`, `CONFIG_VIDEO_BCM2835=y`
- checkpatch 결과: 해당 없음(C 소스 없음)
- 판단이 필요했던 부분:
  - DT를 강의처럼 파일(`fdt_addr_r`)로 다시 읽지 않고 firmware가 넘긴 `${fdt_addr}`를 씀. 파일을 다시 읽으면 `config.txt`의 overlay가 빠지기 때문
  - `fdt get value bootargs`는 env `bootargs`가 비어 있으면 없어도 결과가 같음(`fdt_chosen()`이 firmware 값을 유지). 그래도 커널이 받는 인자를 스크립트에서 명시적으로 드러내려고 넣음
  - 부트 파티션 위치는 `mmc 0:1`로 고정하지 않고 bootstd가 설정한 변수를 씀. `boot.scr`를 찾은 파티션에서 그대로 읽음
  - 메모리 배치는 `rpi.env` 기본값을 그대로 씀. 실제 파일 크기로 겹침 없음을 계산함(위 '확인한 사실')
  - 미확인: `vc4-kms-v3d`·`disable_fw_kms_setup=1` 설정에서 U-Boot HDMI 출력이 나오는지
- 사용자가 보드에서 실행할 명령: 위 '검증 절차' 0~7단계

## 리뷰 (사용자)
별도 코드 리뷰 없이 실측으로 대체함. 변경이 스크립트 5줄과 Makefile뿐이고 완료 기준 6개와 원복을 실측으로 확인함.

- [x] 범위 준수: 새 파일은 `boot/uboot/` 아래뿐, U-Boot 소스 수정 없음, 보드 변경은 파일 2개 추가와 `kernel=` 한 줄
- [x] 복구 경로: 백업 1·2 확보, 백업 `config.txt`로 원복 성공(7단계)
- [x] 과잉 구현 없음: `saveenv`·메뉴·네트워크 부팅 없음
- 남은 지적 사항(동작 영향 없음): `boot.cmd`에 커널·initramfs 파일 이름이 고정됨, Makefile `uboot` 타깃은 phony라 `make`마다 U-Boot 재빌드 확인을 거침
- 수정 요청: 없음

## 실측 결과 (사용자 제공, Claude 기록)
사용자가 준 보드 출력·로그의 원문 일부(대표 로그)와 요약을 적는다. 해석은 마감에서 한다.

### 2~3단계: 백업 2·배포·전환
- 요약: 전송 파일 2개가 `~/bsp/`에 있음. 배포 전 `/boot/firmware/`에 `u-boot.bin`·`boot.scr` 없음, `/chosen`에 `u-boot` 속성 없음. `config.txt.bak-tb01` 생성. 배포 후 `config.txt` 7번 줄이 `kernel=u-boot.bin`.

```text
apps  boot.scr  logs  modules  overlays  u-boot.bin
ls: cannot access '/boot/firmware/u-boot.bin': No such file or directory
ls: cannot access '/boot/firmware/boot.scr': No such file or directory
-rwxr-xr-x 1 root root 1435 Nov 10 17:56 /boot/firmware/config.txt.bak-tb01
7:kernel=u-boot.bin
```

- 재부팅 전 확인: `diff` 결과 7번 줄 `kernel=` 한 줄만 바뀜. 배포 파일 크기가 빌드 결과와 같음(`boot.scr` 642, `u-boot.bin` 944488). `[pi4]` 블록의 `device_tree`·`overlay_prefix` 유지.

```text
7c7
< kernel=kernel8-bsp-study01.img
---
> kernel=u-boot.bin
-rwxr-xr-x 1 root root    642 Nov 10 17:57 /boot/firmware/boot.scr
-rwxr-xr-x 1 root root 944488 Nov 10 17:57 /boot/firmware/u-boot.bin
[pi4]
kernel=u-boot.bin
device_tree=bcm2711-rpi-4-b-bsp-study01.dtb
overlay_prefix=overlays-bsp-study01/
```

### 4~5단계: U-Boot 경유 부팅
- 요약: 재부팅 후 HDMI에 "no match initramfs" 비슷한 메시지가 나온 뒤 데스크톱까지 표시됨(사용자 화면 확인, 원문 미기록). U-Boot 단계에서 `Hit any key to stop autoboot` 문구를 사용자가 HDMI로 봄(배너·나머지 줄은 자세히 보지 못함). SSH에서 커널 `6.18.59-v8-bsp-study01`, `/chosen/u-boot,version` = `2026.10`, `/proc/cmdline`이 전환 전과 같음.

```text
6.18.59-v8-bsp-study01
2026.10
same
```

### 6단계: firmware overlay 전달
- 요약: `dtoverlay=i2c-sensor,mpu6050,int_pin=17` 추가 후 재부팅. `u-boot,version` = `2026.10`(U-Boot 경유). `mpu6050@68` 노드 존재, `interrupt-parent` = `00 00 00 07`, `1-0068`이 `inv-mpu6050-i2c`에 바인딩. U-Boot 배너 표시 여부는 전달받지 않음.

```text
2026.10
compatible  interrupt-parent  interrupts  name  phandle  pinctrl-0  pinctrl-names  reg
 00 00 00 07
/sys/bus/i2c/drivers/inv-mpu6050-i2c
```

### 7단계: 원복
- 요약: `config.txt.bak-tb01`로 원복 후 재부팅. 커널 `6.18.59-v8-bsp-study01`, `/chosen`에 `u-boot` 속성 없음(U-Boot 미경유), `/proc/cmdline`이 전환 전과 같음. 원복 직후 `grep` 출력과 HDMI 화면 내용은 전달받지 않음.

```text
6.18.59-v8-bsp-study01
same
```

## 마감 (Claude)
완료 기준 6개 모두 충족. 기준 2의 `boot.scr` 실행은 화면이 아니라 간접 근거로 판정함.

- 완료 기준 판정:
  1. 충족: 백업 1(Pi·WSL 파일 827개 일치), 백업 2(`config.txt.bak-tb01` 생성)
  2. 충족: `Hit any key to stop autoboot`를 HDMI로 봄 → 현재 화면 설정(`vc4-kms-v3d`, `disable_fw_kms_setup=1`)에서도 U-Boot HDMI 출력 동작. `uname -r` = `6.18.59-v8-bsp-study01`. `boot.scr` 실행 메시지는 화면에서 확인하지 못함. firmware 단계에서 initramfs 짝이 없다는 메시지가 나왔는데도 같은 cmdline으로 부팅되고 데스크톱까지 올라왔으므로, initramfs는 스크립트가 로드한 것으로 판단(간접 근거)
  3. 충족: `/chosen/u-boot,version` = `2026.10` (5·6단계)
  4. 충족: `/proc/cmdline`이 전환 전과 같음 → `fdt get value bootargs`로 firmware bootargs가 그대로 전달됨
  5. 충족: firmware가 적용한 `i2c-sensor` overlay 노드 `mpu6050@68`, `interrupt-parent` = `00 00 00 07`, `inv-mpu6050-i2c` 바인딩 → U-Boot가 firmware DT를 바꾸지 않고 전달
  6. 충족: 원복 후 `u-boot` 속성 없음, cmdline 동일
- 마감 해석:
  - "no match initramfs"(원문 미기록)는 `auto_initramfs=1`이 `kernel=u-boot.bin`과 짝이 되는 initramfs 이름을 찾지 못해 나온 firmware 메시지로 봄. 이 구성에서는 U-Boot가 initramfs를 로드하므로 정상
  - 강의 방식(DTB 파일을 `fdt_addr_r`로 다시 로드)과 달리 `${fdt_addr}`를 써서 overlay가 유지됨을 5번 기준으로 확인
- 갱신한 문서:
  - `boot/uboot/uboot-boot.md` 신설: 부팅 단계별 역할, firmware DT 전달 구조, `boot.cmd` 해설, 메모리 배치, 재현 절차, 확인 포인트, 미검증
  - 이 TASK 파일: 범위·백업·완료 기준·리뷰·마감 갱신
  - `docs/interview/uboot-boot.md` 신설: 면접 요약
  - `.claude/rules/docs.md`: TASK 작성자 표시 유지, 역할 예외 문구 금지
- 미검증 항목:
  - PC SD 리더(Windows)로 `config.txt`를 되돌리는 복구 경로는 실제로 해 보지 않음
  - U-Boot 배너 줄, `boot.scr` 실행 메시지, firmware "no match initramfs" 메시지의 원문
  - U-Boot 프롬프트에서 USB 키보드 입력
  - U-Boot 경유 상태에서 장시간 운용, 센서 IIO 버퍼 수집
- 병합 시 반영 (트랙 TASK만, worklog·roadmap·README):
  - worklog: 현재 단계를 U-Boot 경유 부팅 확인으로, 다음 작업을 roadmap 12단계 Yocto 최소 이미지로
  - roadmap 11단계: 완료 표시, [U-Boot 학습 문서](../../boot/uboot/uboot-boot.md) 링크
  - bringup.md SD 카드 표: 카드 한 장(64GB)을 두 트랙이 공용, `/boot/firmware`에 `u-boot.bin`·`boot.scr`·`config.txt.bak-tb01`이 남아 있으나 현재 `config.txt`는 기존 직접 부팅
  - `.claude/rules/docs.md` 학습 문서 목록에 `boot/uboot/uboot-boot.md` 추가
  - README 실습 문서 링크에 U-Boot 문서 추가
  - `docs/interview/README.md`에 `uboot-boot.md` 링크 추가
- 추천 커밋 메시지: `Feat: U-Boot 경유 커스텀 커널 부팅과 firmware DT 전달 확인`
