# Yocto 최소 이미지: 직접 만든 레이어로 Pi 4 이미지 빌드와 Wi-Fi SSH

- [1. 한눈에 보기](#1-한눈에-보기)
- [2. 구조](#2-구조)
- [3. 알아야 할 개념](#3-알아야-할-개념)
- [4. 구현 흐름](#4-구현-흐름)
- [5. 재현](#5-재현)
- [6. 트러블슈팅과 한계](#6-트러블슈팅과-한계)

## 1. 한눈에 보기

- **목적:** 1~16단계에서 손으로 하던 커널·rootfs 구성을 Yocto 빌드로 바꾼다. 레이어 → `MACHINE` → 이미지 레시피가 커널·DTB·부트 파일·rootfs로 이어지는 과정을 직접 만든 레이어로 설명할 수 있게 한다. 18단계 통합 이미지의 바탕이다.
- **기능:** scarthgap(5.0) poky + meta-raspberrypi 위에 `meta-bsp-study` 레이어를 얹어 `bsp-study-image`를 만든다. core-image-minimal + dropbear SSH(비밀번호 없는 root) + Wi-Fi 자동 연결(firmware·`brcmfmac` 모듈·`wpa_supplicant`·wlan0 대기 스크립트)
- **결과:** 새 32GB 카드에서 랜선 없이 Wi-Fi로 부팅해 노트북에서 `ssh root@<IP>` 접속, 재부팅 후 자동 연결을 보드에서 확인했다. 커널 `6.6.63-v8`, Raspberry Pi 4 Model B Rev 1.5, Poky 5.0.21. 첫 빌드 127분(병렬도 3), Wi-Fi 추가 재빌드 13분
- **범위:** U-Boot·센서·Bluetooth·systemd는 넣지 않았다. 커널은 meta-raspberrypi 기본 `linux-raspberrypi` 6.6.63을 그대로 쓴다(기존 카드는 6.18.59). 64GB 카드 원복은 확인하지 않았다
- **관련 파일:** [layer.conf](meta-bsp-study/conf/layer.conf), [local.conf.sample](meta-bsp-study/conf/templates/bsp-study/local.conf.sample), [bblayers.conf.sample](meta-bsp-study/conf/templates/bsp-study/bblayers.conf.sample), [bsp-study-image.bb](meta-bsp-study/recipes-core/images/bsp-study-image.bb), [interfaces](meta-bsp-study/recipes-core/init-ifupdown/files/interfaces), [bsp-study-wifi](meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi), 진행 기록 [12 작업 노트](../docs/tasks/12-yocto-minimal.md)·[Codex 지시서](../docs/tasks/12-yocto-minimal.codex.md). poky·meta-raspberrypi·빌드 디렉터리는 저장소 밖 `~/yocto`

## 2. 구조

**흐름:** `TEMPLATECONF` → `build/conf`(local·bblayers) → 레이어 4개 → `MACHINE` + 이미지 레시피 → bitbake 태스크 → `deploy/images/raspberrypi4-64/*.wic.bz2` → 카드 전체 덮어쓰기 → firmware → 커널 → rootfs(sysvinit) → Wi-Fi → SSH

```text
 ~/bspworkspace/bsp-study (Git)          ~/yocto (Git 대상 아님)
 +-----------------------------+         +-------------------------------+
 | yocto/meta-bsp-study        |         | poky (meta, meta-poky)        |
 |  conf/layer.conf            |         | meta-raspberrypi              |
 |  conf/templates/bsp-study --+-------->| build/conf/local.conf         |
 |  recipes-core/images        | copy    | build/conf/bblayers.conf      |
 |  recipes-core/init-ifupdown | once    | wifi-secret.conf (include)    |
 |  recipes-connectivity       |         | downloads, sstate-cache       |
 +-----------------------------+         | build/tmp/deploy/images/...   |
                                         +-------------------------------+
```

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| core | poky `meta` (OE-core) | 클래스·기본 레시피(busybox, sysvinit, init-ifupdown, wpa-supplicant, core-image-minimal) |
| yocto | poky `meta-poky` | `DISTRO = "poky"` 정책 |
| raspberrypi | meta-raspberrypi | `raspberrypi4-64` 머신, `linux-raspberrypi` 커널, firmware·`config.txt`, Wi-Fi firmware 레시피 |
| bsp-study | `meta-bsp-study` | 빌드 설정 템플릿, 이미지 레시피, `interfaces` 교체, wlan0 대기 스크립트 |

## 3. 알아야 할 개념

### 왜 conf를 직접 고치지 않고 레이어 템플릿으로 만드는가

`oe-init-build-env`는 빌드 디렉터리에 `conf/`가 없을 때만 `TEMPLATECONF`의 `local.conf.sample`·`bblayers.conf.sample`을 복사한다(poky `scripts/oe-setup-builddir` L54~61). 템플릿을 저장소에 두면 다른 PC에서도 같은 명령으로 같은 conf가 생긴다. `bblayers.conf.sample`의 `##OEROOT##`는 복사할 때 실제 poky 경로로 바뀌어(같은 파일 L96) 저장소에 홈 절대 경로가 남지 않는다. 반대로 **이미 만든 `build/conf`에는 템플릿 변경이 들어가지 않는다.** 2차(Wi-Fi)에서 템플릿에 두 줄을 더했을 때 `build/conf/local.conf`에도 직접 더해야 했던 이유다.

### `MACHINE` 한 줄이 무엇을 정하는가

`MACHINE = "raspberrypi4-64"`가 meta-raspberrypi `conf/machine/raspberrypi4-64.conf`를 고른다. 이 파일과 `rpi-base.inc`가 CPU 튜닝(`cortexa72`), 커널 레시피, DTB·overlay 목록, firmware(`start4.elf` 등), `config.txt` 생성(`rpi-config`), 이미지 형식(`wic.bz2`)을 정한다. 예를 들어 `ENABLE_UART = "1"`은 `rpi-config_git.bb` L184~186에서 `config.txt`에 `enable_uart=1`을 쓴다.

### 왜 Wi-Fi firmware와 모듈을 따로 넣어야 했는가

Wi-Fi 패키지와 `kernel-modules`는 `MACHINE_EXTRA_RRECOMMENDS`에 있다(`raspberrypi4-64.conf` L8~13, `rpi-base.inc` L130). 이 목록을 가져오는 곳은 `packagegroup-base`인데 core-image-minimal은 이것을 쓰지 않는다. 1차 이미지에 `/lib/modules`가 아예 없었던 이유다. 이 커널은 `CONFIG_BRCMFMAC=m`·`CONFIG_CFG80211=m`이라 모듈이 없으면 wlan0이 생기지 않는다. firmware 레시피는 `LICENSE_FLAGS = "synaptics-killswitch"`라서 `LICENSE_FLAGS_ACCEPTED`에 넣어야 빌드된다(`linux-firmware-rpidistro_git.bb` L16). 이더넷·SD·ext4는 `=y`(커널 내장)라서 1차 이미지도 유선으로는 부팅·접속이 가능한 구성이었다.

### `ifup wlan0`이 `wpa_supplicant`까지 어떻게 이어지는가

이미지의 `ifup`은 busybox 애플릿이다. `/etc/network/interfaces`의 iface 옵션을 `IF_<대문자>` 환경변수로 바꾸고(busybox `ifupdown.c` L1113) `run-parts /etc/network/if-pre-up.d`를 실행한다(L1206). wpa-supplicant 패키지가 넣는 훅이 `IF_WPA_CONF`·`IF_WPA_DRIVER`를 읽어 `wpa_supplicant -B -i wlan0 -c ... -D nl80211`을 띄운다(poky `wpa-supplicant.sh` L13·L30~50). 그다음 `inet dhcp` 메서드가 `udhcpc`를 실행하는데, `hostname`·`udhcpc_opts` 옵션이 있을 때만 그 인자를 붙인다(`ifupdown.c` L648). 그래서 `interfaces`를 bbappend로 바꿔 `nl80211`·`hostname`·재시도 옵션을 넣었다.

### 왜 대기 스크립트가 필요한가

두 번째 부팅부터 udev init은 `udevadm settle --timeout=3`을 백그라운드로 돌리고 넘어간다(rootfs `/etc/init.d/udev` L103). `brcmfmac`의 firmware 로드는 그보다 늦을 수 있어서, rc5 `S01networking`의 `ifup -a` 시점에 wlan0이 없을 수 있다. 그래서 `auto wlan0`을 쓰지 않고 S02 스크립트가 wlan0을 기다렸다가 올린다.

## 4. 구현 흐름

1. **레이어 등록** — [layer.conf](meta-bsp-study/conf/layer.conf#L6): 컬렉션 `bsp-study`, 우선순위 9, `LAYERSERIES_COMPAT = "scarthgap"`, `LAYERDEPENDS = "core raspberrypi"`. poky `bitbake-layers create-layer` 템플릿과 같은 형식이고 의존성만 더했다.
2. **빌드 설정** — [local.conf.sample](meta-bsp-study/conf/templates/bsp-study/local.conf.sample#L1): `MACHINE`, `DISTRO`, `DL_DIR`·`SSTATE_DIR`은 `${TOPDIR}/../`(빌드 디렉터리를 지워도 다운로드·캐시가 남음), `ENABLE_UART`, 라이선스 플래그, `include ${TOPDIR}/../wifi-secret.conf`. `include`는 파일이 없어도 오류가 아니다. 병렬도는 PC마다 달라 템플릿에 넣지 않고 `build/conf/local.conf`에 둔다(16GB PC에서 3).
3. **이미지 레시피** — [bsp-study-image.bb](meta-bsp-study/recipes-core/images/bsp-study-image.bb#L1): `require core-image-minimal.bb` 위에 `IMAGE_FEATURES`로 dropbear와 로그인 기능 세 개(`empty-root-password`·`allow-empty-password`·`allow-root-login`), [L14](meta-bsp-study/recipes-core/images/bsp-study-image.bb#L14)에서 Wi-Fi 패키지 6개를 넣는다. poky 기본 `local.conf.sample`에는 `EXTRA_IMAGE_FEATURES ?= "debug-tweaks"`가 있지만 이 템플릿에는 없어서, 필요한 기능만 레시피에 명시했다.
4. **Wi-Fi 설정 파일** — [L27](meta-bsp-study/recipes-core/images/bsp-study-image.bb#L27) `ROOTFS_POSTPROCESS_COMMAND`의 python 함수가 rootfs에 `/etc/wpa_supplicant.conf`를 쓴다. SSID는 UTF-8 hex, `psk`는 `wpa_passphrase`와 같은 PBKDF2-HMAC-SHA1(4096회, 32바이트), 권한 0600. 별도 패키지로 만들지 않은 이유는 값이 sstate·`deploy/rpm`에 남지 않게 하려는 것이다. `image.bbclass` L124~144가 후처리 함수와 그 안의 `getVar` 변수를 `do_rootfs` 서명에 넣으므로 SSID·비밀번호를 바꾸면 rootfs만 다시 만든다(SSID 수정 후 재빌드 2분 27초).
5. **`interfaces` 교체** — [init-ifupdown_%.bbappend](meta-bsp-study/recipes-core/init-ifupdown/init-ifupdown_%25.bbappend)는 `FILESEXTRAPATHS:prepend` 한 줄이다. 원본 레시피의 `file://interfaces`가 내 레이어의 [interfaces](meta-bsp-study/recipes-core/init-ifupdown/files/interfaces#L8)를 먼저 찾는다. 원본과 다른 곳은 wlan0 항목뿐이다(`wext` → `nl80211`, `hostname raspberrypi4-64`, `udhcpc_opts -t 5 -A 5`).
6. **wlan0 대기·재시도** — [bsp-study-wifi](meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi#L4)는 백그라운드 서브셸에서 wlan0을 최대 20초 기다리고, `ifup` 후 30초 안에 IPv4가 없으면 `ifdown`/`ifup`을 2회 더 한다([L20](meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi#L20)). [레시피](meta-bsp-study/recipes-connectivity/bsp-study-wifi/bsp-study-wifi.bb#L11)의 `INITSCRIPT_PARAMS = "start 02 5 ..."`로 rc5 `S01networking` → `S02bsp-study-wifi` → `S10dropbear` 순서가 된다. `udhcpc`는 poky 기본 `-b` 때문에 lease를 못 받아도 백그라운드에서 계속 재시도한다(busybox `dhcpc.c` L1454~1489).
7. **카드** — `wic`가 부트(FAT 130M)와 rootfs(ext4)를 묶은 디스크 이미지를 만든다. 카드에 파일을 더하는 방식이 아니라 **카드 전체를 덮어쓴다.** rootfs 파티션은 이미지 크기 그대로라 `df -h /`가 135M로 보인다.

VEDA 강의(`VEDA/임베디드리눅스/강의자료/임베디드리눅스2.pdf` 13~31쪽)와 흐름은 같다(clone → `oe-init-build-env` → conf → `bitbake` → `deploy/images`). 다른 점만 남긴다.

| 항목 | 강의 | 이 실습 | 이유 |
| --- | --- | --- | --- |
| 릴리스·머신 | kirkstone, `raspberrypi5`, meta-openembedded | scarthgap, `raspberrypi4-64`, meta-oe 없음 | 보드가 Pi 4이고 필요한 패키지가 OE-core에 있음 |
| conf | vim으로 `local.conf`·`bblayers.conf` 수정, `/home/ubuntu/...` 절대 경로 | 레이어 템플릿 + `##OEROOT##` 상대 경로 | 저장소에 남고 다른 PC에서 재현됨 |
| root 로그인 | 기본 `debug-tweaks` | 기능 세 개를 이미지 레시피에 명시 | 템플릿에 `debug-tweaks`가 없음 |
| 확인 | `runqemu qemuarm` | 실제 카드 부팅 + Wi-Fi SSH | `raspberrypi4-64` 이미지는 `runqemu`로 실행하지 못함 |

## 5. 재현

선행 조건: Ubuntu 22.04(WSL2), 리눅스 홈에 100GB 이상, 호스트 패키지(작업 노트 1단계 목록), 이 저장소가 `~/bspworkspace/bsp-study`, 소스는 `~/yocto`(두 경로가 모두 홈 바로 아래여야 `bblayers.conf.sample`의 상대 경로가 맞음).

```bash
# [WSL] ~/yocto — 소스
mkdir -p ~/yocto && cd ~/yocto
git clone -b scarthgap https://git.yoctoproject.org/poky
git clone -b scarthgap https://github.com/agherzan/meta-raspberrypi
```

```bash
# [WSL] ~/yocto — Wi-Fi 값(저장소 밖, 권한 600)
( umask 077; touch ~/yocto/wifi-secret.conf )
vim ~/yocto/wifi-secret.conf     # WIFI_SSID = "<SSID>" / WIFI_PSK = "<비밀번호>"
```

```bash
# [WSL, tmux] ~/yocto — 빌드 셸(Windows PATH 제거) → 빌드 디렉터리
tmux new -A -s yocto
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
cd ~/yocto
TEMPLATECONF=$HOME/bspworkspace/bsp-study/yocto/meta-bsp-study/conf/templates/bsp-study \
  source poky/oe-init-build-env build
bitbake-layers show-layers           # core·yocto·raspberrypi·bsp-study
time bitbake bsp-study-image
```

- RAM 16GB PC면 빌드 전에 `conf/local.conf`에 `BB_NUMBER_THREADS = "3"`, `PARALLEL_MAKE = "-j 3"`을 더한다.
- `build/conf`가 이미 있으면 템플릿이 다시 복사되지 않는다. 새 셸에서는 PATH export와 `source poky/oe-init-build-env build`만 다시 한다.

```bash
# [WSL] ~/yocto/build — 산출물과 이미지 안 SSID 확인(psk는 출력하지 않음)
D=tmp/deploy/images/raspberrypi4-64
R=tmp/work/raspberrypi4_64-poky-linux/bsp-study-image/1.0/rootfs
ls -lhL $D/bsp-study-image-raspberrypi4-64.rootfs.wic.bz2
grep -o 'ssid=[0-9a-f]*' $R/etc/wpa_supplicant.conf | cut -d= -f2 | xxd -r -p; echo
bzcat $D/bsp-study-image-raspberrypi4-64.rootfs.wic.bz2 > /mnt/d/bsp-images/bsp-study-image.img
```

Windows Raspberry Pi Imager: Raspberry Pi 4 → "Use custom" → `.img` → **32GB 카드(용량 확인)** → 커스터마이즈 "아니요". 랜선 없이 부팅한 뒤:

```bash
# [노트북 WSL] — 같은 Wi-Fi. IP는 공유기 DHCP 목록
ssh -o StrictHostKeyChecking=accept-new root@<IP>
```

```bash
# [Pi, Yocto] — busybox 명령
uname -r; cat /etc/issue
lsmod | grep -E 'brcmfmac|cfg80211'
iw dev wlan0 link | head -n 3
ip addr show wlan0
```

확인: `6.6.63-v8`, `Poky ... 5.0.21`, `brcmfmac`·`cfg80211` 로드, `Connected to`, wlan0 `inet`. 연결이 안 될 때의 HDMI 진단·수동 연결 절차는 [작업 노트 7단계](../docs/tasks/12-yocto-minimal.md#7-부팅wi-fi-ssh-확인-노트북)에 있다.

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| wlan0은 있는데 `iw dev wlan0 link`가 Not connected, `udhcpc`가 discover만 반복 | `wifi-secret.conf`의 SSID 대소문자 오류. 이미지에는 SSID가 hex로 들어가 눈으로 비교하기 어려움 | `iw dev wlan0 scan \| grep SSID:`와 설정 SSID(hex → 글자)를 비교. `wifi-secret.conf` 수정 → 재빌드(rootfs만, 2분) → 카드 다시 쓰기 |
| 템플릿에 줄을 더했는데 `bitbake -e`에 안 보임 | `build/conf`는 처음 한 번만 템플릿에서 복사됨 | `build/conf/local.conf`에 같은 줄을 직접 더하거나 `build/conf`를 지우고 다시 만듦(병렬도도 다시) |
| 1차 이미지에 `/lib/modules`가 없음 | `kernel-modules`가 `MACHINE_EXTRA_RRECOMMENDS`에 있고 core-image-minimal은 그 목록을 쓰지 않음 | 이미지 레시피에 `kernel-modules`를 직접 넣음(2차) |
| `logread`가 비어 있음 | busybox syslog `DESTINATION=file`이라 메모리 버퍼가 아니라 `/var/log/messages`에 씀 | `grep bsp-study-wifi /var/log/messages` |
| `ip -br`, `head -3`, `/etc/os-release`가 없음 | busybox 애플릿 옵션이 적고 core-image-minimal에 os-release 패키지가 없음 | `ip addr show`, `head -n 3`, `cat /etc/issue` |
| `p2p-dev-wlan0` 생성 실패 메시지 | Wi-Fi Direct용 가상 인터페이스. 이 실습에서는 쓰지 않음 | 무시 |
| 첫 빌드 경고 3개(`Failed to fetch URL ...`) | 원래 주소에서 받지 못해 미러에서 받음 | `downloads`에 `.done`이 생기면 다음부터 나오지 않음 |

- 커널 버전이 다르다: 이 이미지는 `linux-raspberrypi` 6.6.63, 기존 장치 작업 카드는 6.18.59다. 18단계는 Yocto 기본 커널에 맞춰 직접 만든 드라이버·overlay를 레시피로 다시 빌드하는 방향으로 진행한다.
- 이미지와 카드에는 Wi-Fi `psk`(PBKDF2 값)가 들어 있어 그 값만으로 그 Wi-Fi에 접속할 수 있다. 학습용 보드라 비밀번호 없는 root SSH와 함께 보안은 고려하지 않았다.
- 미검증: HDMI 콘솔 수동 연결 절차와 Pi에서 SSID hex를 글자로 바꾸는 `printf` 명령, USB-시리얼 콘솔, 공유기 DHCP 목록의 `raspberrypi4-64` 이름 표시, 대기 스크립트의 재시도 로그, 64GB 카드 원복, WSL 창을 모두 닫았을 때 tmux 빌드 유지
