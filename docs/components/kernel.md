# 커널: 6.18 커스텀 커널 빌드·설치·복구와 외부 모듈

## 1. 요약
- **역할:** 64GB 카드의 커널(`6.18.59-v8-bsp-study01`)을 만들고, 모든 외부 모듈이 빌드 기준(`KDIR`)으로 쓰는 `out/kernel-rpi4`를 유지한다. Yocto 카드의 6.6.63 커널은 [yocto](yocto.md)가 맡는다.
- **상태:** 빌드·설치·부팅·외부 모듈 로드 확인. 기존 커널로의 실제 복귀와 `dtbs_install` 방식 전체 재실행은 미검증([07 6장](../learning/07-kernel-build.md#6-트러블슈팅과-한계)).
- **담당 파일:** 커널 소스 `external/linux`(커밋 `b3495ea401`, Git 대상 아님), 빌드 출력 `out/kernel-rpi4`, config fragment [configs/bsp-study.config](../../configs/bsp-study.config)
- **학습 문서:** [07 커널 빌드](../learning/07-kernel-build.md), [02 외부 모듈](../learning/02-kernel-module.md), [16 config fragment](../learning/16-ultrasonic.md#왜-드라이버를-쓰지-않고-config만-바꿨는가)

## 2. 구성
흐름: `external/linux(b3495ea401) → bcm2711_defconfig + fragment → out/kernel-rpi4 → 새 이름 파일 → config.txt [pi4] 선택 → 부팅`

| 항목 | 64GB 카드 (6.18.59 Debian) | Yocto 카드 (6.6.63) |
| --- | --- | --- |
| 소스 | raspberrypi/linux `rpi-6.18.y` 커밋 고정 | meta-raspberrypi `linux-raspberrypi_6.6.bb` |
| 설정 | `bcm2711_defconfig` + `LOCALVERSION=-v8-bsp-study01` + `configs/bsp-study.config` | 레시피 기본 config + `recipes-kernel/linux/files/bsp-study.cfg` |
| 부트 파일 | `kernel8-bsp-study01.img`, `bcm2711-rpi-4-b-bsp-study01.dtb`, `overlays-bsp-study01/`, `initramfs8-bsp-study01` | 부트 파티션 `Image`(U-Boot가 로드), 기본 이름 DTB·`overlays/` |
| 선택 방식 | `config.txt` `[pi4]`의 `kernel=`·`device_tree=`·`overlay_prefix=` | U-Boot `boot.scr`([uboot](uboot.md)) |
| 모듈 위치 | `/lib/modules/6.18.59-v8-bsp-study01/`(기존 `6.18.50+rpt-rpi-v8`과 공존) | `/lib/modules/6.6.63-v8/` |
| 외부 모듈 빌드 | `make -C out/kernel-rpi4 M=...`(예제 Makefile의 `KDIR`) | `inherit module` 레시피 |

- fragment 두 파일은 지금 같은 내용(`CONFIG_SRF04=m`)이다. 하나만 고치면 두 카드의 설정이 달라진다.
- 기존 배포판 커널과 부트 파일은 지우지 않았다. `config.txt` 한 곳으로 되돌릴 수 있게 하기 위해서다.

## 3. 빌드·배포
전체 절차(도구 설치, 백업, 배포 트리, initramfs)는 [07 5장](../learning/07-kernel-build.md#5-재현)에 펼쳐 둔다. 평소에 쓰는 것은 아래 두 가지다.

```bash
# [WSL] ~/bspworkspace/bsp-study — 설정을 바꾼 뒤 모듈만 다시 빌드(=m 추가일 때)
K="make -C external/linux O=$PWD/out/kernel-rpi4 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="
external/linux/scripts/kconfig/merge_config.sh -m -O out/kernel-rpi4 out/kernel-rpi4/.config configs/bsp-study.config
$K olddefconfig && $K prepare && $K -s kernelrelease     # 6.18.59-v8-bsp-study01
$K -j4 modules
```

```bash
# [WSL] 외부 모듈 — 예제 디렉터리에서. KDIR 기본값이 out/kernel-rpi4
make -C examples/<예제> && modinfo -F vermagic examples/<예제>/*.ko
```

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| 실행 커널 | `[Pi] uname -r` | `6.18.59-v8-bsp-study01` |
| 루트 | `[Pi] findmnt /` | `/dev/mmcblk0p2` ext4 |
| 모듈 디렉터리 | `[Pi] ls -d /lib/modules/$(uname -r)` | 있음 |
| 빌드 기준 일치 | `[WSL] $K -s kernelrelease`(`K`는 3장 블록) | `uname -r`과 같음 |
| 외부 모듈 | `modinfo -F vermagic <모듈>.ko` | `6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64` |
| 화면 | HDMI | 콘솔·데스크톱이 나옴(`uname`만으로 판단하지 않음) |

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| 커널 기능 `=m` 추가 | `configs/bsp-study.config` | Yocto `bsp-study.cfg`에 같은 줄 | `.config` diff 한 줄, `make modules` 후 `.ko`만 설치·`depmod -a`. `Image` 시각이 그대로인지 |
| 커널 기능 `=y` 추가·변경 | 위와 같음 | 커널 이미지와 모든 모듈을 다시 설치(vmlinux가 바뀜) | 07 5장 3·5·6단계, 부팅 후 4장 표 |
| 커널 커밋 변경 | `external/linux` 커밋, bringup 고정 커밋 | 모든 외부 모듈 재빌드, `LOCALVERSION`을 새 이름으로(기존 설치와 분리) | 4장 표 전체 |
| 외부 모듈 빌드 기준 변경 | 예제 Makefile `KDIR` | `KDIR`은 소스가 아니라 빌드 출력이어야 함(`Module.symvers` 필요) | vermagic |

## 6. 트러블슈팅
먼저 볼 계층: 1 빌드 → 2 부팅.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 1 빌드 | `.config`를 바꿨는데 `kernelrelease`가 이전 `6.18.59-v8` | `include/config/auto.conf` 시각 | `olddefconfig`만으로는 버전 생성에 쓰는 `auto.conf`가 갱신되지 않음 | `make prepare` |
| 1 빌드 | in-tree 모듈 하나만 `make drivers/.../srf04.ko`로 빌드하면 modpost가 `devm_iio_device_alloc ... undefined` | 의존 기호를 내보내는 모듈(`industrialio.ko`) | 단일 모듈 빌드의 modpost는 기존 `Module.symvers`를 읽지 않음(`scripts/Makefile.modpost` L104~111) | `make modules` 증분 빌드 |
| 1 빌드 | 단일 빌드 실패 뒤 `modules.order`가 1줄 | `wc -l out/kernel-rpi4/modules.order` | 단일 빌드가 목록을 덮어씀 | `make modules`로 다시 만든 뒤(1918줄) `modules_install` |
| 2 부팅 | SSH는 되는데 커스텀 커널에서 화면이 안 나옴. 로그 `Failed to resolve overlay 'vc4-kms-v3d'` | `overlays-bsp-study01/overlay_map.dtb` 유무 | `*.dtbo`만 복사해 `vc4-kms-v3d` → `-pi4` 매핑 파일이 빠짐 | [조치 절차: overlay_map 누락](#조치-절차-overlay_map-누락) |
| 2 부팅 | 새 커널이 부팅되지 않음 | HDMI·전원 LED, `config.txt` `[pi4]` 블록 | (아직 겪지 않음) | [조치 절차: 기존 커널로 복구](#조치-절차-기존-커널로-복구) |

외부 모듈의 `Unknown symbol`(의존 모듈 미적재)은 장치별 문서에 둔다([imu](imu.md#6-트러블슈팅)).

### 조치 절차: overlay_map 누락
1. 기존 배포판 overlay 폴더의 매핑 파일을 새 폴더로 복사한다(당시 복구 방법, 커널 재빌드 없음).
2. 재부팅하고 화면을 확인한다.
3. 다음 배포부터는 `dtbs_install`로 모은다(`dtbs-list`에 `overlay_map.dtb`·`hat_map.dtb`가 들어 있음).

원인을 좁힌 과정은 [07 사례 분석](../learning/07-kernel-build.md#사례-분석-ssh는-되지만-커스텀-커널-화면이-나오지-않음)에 있다.

```bash
# [Pi] 64GB 카드
ls /boot/firmware/overlays-bsp-study01/overlay_map.dtb || \
  sudo cp /boot/firmware/overlays/overlay_map.dtb /boot/firmware/overlays-bsp-study01/
sudo reboot
```

### 조치 절차: 기존 커널로 복구
1. Pi 전원을 끄고 SD 카드를 노트북 카드 리더기에 꽂는다.
2. Windows 탐색기에서 FAT 부트 파티션을 연다.
3. 백업 `out/boot-backup-20261102-222418/firmware/config.txt`를 부트 파티션의 `config.txt`로 덮어쓴다(WSL 경로는 `\\wsl.localhost\<배포판>\home\nakta\bspworkspace\bsp-study\out\...`).
4. 다시 부팅해 `uname -r`이 `6.18.50+rpt-rpi-v8`인지 본다.

이 복구는 준비만 했고 실제로 실행하지 않았다. 기존 커널·DTB·initramfs·모듈은 그대로 있어서 `config.txt`만 되돌리면 된다.

**점검 포인트**
- 외부 모듈 `insmod`가 `Invalid module format`이면 vermagic이 `uname -r`과 다른 것이다. `KDIR`과 실행 커널을 맞춘다.
- `mkinitramfs`에 버전을 빼면 실행 중인 기존 커널용이 만들어진다. 항상 `6.18.59-v8-bsp-study01`을 명시한다.

## 7. 알려진 제한
- 백업은 부트 파티션만이라 루트 파일시스템이 깨지면 이 절차로 복구할 수 없다.
- Pi의 `/proc/config.gz`(`configs.ko`)는 fragment 적용 전 설정으로 보인다. 실제 설정은 `out/kernel-rpi4/.config`가 기준이다.
