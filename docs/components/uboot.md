# U-Boot: firmware 다음 단계 부트로더와 boot.scr

## 1. 요약
- **역할:** Pi firmware가 만든 DT·bootargs를 받아 커널(64GB는 initramfs까지)을 `booti`로 시작한다. firmware의 일을 대신하지 않고 "커널을 고르고 올리는 단계"만 맡는다.
- **상태:** 두 카드에서 U-Boot 경유 부팅과 cmdline·장치 DT 노드 유지 확인(64GB v2026.10, Yocto 2024.01). 시리얼 콘솔·프롬프트 입력·Yocto 원복 실행은 미검증([11 6장](../learning/11-uboot-boot.md#6-트러블슈팅과-한계)).
- **담당 파일:**
  - 64GB 수동: [boot.cmd](../../boot/uboot/boot.cmd), [Makefile](../../boot/uboot/Makefile), U-Boot 소스 `external/u-boot`(v2026.10, Git 대상 아님)
  - Yocto: [bsp-study-devices.inc#L16](../../yocto/meta-bsp-study/conf/bsp-study-devices.inc#L16) `RPI_USE_U_BOOT`, [rpi-u-boot-scr.bbappend](../../yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/rpi-u-boot-scr.bbappend), [boot.cmd.in](../../yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in)
- **학습 문서:** [11 U-Boot](../learning/11-uboot-boot.md)

## 2. 구성
흐름: `start4.elf(config.txt → DT + overlay) → U-Boot(bootflow scan → boot.scr) → booti(Image, 같은 DT) → Linux`

| 항목 | 64GB 카드 (6.18.59 Debian) | Yocto 카드 (6.6.63) |
| --- | --- | --- |
| 현재 상태 | 시험 후 원복함. 평소에는 firmware가 커널을 직접 부팅 | 기본 부팅 경로 |
| U-Boot | v2026.10 직접 빌드, `rpi_arm64_defconfig` | poky `u-boot_2024.01.bb`, `rpi_arm64_config` |
| firmware가 실행하는 파일 | `config.txt` `[pi4]`의 `kernel=u-boot.bin` | 이름이 `kernel8.img`인 U-Boot(621,144 B), `kernel=` 줄 없음 |
| 스크립트 | `boot.cmd` → `make`로 `boot.scr` | `boot.cmd.in` → 레시피가 `boot.scr` |
| 커널·initramfs | `kernel8-bsp-study01.img` + `initramfs8-bsp-study01` | `Image`, initramfs 없음(`-`) |
| DT | firmware DT 주소 `${fdt_addr}` 그대로 | 같음 |
| 환경 저장 | 없음(`saveenv` 안 씀) | 없음(기본 스크립트의 `saveenv` 줄을 뺌), `uboot.env` 없음 |

- 두 경로 모두 `bootflow scan`이 부트 파티션의 `boot.scr`를 찾고, 메모리 주소는 `board/raspberrypi/rpi/rpi.env`의 값을 쓴다.
- U-Boot가 DT에 더하는 것은 `/chosen/u-boot,version`과 비활성 `framebuffer` 노드뿐이다(18-2 전체 DT diff). 장치 overlay 노드는 바뀌지 않는다.

## 3. 빌드·배포
- **64GB 카드:** WSL에서 빌드하고 부트 파티션에 두 파일을 추가한 뒤 `kernel=` 한 줄만 바꾼다.

```bash
# [WSL] ~/bspworkspace/bsp-study/boot/uboot — external/u-boot v2026.10 준비 후
make && scp ../../out/uboot-rpi4/u-boot.bin ../../out/uboot-rpi4/boot.scr pi:bsp/
# [Pi] 64GB — 먼저 config.txt.bak-tb01 백업([11 5-2](../learning/11-uboot-boot.md#5-2-백업))
sudo cp ~/bsp/u-boot.bin ~/bsp/boot.scr /boot/firmware/
sudo sed -i 's/^kernel=kernel8-bsp-study01.img$/kernel=u-boot.bin/' /boot/firmware/config.txt
```

- **Yocto 카드:** `RPI_USE_U_BOOT = "1"`이면 meta-raspberrypi가 부트 파일을 `Image`, `u-boot.bin;kernel8.img`, `boot.scr`로 바꾼다(`rpi-base.inc` L166~170). 이미지 빌드에 포함된다.

```bash
# [WSL, Yocto] ~/yocto/build
bitbake -e bsp-study-image | grep -E '^(RPI_USE_U_BOOT|IMAGE_BOOT_FILES)='
strings tmp/deploy/images/raspberrypi4-64/boot.scr | grep -E 'fdt|load|booti|saveenv'   # saveenv 없음
```

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| U-Boot를 거쳤는가 | `tr -d '\0' < /proc/device-tree/chosen/u-boot,version; echo` | 64GB `2026.10`, Yocto `2024.01`. 직접 부팅이면 파일 없음 |
| bootargs 전달 | U-Boot 전에 저장한 `/proc/cmdline`과 `diff` | 같음 |
| overlay 전달 | `ls /proc/device-tree/soc/i2c@7e804000/` | 장치 overlay 노드(`mpu6050@68` 등)가 있음 |
| 환경 저장 없음(Yocto) | `ls /boot/uboot.env` | 없음 |
| U-Boot 본체(Yocto) | `strings /boot/kernel8.img \| grep -m1 'U-Boot 2024'` | `U-Boot 2024.01 (Jan 08 2024 ...)` |
| 장치 전체 | `devcheck all` | 6개 PASS |

HDMI 배너만으로 판정하지 않는다. 18-2에서는 배너 문자열을 읽지 못했고 `u-boot,version`으로 판정했다.

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| 64GB에서 커널·initramfs 이름 변경 | `boot/uboot/boot.cmd`의 `load` 두 줄 | `make`로 `boot.scr` 재생성, 부트 파티션에 다시 복사 | 4장 표 |
| Yocto 스크립트 변경 | `recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in` | 자리표시 `@@KERNEL_IMAGETYPE@@`·`@@KERNEL_BOOTCMD@@`·`@@BOOT_MEDIA@@`는 레시피 sed가 치환하므로 유지 | `strings boot.scr` |
| Yocto에서 U-Boot 빼기 | `bsp-study-devices.inc`의 `RPI_USE_U_BOOT` 줄 삭제 | 부트 파일이 `Image;kernel8.img`로 돌아감 | `u-boot,version` 없음 |
| bootargs 바꾸기 | 두 카드 모두 부트 파티션 `cmdline.txt` | 스크립트는 firmware가 만든 `/chosen/bootargs`를 복사하므로 스크립트에 적지 않음 | `/proc/cmdline` |
| U-Boot 버전 변경(Yocto) | `PREFERRED_VERSION_u-boot`(아직 안 해 봄) | meta-raspberrypi u-boot bbappend 패치가 맞는지 | 배너·`u-boot,version` |

## 6. 트러블슈팅
먼저 볼 계층: 2 부팅 → 3 DT.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 2 부팅 | 64GB `kernel=u-boot.bin` 전환 후 부팅 초기에 "no match initramfs" 메시지, 이후 정상 부팅 | `config.txt`의 `auto_initramfs=1` | firmware가 `u-boot.bin`과 짝이 되는 initramfs 이름을 찾지 못함 | 조치 없음. U-Boot가 [boot.cmd#L8](../../boot/uboot/boot.cmd#L8)로 직접 올림([11 6-1](../learning/11-uboot-boot.md#6-1-firmware-단계의-no-match-initramfs-메시지)) |
| 2 부팅 | U-Boot 단계에서 멈춤 | HDMI의 `Hit any key to stop autoboot` 이후 출력 | (겪지 않음) | [조치 절차: U-Boot 빼고 직접 부팅](#조치-절차-u-boot-빼고-직접-부팅) |

### 조치 절차: U-Boot 빼고 직접 부팅
1. 전원을 끄고 SD 카드를 노트북 카드 리더기에 꽂아 FAT 부트 파티션을 연다.
2. **64GB:** `config.txt.bak-tb01`을 `config.txt`로 덮어쓴다(`kernel=kernel8-bsp-study01.img`로 돌아감).
3. **Yocto:** `kernel8.img`(U-Boot)를 `u-boot.bin`으로 복사해 두고, `Image`를 `kernel8.img`로 덮어쓴다. firmware가 커널을 직접 부팅하고 `boot.scr`는 쓰이지 않는다. U-Boot로 돌아가려면 `u-boot.bin`을 다시 `kernel8.img`로 덮어쓴다.
4. 부팅한 뒤 `u-boot,version` 파일이 없는지 본다.

64GB는 Pi 안에서 원복해 봤고(카드 리더 경로는 미검증), Yocto 방법은 준비만 했다.

**점검 포인트**
- `/proc/cmdline`이 `cmdline.txt`와 다르면 U-Boot 환경의 `bootargs`가 firmware 값을 덮어쓴 것이다. 스크립트 첫 줄의 `fdt get value bootargs /chosen bootargs`가 있는지 본다.
- 부팅 중 `saveenv`가 실행되면 부트 파티션에 `uboot.env`가 생기고 다음 부팅부터 그 환경을 읽는다. Yocto 스크립트에서 뺀 이유다.
- Yocto 커널 `Image`(약 27MB)는 `CONFIG_SYS_BOOTM_LEN`(16MB)보다 크지만 정상 부팅했다. 비압축 이미지는 이 크기 검사를 지나간다(`boot/image.c` L459~466).

## 7. 알려진 제한
- 시리얼 콘솔이 없어 U-Boot 단계 실패 로그를 남기기 어렵다.
- 64GB `boot.cmd`에 커널·initramfs 이름이 고정되어 있다.
- U-Boot 프롬프트 입력(USB 키보드)은 확인하지 않았다.
