# U-Boot: firmware 다음 단계에서 커스텀 커널 부팅

## 한 줄 요약
Pi 4의 firmware와 커널 사이에 U-Boot를 넣어 커스텀 커널(initramfs 포함)을 부팅함. firmware가 `config.txt`의 overlay까지 적용한 DT가 U-Boot를 거쳐 그대로 커널에 도착하는 것을 확인함. 목적은 일반 SoC BSP의 부트로더 단계 경험과 Yocto 이미지 부팅 경로 준비.

**관련 파일**
- [T-B01 TASK](../tasks/T-B01-uboot-boot.md): 계획·실측 로그·마감
- [U-Boot 학습 문서](../../boot/uboot/uboot-boot.md): 구조·스크립트 해설·재현 절차
- [boot.cmd](../../boot/uboot/boot.cmd), [Makefile](../../boot/uboot/Makefile)
- U-Boot 소스: `external/u-boot` (`v2026.10`, Git 대상 아님). `board/raspberrypi/rpi/rpi.c`, `boot/fdt_support.c`, `boot/bootmeth_script.c`

## 구조

```text
 +---------------------+     +----------------------------+     +------------------------+
 | firmware start4.elf |     | U-Boot (u-boot.bin)        |     | Linux 6.18.59          |
 | reads config.txt    |     | bootstd: bootflow scan     |     | /proc/device-tree      |
 | DTB + dtoverlay     |---->|  -> finds boot.scr         |---->|  /chosen/bootargs      |
 | kernel=u-boot.bin   | DT  | fdt addr ${fdt_addr}       |booti|  /chosen/u-boot,version|
 | auto_initramfs: --  | ptr | load Image, initramfs      |     | initramfs -> rootfs    |
 +---------------------+     +----------------------------+     +------------------------+
```

| 단계 | 읽는 것 | 넘기는 것 |
| --- | --- | --- |
| firmware | `config.txt`, DTB, overlay, `cmdline.txt` | `u-boot.bin` 실행 + overlay 적용 DT 주소(bootargs는 `/chosen`에) |
| U-Boot | `boot.scr`, 커널 Image, initramfs | 커널 실행 + initramfs 주소·크기 + 같은 DT |
| 커널 | DT, initramfs | rootfs 전환 |

## 핵심 명령과 선택 이유

| 명령 | 하는 일 | 선택 이유·대안 |
| --- | --- | --- |
| `fdt addr ${fdt_addr}` | firmware가 넘긴 DT 사용 | 강의는 DTB 파일을 `fdt_addr_r`로 다시 읽음 → `dtparam`·`dtoverlay`가 빠짐 |
| `fdt get value bootargs /chosen bootargs` | firmware bootargs를 env로 복사 | 강의는 `root=/dev/mmcblk0p2`를 직접 입력 → `cmdline.txt`의 PARTUUID와 어긋날 수 있음 |
| `load ... kernel8-bsp-study01.img` / `initramfs8-bsp-study01` | 커널·initramfs를 메모리로 읽음 | 강의는 initramfs 없음. 현재 카드는 initramfs로 부팅 |
| `booti 커널 initrd:크기 DT` | 비압축 arm64 Image 부팅 | `bootm`은 uImage용. Pi arm64 커널은 Image |
| bootstd + `boot.scr` | U-Boot가 부트 파티션에서 스크립트를 찾아 실행 | 강의는 프롬프트 입력 + `saveenv` → SD에 환경을 씀. 스크립트는 Git으로 재현 가능하고 카드에 쓰지 않음 |

U-Boot는 `booti` 직전 `fdt_chosen()`에서 env `bootargs`가 있으면 `/chosen/bootargs`를 덮어쓰고, `/chosen/u-boot,version`을 항상 추가함. 이 속성이 리눅스에서 U-Boot 경유를 판별하는 근거.

## 실측값
- U-Boot `v2026.10`, `rpi_arm64_defconfig`, `u-boot.bin` 944,488 B, `boot.scr` 642 B
- U-Boot 경유 부팅: `uname -r` = `6.18.59-v8-bsp-study01`, `/chosen/u-boot,version` = `2026.10`, `/proc/cmdline` 전환 전과 동일
- firmware overlay 전달: `dtoverlay=i2c-sensor,mpu6050,int_pin=17` 추가 후 `mpu6050@68` 노드, `interrupt-parent` = `00 00 00 07`, `inv-mpu6050-i2c` 바인딩
- 원복: `u-boot` 속성 없음, cmdline 동일
- HDMI에서 `Hit any key to stop autoboot` 확인 → `vc4-kms-v3d`·`disable_fw_kms_setup=1` 설정에서도 U-Boot 화면 출력 동작
- 메모리: 커널 `image_size` 28.4 MiB, `booti`가 `0x200000`으로 재배치 → 끝 약 `0x1E60000`. initramfs 17.7 MiB는 `0x05700000`부터. 서로 겹치지 않고 `bootm_size`(512 MiB) 안

## 트러블슈팅: firmware의 "no match initramfs" 메시지
- **증상:** `kernel=u-boot.bin`으로 바꾼 뒤 부팅 초기 HDMI에 initramfs 짝을 찾지 못했다는 메시지(원문 미기록). 이후 정상 부팅
- **확인한 계층:** firmware 단계 메시지, U-Boot·리눅스는 정상(`u-boot,version`, cmdline 동일)
- **원인:** `auto_initramfs=1`은 `kernel=` 파일 이름과 짝이 되는 initramfs를 firmware가 찾음. `u-boot.bin`에는 짝이 없음
- **조치:** 없음. 이 구성에서는 U-Boot가 `boot.scr`로 initramfs를 직접 올림. firmware가 initramfs를 안 올렸는데 initramfs 부팅이 됐다는 점이 스크립트 실행의 간접 근거가 됨

## 예상 질문과 답

**Q1. Pi는 firmware가 커널을 직접 올리는데 왜 U-Boot를 넣었나?**
Pi에서는 필수가 아님. 대부분의 SoC에서는 U-Boot가 커널·DT·initramfs를 고르고 올리므로 그 단계를 직접 구성해 보려는 목적. Yocto에서 U-Boot를 이미지에 넣는 구성의 사전 확인이기도 함.

**Q2. U-Boot를 거쳤는지 리눅스에서 어떻게 확인했나?**
U-Boot가 `booti` 직전에 DT `/chosen`에 `u-boot,version`을 항상 넣는 것을 소스에서 확인함. U-Boot 경유 부팅에서는 이 값이 `2026.10`이었고, 원복 후에는 없었음. 화면 출력 외에 리눅스 쪽 증거로 판정함.

**Q3. DT를 파일로 다시 읽지 않은 이유는?**
Pi는 firmware가 `config.txt`의 `dtparam`·`dtoverlay`를 적용해 DT를 완성함. 파일을 다시 읽으면 그 결과가 모두 빠짐. firmware DT 주소(`fdt_addr`)를 그대로 넘기고, overlay 노드와 phandle(`interrupt-parent` = `0x07`)이 커널에 남는 것으로 확인함.

**Q4. bootargs는 어디서 오고 어떻게 유지했나?**
`cmdline.txt`를 firmware가 DT `/chosen/bootargs`에 넣음. U-Boot는 env `bootargs`가 있으면 덮어쓰므로 스크립트에서 `/chosen/bootargs`를 env로 복사해 같은 값을 유지함. 전환 전후 `/proc/cmdline`을 diff해서 같음을 확인함.

**Q5. 메모리 주소가 겹치지 않는다는 건 어떻게 판단했나?**
U-Boot 기본 주소(`rpi.env`)와 실제 파일 크기로 계산함. 커널은 헤더의 `image_size`(28.4 MiB)를 기준으로 봤고, flags bit 3이 있어 `booti`가 2MB 경계(`0x200000`)로 옮기는 것도 소스에서 확인함. 끝 주소가 스크립트·initramfs 영역보다 아래.

**Q6. SD 카드 한 장으로 어떻게 안전하게 시험했나?**
변경을 부트 파티션 파일 2개 추가와 `config.txt` 한 줄로 제한함. 시작 전에 부트 파티션 전체를 WSL로 백업하고 `config.txt`도 따로 백업함. U-Boot 프롬프트에서 쓰기 명령(`saveenv` 등)은 쓰지 않기로 함. 마지막에 백업으로 원복해 기존 부팅이 그대로 되는 것까지 확인함.

## 하지 않은 것·한계
- **VEDA 강의에서 한 일:** U-Boot 빌드, 프롬프트에서 `setenv`·`fatload`·`booti`로 기본 커널 부팅, `saveenv`
- **bsp-study에서 한 일:** 스크립트(`boot.scr`) 방식, firmware DT·bootargs 유지, initramfs 로드, U-Boot 경유·DT 전달·원복을 리눅스 쪽 증거로 검증
- **미검증:** PC SD 리더로 복구(이번엔 Pi 안에서 원복), U-Boot 배너·스크립트 메시지 원문, USB 키보드로 프롬프트 입력, 시리얼 콘솔, U-Boot 경유 상태의 장시간 운용
- **한계:** 커널·initramfs 파일 이름이 스크립트에 고정됨. 화면만으로는 부팅 실패 로그를 남기기 어려움
