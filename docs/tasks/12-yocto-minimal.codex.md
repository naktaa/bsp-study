# 12 Yocto 최소 이미지 — Codex 지시서

작업 노트: [12-yocto-minimal.md](12-yocto-minimal.md)

## 지시 (Claude)
- 실행 장소: 데스크톱 WSL. 작업 노트 1·2단계가 끝나 `~/yocto/poky`·`~/yocto/meta-raspberrypi`(scarthgap)가 있는 상태에서 시작한다. 없으면 멈추고 보고한다.
- 변경 파일(모두 새 파일, `yocto/meta-bsp-study/` 아래):
  - `conf/layer.conf`
  - `conf/templates/bsp-study/local.conf.sample`
  - `conf/templates/bsp-study/bblayers.conf.sample`
  - `conf/templates/bsp-study/conf-notes.txt`
  - `recipes-core/images/bsp-study-image.bb`
- 요구사항 — `layer.conf`:
  - 컬렉션 이름 `bsp-study`, 패턴·우선순위는 meta-raspberrypi `conf/layer.conf` 형식을 따른다
  - `LAYERSERIES_COMPAT_bsp-study = "scarthgap"`, `LAYERDEPENDS_bsp-study = "core raspberrypi"`
- 요구사항 — `local.conf.sample`:
  - `MACHINE = "raspberrypi4-64"`, `DISTRO = "poky"`
  - `DL_DIR`·`SSTATE_DIR`는 `${TOPDIR}/../downloads`, `${TOPDIR}/../sstate-cache`(빌드 디렉터리 `~/yocto/build` 기준)
  - `ENABLE_UART = "1"`(meta-raspberrypi `rpi-config`가 `config.txt`에 `enable_uart=1`을 쓰는지 확인하고 근거를 구현 보고에 적는다)
  - `BB_NUMBER_THREADS`·`PARALLEL_MAKE`는 넣지 않는다(bitbake 기본값 사용, 메모리 부족 시 사용자가 빌드 디렉터리 `local.conf`에서 조정)
  - `~`·`$HOME`·`/home/...` 절대 경로를 쓰지 않는다
  - `CONF_VERSION`은 poky `meta-poky/conf/templates/default/local.conf.sample`의 값과 같게
  - `LICENSE_FLAGS_ACCEPTED`는 raspberrypi4-64 기본 패키지(Wi-Fi·BT 펌웨어 등)가 요구할 때만 넣고, 어떤 레시피가 요구하는지 근거를 구현 보고에 적는다
  - poky 기본 sample 전체를 복사하지 않는다. 위 항목과 동작에 필요한 최소 줄만 둔다
- 요구사항 — `bblayers.conf.sample`:
  - poky `meta`·`meta-poky`, meta-raspberrypi, meta-bsp-study 4개. `meta-yocto-bsp`는 넣지 않는다
  - 경로는 `##OEROOT##` 기준 상대 경로로 쓴다. 디렉터리 배치는 `~/yocto/poky`, `~/yocto/meta-raspberrypi`, `~/bspworkspace/bsp-study`를 전제로 하고, 치환 방식은 poky `scripts/oe-setup-builddir`에서 확인한다
- 요구사항 — `conf-notes.txt`: `bitbake bsp-study-image` 한 줄 안내
- 요구사항 — `bsp-study-image.bb`:
  - `core-image-minimal`을 `require`로 가져온다
  - SSH는 dropbear(`ssh-server-dropbear`)
  - 비밀번호 없는 root SSH 로그인을 켠다. scarthgap에서 쓰는 방식(`debug-tweaks` 또는 `empty-root-password`·`allow-empty-password`·`allow-root-login`)을 poky `meta/classes-recipe/`·`meta/conf/documentation.conf` 등에서 확인하고 근거를 구현 보고에 적는다
  - `SUMMARY`·`LICENSE`는 OE-core 이미지 레시피 형식을 따른다. 그 밖의 패키지는 넣지 않는다
- 주석은 한글로, 코드만으로 알기 어려운 이유(학습용 root 로그인 등)에만 쓴다
- 하지 말 것: bitbake·`bitbake-layers`·`oe-init-build-env` 실행, `~/yocto` 아래 파일 수정, U-Boot(`RPI_USE_U_BOOT`)·Wi-Fi 설정·커널 bbappend·센서 레시피·추가 패키지, 문서 수정(구현 보고 제외), 보드 조작
- 확인 명령:

```bash
# [데스크톱 WSL] ~/bspworkspace/bsp-study
find yocto/meta-bsp-study -type f | sort
git -C ~/yocto/poky log -1 --format=%h
git -C ~/yocto/meta-raspberrypi log -1 --format=%h
grep -n 'LAYERSERIES_COMPAT' ~/yocto/meta-raspberrypi/conf/layer.conf
grep -rnE '/home|~/|\$HOME|BB_NUMBER_THREADS|PARALLEL_MAKE' yocto/meta-bsp-study || echo none
git diff --check
```

## 구현 보고 (Codex)
- 변경 요약: 지시된 `yocto/meta-bsp-study/` 아래 새 파일 5개를 생성했다. scarthgap 레이어, `raspberrypi4-64`·`poky` 최소 설정 템플릿, 레이어 4개의 상대 경로, 빌드 안내, `core-image-minimal` 기반 dropbear 이미지를 구성했다. 이미지에 추가한 기능은 SSH와 비밀번호 없는 root 로그인에 필요한 항목뿐이다. 기존 작업 노트의 변경은 건드리지 않았다.
- 빌드 결과: 미실행(3단계에서 사용자 확인)
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`): 해당 없음(Yocto 메타데이터)
- 판단이 필요했던 부분:
  1. **UART:** `~/yocto/meta-raspberrypi/recipes-bsp/bootfiles/rpi-config_git.bb:44–48`에서 `do_deploy()`의 `CONFIG`가 `${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/config.txt`로 지정된다. 같은 파일 `184–186`행에서 `ENABLE_UART`가 `1` 또는 `0`이면 `enable_uart=${ENABLE_UART}`를 이 파일에 추가한다. 따라서 템플릿의 `ENABLE_UART = "1"`은 `enable_uart=1` 생성으로 이어진다. 생성 산출물 자체는 아직 확인하지 않았다.
  2. **템플릿 치환과 상대 경로:** `~/yocto/poky/scripts/oe-setup-builddir:92–98`은 `sed -e "s|##OEROOT##|$OEROOT|g"`로 토큰을 치환해 빌드 디렉터리의 `conf/bblayers.conf`를 만든다. `~/yocto/poky/oe-init-build-env:38–44`에서 `OEROOT`는 스크립트 디렉터리의 정규화된 경로다(실행하지 않고 읽기 확인). 전제 배치의 `OEROOT`는 `~/yocto/poky`이므로 `meta`·`meta-poky`는 그 아래, meta-raspberrypi는 `../meta-raspberrypi`, 저장소 레이어는 `../../bspworkspace/bsp-study/yocto/meta-bsp-study`다. 이 네 경로를 토큰 치환 후 정규화하여 각 `conf/layer.conf`가 존재함을 확인했다. `BBPATH`, `BBFILES`, `POKY_BBLAYERS_CONF_VERSION = "2"`는 `~/yocto/poky/meta-poky/conf/templates/default/bblayers.conf.sample:3–6`을 따랐다.
  3. **CONF_VERSION:** `~/yocto/poky/meta-poky/conf/templates/default/local.conf.sample:282–285`의 설명과 `CONF_VERSION = "2"`를 확인해 같은 값을 사용했다.
  4. **LICENSE_FLAGS_ACCEPTED:** 현재 이미지에는 필요하지 않다는 정적 판단으로 넣지 않았다. `~/yocto/meta-raspberrypi/conf/machine/raspberrypi4-64.conf:8–13`은 Wi-Fi·BT 펌웨어를 `MACHINE_EXTRA_RRECOMMENDS`에 추가한다. 이 중 `~/yocto/meta-raspberrypi/recipes-kernel/linux-firmware-rpidistro/linux-firmware-rpidistro_git.bb:16`은 `LICENSE_FLAGS = "synaptics-killswitch"`를 요구한다. 다만 `~/yocto/poky/meta/recipes-core/images/core-image-minimal.bb:3`은 `packagegroup-core-boot ${CORE_IMAGE_EXTRA_INSTALL}`만 설치하고, `~/yocto/poky/meta/classes-recipe/core-image.bbclass:82`에서 추가 설치 목록의 기본값은 비어 있다. `~/yocto/poky/meta/recipes-core/packagegroups/packagegroup-core-boot.bb:23–42`는 `MACHINE_ESSENTIAL_EXTRA_RDEPENDS`·`MACHINE_ESSENTIAL_EXTRA_RRECOMMENDS`를 사용하며 `MACHINE_EXTRA_RRECOMMENDS`를 가져오지 않는다. 후자를 가져오는 곳은 `~/yocto/poky/meta/recipes-core/packagegroups/packagegroup-base.bb:132–134`의 `packagegroup-machine-base`다. 따라서 이 최소 이미지에서는 해당 Wi-Fi 펌웨어 레시피를 선택하지 않는다. 플래그를 받지 않은 레시피를 건너뛰는 처리는 `~/yocto/poky/meta/classes-global/base.bbclass:528–536`에 있다. 실제 파싱·의존성 확인은 사용자 3단계에 남긴다.
  5. **비밀번호 없는 root SSH:** 이미지에 `ssh-server-dropbear`, `empty-root-password`, `allow-empty-password`, `allow-root-login`을 추가했다. `~/yocto/poky/meta/classes-recipe/core-image.bbclass:57`은 SSH 기능을 `packagegroup-core-ssh-dropbear`로 연결하고, `~/yocto/poky/meta/recipes-core/packagegroups/packagegroup-core-ssh-dropbear.bb:5–6`은 dropbear 의존성과 `openssh-sftp-server` 기본 권장을 정의한다. 별도 패키지를 직접 추가하지 않았다. `~/yocto/poky/meta/classes-recipe/image.bbclass:43`에서 로그인 기능 세 항목이 유효함을 확인했다. `~/yocto/poky/meta/classes-recipe/rootfs-postcommands.bbclass:7–14`에 따라 빈 root 비밀번호를 유지하고, 빈 비밀번호 SSH 인증과 root SSH 로그인을 허용한다. 같은 파일 `257–264`행은 dropbear에 `-B`를 추가하고 `285–288`행은 root 로그인을 금지하는 `-w`를 제거한다. 필요한 세 기능을 명시했으며 `debug-tweaks`는 추가하지 않았다. 보드에서의 로그인은 미검증이다.
  6. **레이어·이미지 형식:** `~/yocto/meta-raspberrypi/conf/layer.conf:2–13`의 BBPATH·BBFILES·컬렉션·즉시 확장 패턴·우선순위 형식과 우선순위 `9`를 따랐다. 컬렉션은 `bsp-study`, 호환 릴리스는 `scarthgap`, 의존성은 `core raspberrypi`로 지정했다. `~/yocto/poky/meta/recipes-core/images/core-image-minimal.bb:1–9`를 근거로 `require recipes-core/images/core-image-minimal.bb` 뒤에 이미지 설명과 `LICENSE = "MIT"`를 지정했다.

확인 명령은 저장소 루트에서 지시서 그대로 실행했다. 출력은 다음과 같다(`git diff --check`는 출력 없이 종료 코드 0).

```text
$ find yocto/meta-bsp-study -type f | sort
yocto/meta-bsp-study/conf/layer.conf
yocto/meta-bsp-study/conf/templates/bsp-study/bblayers.conf.sample
yocto/meta-bsp-study/conf/templates/bsp-study/conf-notes.txt
yocto/meta-bsp-study/conf/templates/bsp-study/local.conf.sample
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb
$ git -C ~/yocto/poky log -1 --format=%h
6b7c1066f7
$ git -C ~/yocto/meta-raspberrypi log -1 --format=%h
018d444
$ grep -n 'LAYERSERIES_COMPAT' ~/yocto/meta-raspberrypi/conf/layer.conf
12:LAYERSERIES_COMPAT_raspberrypi = "nanbield scarthgap"
$ grep -rnE '/home|~/|\$HOME|BB_NUMBER_THREADS|PARALLEL_MAKE' yocto/meta-bsp-study || echo none
none
$ git diff --check
```

추가 정적 확인: 네 레이어 경로의 토큰 치환·정규화와 `conf/layer.conf` 존재 확인 통과. 새 파일 5개의 줄 끝 공백·마지막 개행 확인 통과. `bitbake`·`bitbake-layers`·`oe-init-build-env`는 실행하지 않았으며, `~/yocto` 소스 수정·보드 조작·커밋도 하지 않았다.

## 리뷰 (Claude)
- 결과: 통과(정적 검토. 파싱·빌드는 작업 노트 3·4단계에서 확인)
- 범위: 새 파일 5개(`yocto/meta-bsp-study/`)만 추가됨. `~/yocto` 소스·다른 문서 변경 없음(`git status`). 홈 절대 경로·병렬도 없음, `git diff --check` 통과
- `layer.conf`: `LAYERDEPENDS`의 `raspberrypi`가 meta-raspberrypi `BBFILE_COLLECTIONS` 이름과 같음(`conf/layer.conf` L8). 형식·우선순위 9도 같음
- `TEMPLATECONF`: `oe-setup-builddir`가 "한 단계 위가 `templates`이고 `../../layer.conf`가 있어야 함"을 검사함(L54~56). `conf/templates/bsp-study` 배치가 이 조건을 만족함. `conf-summary.txt`가 없으면 poky 기본 파일을 씀(L103~106)
- `bblayers.conf.sample`: `OEROOT`는 `oe-init-build-env`에서 `readlink -f`로 정규화된 `~/yocto/poky`(L39~40)이고 `##OEROOT##`를 그 값으로 치환함(`oe-setup-builddir` L96). `../../bspworkspace/...`는 `~/yocto`와 `~/bspworkspace`가 홈 바로 아래에 있을 때만 맞음. 지시서가 정한 배치라 유지
- `ENABLE_UART`: `rpi-config_git.bb` L184~186에서 `enable_uart=1`을 `config.txt`에 추가함. `CONF_VERSION = "2"`는 poky 기본 sample과 같음
- 이미지: 세 로그인 기능이 `image.bbclass` L43의 유효 항목에 있고, `rootfs-postcommands.bbclass` L7~14가 각각 빈 비밀번호 유지·빈 비밀번호 SSH·root SSH를 처리함. `debug-tweaks` 대신 필요한 기능만 명시한 판단이 맞음
- `LICENSE_FLAGS_ACCEPTED` 생략: Wi-Fi·BT 펌웨어는 `MACHINE_EXTRA_RRECOMMENDS`(`raspberrypi4-64.conf` L8~13)에 있고, 이를 가져오는 `packagegroup-base`를 core-image-minimal이 쓰지 않으므로 맞음
- 참고(수정 요구 아님): `kernel-modules`도 같은 `MACHINE_EXTRA_RRECOMMENDS`에 있어(`rpi-base.inc` L130) 이미지에 들어가지 않음. 보드에서 `/lib/modules/$(uname -r)`가 거의 비어 있을 수 있고, 작업 노트 7단계처럼 결과만 기록함
- 수정 지시: 없음

---

# 2차: Wi-Fi 자동 연결

## 지시 (Claude)
- 목적: 랜선 없이 집 Wi-Fi로 부팅 후 바로 `ssh root@<IP>` 접속. 1차 레이어 위에 추가한다. 실행 장소·전제는 1차와 같다(`~/yocto` 소스는 읽기만).
- 변경 파일(`yocto/meta-bsp-study/` 아래):
  - 수정: `conf/templates/bsp-study/local.conf.sample`, `recipes-core/images/bsp-study-image.bb`
  - 새 파일: `recipes-core/init-ifupdown/init-ifupdown_%.bbappend`, `recipes-core/init-ifupdown/files/interfaces`, `recipes-connectivity/bsp-study-wifi/bsp-study-wifi.bb`, `recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi`
- 근거(Claude 확인, 경로는 `~/yocto` 기준):
  - firmware 패키지 `linux-firmware-rpidistro-bcm43455`, `LICENSE_FLAGS = "synaptics-killswitch"`: `meta-raspberrypi/recipes-kernel/linux-firmware-rpidistro/linux-firmware-rpidistro_git.bb` L16·L64·L92, 허용 예시 `meta-raspberrypi/docs/ipcompliance.md` L20
  - 커널 `CONFIG_BRCMFMAC=m`·`CONFIG_CFG80211=m`·`CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y`, vendor 모듈 `brcmfmac-cyw.ko`: `build/tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git/` 아래 `.config`·`image/lib/modules`
  - `wireless-regdb-static`: `poky/meta/recipes-kernel/wireless-regdb/wireless-regdb_2026.09.03.bb` L26
  - busybox `ifup`이 `/etc/network/if-pre-up.d`를 `run-parts`로 실행하고 iface 옵션을 `IF_<대문자>` 환경변수로 넘김: busybox `networking/ifupdown.c` L1113·L1206. wpa-supplicant 훅이 `IF_WPA_CONF`·`IF_WPA_DRIVER`를 읽음: `poky/meta/recipes-connectivity/wpa-supplicant/wpa-supplicant/wpa-supplicant.sh` L13·L30~38, 설치 위치 `wpa-supplicant_2.10.bb` L94
  - `udhcpc`는 iface에 `hostname` 옵션이 있을 때만 `-x hostname:`을 보내고, `udhcpc_opts`를 뒤에 붙임: `ifupdown.c` L648. poky `UDHCPC_CMD_OPTIONS="-R -b"`에서 `-b`는 lease 실패 시 백그라운드로 계속 재시도, `-t`(discover 횟수, 기본 3)·`-A`(재시도 대기, 기본 20초): busybox `networking/udhcp/dhcpc.c` L1180~1186·L1454~1489
  - 2번째 부팅부터 udev는 `udevadm settle --timeout=3`을 백그라운드로 돌리고 넘어감(rootfs `/etc/init.d/udev` L103). 네트워크는 rc5 `S01networking`이라 wlan0이 아직 없을 수 있음
- 요구사항 — `local.conf.sample`: `LICENSE_FLAGS_ACCEPTED = "synaptics-killswitch"`와 `include ${TOPDIR}/../wifi-secret.conf`를 추가한다(`include`라서 파일이 없어도 파싱됨). 주석은 한글 한 줄씩
- 요구사항 — `bsp-study-image.bb`:
  - `IMAGE_INSTALL:append`로 `kernel-modules linux-firmware-rpidistro-bcm43455 wireless-regdb-static wpa-supplicant iw bsp-study-wifi`
  - `WIFI_SSID ??= ""`, `WIFI_PSK ??= ""`
  - python 함수를 `ROOTFS_POSTPROCESS_COMMAND`에 추가한다. 구분자 형식은 poky `meta/classes-recipe/rootfs-postcommands.bbclass`의 기존 항목을 따른다
    - `WIFI_SSID`가 비어 있으면 `bb.warn`만 하고 끝낸다(패키지 기본 `wpa_supplicant.conf` 유지)
    - SSID가 있는데 `WIFI_PSK` 길이가 8~63이 아니면 `bb.fatal`
    - `${IMAGE_ROOTFS}/etc/wpa_supplicant.conf`를 새로 쓴다: `ctrl_interface=/var/run/wpa_supplicant`, `country=KR`, `network={ ssid=<SSID UTF-8 바이트의 hex, 따옴표 없음> psk=<PBKDF2-HMAC-SHA1(passphrase, ssid, 4096회, 32바이트)의 hex> }`. 권한 0600, 소유자는 rootfs 기본(root)
    - 비밀번호 원문·`psk` 값을 `bb.note`·`bb.warn` 등 로그에 출력하지 않는다
  - 이유: 별도 패키지로 만들면 비밀번호가 sstate·`deploy/rpm`에 남으므로 이미지 후처리에서 만든다. 원문 대신 `psk` 해시를 넣는다
- 요구사항 — init-ifupdown bbappend:
  - `FILESEXTRAPATHS:prepend := "${THISDIR}/files:"`
  - `files/interfaces`는 poky `meta/recipes-core/init-ifupdown/init-ifupdown-1.0/interfaces`를 복사하고 wlan0 항목만 바꾼다: `wpa-driver nl80211`, `wpa-conf /etc/wpa_supplicant.conf`, `hostname raspberrypi4-64`, `udhcpc_opts -t 5 -A 5`. `wireless_mode`·`wireless_essid` 줄은 뺀다. `auto wlan0`은 넣지 않는다(대기 스크립트가 올림). 그 밖의 항목(lo, eth0 등)은 원본 그대로
  - `hostname` 값은 `/etc/hostname`(poky `base-files`의 기본값 `${MACHINE}`)과 같음을 확인하고 구현 보고에 근거를 적는다
- 요구사항 — `bsp-study-wifi.bb`:
  - `SUMMARY`·`LICENSE = "MIT"`·`LIC_FILES_CHKSUM`(`${COMMON_LICENSE_DIR}/MIT`), `SRC_URI = "file://bsp-study-wifi"`, `S`·설치 형식은 poky `init-ifupdown_1.0.bb`처럼 scarthgap 레시피 형식을 따른다
  - `inherit update-rc.d`, `INITSCRIPT_NAME = "bsp-study-wifi"`, `INITSCRIPT_PARAMS`는 rc5에서 `S02`(networking 뒤, dropbear 앞)로 시작하게 한다. 형식은 `update-rc.d.bbclass`와 poky 레시피 예에서 확인
  - `RDEPENDS:${PN}`에 `wpa-supplicant`
- 요구사항 — `files/bsp-study-wifi`(busybox `sh`):
  - `start`: 아래를 백그라운드 서브셸로 실행하고 바로 반환한다(부팅·dropbear를 막지 않음)
    1. `/sys/class/net/wlan0`을 1초 간격 최대 20초 기다린다. 없으면 `logger`로 남기고 끝낸다
    2. `ifup wlan0` 후 wlan0에 IPv4가 생기는지 1초 간격 최대 30초 본다
    3. IPv4가 없으면 `ifdown wlan0` → `ifup wlan0`을 최대 2회 더 한다(busybox `ifup`은 이미 올라간 iface를 다시 올리지 않으므로 `ifdown` 먼저)
    4. 각 단계 결과를 `logger -t bsp-study-wifi`로 남긴다
  - `stop`: `ifdown wlan0`
  - 주석은 한글로, 위 대기·재시도 이유에만 짧게
- 하지 않을 것: 비밀번호·SSID를 저장소 파일·커밋·구현 보고에 적기, `~/yocto/wifi-secret.conf` 만들기·읽기, systemd 전환, NetworkManager·connman·Bluetooth, `auto wlan0`, 1차 파일 중 위에 없는 것 수정, bitbake·`bitbake-layers`·`oe-init-build-env` 실행, 문서 수정(구현 보고 제외), 보드 조작
- 확인 명령:

```bash
# [데스크톱 WSL] ~/bspworkspace/bsp-study
find yocto/meta-bsp-study -type f | sort
diff ~/yocto/poky/meta/recipes-core/init-ifupdown/init-ifupdown-1.0/interfaces \
  yocto/meta-bsp-study/recipes-core/init-ifupdown/files/interfaces
sh -n yocto/meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi && echo sh-ok
ls -l yocto/meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi
grep -rnE 'WIFI_(SSID|PSK)' yocto/meta-bsp-study
grep -rnE '/home|~/|\$HOME|BB_NUMBER_THREADS|PARALLEL_MAKE' yocto/meta-bsp-study || echo none
git diff --check
```

- 추가 확인: 후처리 python 함수의 PBKDF2 계산을 호스트 `python3 -I`로 같은 입력(예: SSID `test`, 비밀번호 `password123`)에 대해 돌린 값과, 호스트에 `wpa_passphrase`가 있으면 그 출력 `psk=`와 비교한다. 없으면 hostapd/wpa_supplicant 문서의 계산식을 근거로 적는다

## 구현 보고 (Codex)
- 변경 요약: 지정된 6개 파일만 수정·생성했다. 템플릿에 라이선스 플래그와 선택적 비밀 설정 include를 추가하고, 이미지에 지정 패키지 6개와 `write_wifi_config` 후처리를 추가했다. init-ifupdown의 wlan0 설정을 교체하고, rc5 S02에서 백그라운드로 장치 대기·IPv4 확인·최대 2회 추가 재시도를 하는 실행 권한 0755의 스크립트와 레시피를 만들었다. 문서는 이 2차 구현 보고만 채웠다.
- 빌드 결과: 미실행(3~5단계에서 사용자 확인)
- checkpatch: 해당 없음(Yocto 메타데이터·셸 스크립트)
- 판단이 필요했던 부분:
  1. **후처리 구분자:** `~/yocto/poky/meta/classes-recipe/rootfs-postcommands.bbclass:23`의 `ROOTFS_POSTPROCESS_COMMAND += "rootfs_update_timestamp "`처럼 함수 이름 뒤에 공백을 두어 `ROOTFS_POSTPROCESS_COMMAND += "write_wifi_config "`로 추가했다. `~/yocto/poky/meta/lib/oe/utils.py:259–267`은 세미콜론을 공백으로 바꾼 뒤 `split()`한 각 함수 이름을 실행한다. `~/yocto/poky/meta/classes-recipe/image.bbclass:207`의 rootfs 태스크는 fakeroot로 실행되고, `~/yocto/poky/meta/lib/oe/rootfs.py:194,226`에서 후처리 목록을 읽어 실행한다. 후처리는 패키지가 설치한 설정 파일을 덮어쓰므로 기존 소유자를 유지하고 `fchmod`로 0600을 지정한다. 호스트 임시 디렉터리 검증에서는 기존 소유자 유지와 0600을 확인했으며, 이미지 안의 root 소유권은 사용자 빌드 후 확인 대상이다.
  2. **bbappend와 interfaces:** `~/yocto/poky/meta/recipes-core/init-ifupdown/init-ifupdown_1.0.bb:13–18,27`은 이미 `file://interfaces`를 받아 `${WORKDIR}/interfaces`를 설치하므로 SRC_URI를 다시 추가하지 않았다. `~/yocto/poky/meta/classes-global/base.bbclass:59–61`의 FILESPATH와 THISDIR 즉시 확장 설명, `~/yocto/poky/meta/classes-global/utils.bbclass:277–292`의 FILESEXTRAPATHS 선행 처리, `~/yocto/meta-raspberrypi/recipes-core/psplash/psplash_%.bbappend:1`의 예를 근거로 `FILESEXTRAPATHS:prepend := "${THISDIR}/files:"`만 사용했다. 원본 `~/yocto/poky/meta/recipes-core/init-ifupdown/init-ifupdown-1.0/interfaces:8–12`의 wlan0 블록만 수정했다. 원본 주석·공백·그 밖의 인터페이스는 바이트 단위로 유지했고 `auto wlan0`은 없다. 변경 줄의 실제 diff는 아래 확인 출력에 붙였다.
  3. **rc5 순서와 INITSCRIPT_PARAMS:** `~/yocto/poky/meta/classes-recipe/update-rc.d.bbclass:31–38`은 `${INITSCRIPT_NAME} ${INITSCRIPT_PARAMS}`를 update-rc.d에 전달한다. `~/yocto/poky/meta/recipes-core/init-ifupdown/init-ifupdown_1.0.bb:10–11`의 `start 01 2 3 4 5 . stop 80 0 6 1 .` 형식을 따라 `start 02 5 . stop 80 0 6 1 .`로 지정했다. networking은 rc5 S01이고, `~/yocto/poky/meta/recipes-core/dropbear/dropbear_2022.83.bb:50–51`은 `defaults 10`이다. 해당 명령 구현은 `~/yocto/poky/meta/recipes-core/update-rc.d/update-rc.d_0.8.bb:9–10`의 SRCREV에 맞춰 `git -C ~/yocto/downloads/git2/git.yoctoproject.org.update-rc.d show b8f950105010270a768aa12245d6abf166346015:update-rc.d`로 읽었다. 이 파일 `62–71`행은 링크 이름을 `${etcd}${lev}.d/${startstop}${nn}${bn}`으로 만들고, `242–260`행은 defaults의 시작 번호와 runlevel 2·3·4·5를, `272–299`행은 start/stop 번호·runlevel·마침표 형식을 처리한다. `~/yocto/poky/meta/recipes-core/sysvinit/sysvinit/rc:142–145,166–173`은 S* 순서로 start를 호출하므로 rc5는 `S01networking` → `S02bsp-study-wifi` → `S10dropbear` 순서다. 실제 rootfs 링크 생성은 미검증이다.
  4. **hostname:** `~/yocto/poky/meta/recipes-core/base-files/base-files_3.0.14.bb:63–69`에서 기본값이 `hostname = "${MACHINE}"`이고, 같은 파일 `113–116`행에서 그 값을 `/etc/hostname`에 쓴다. 템플릿의 MACHINE이 `raspberrypi4-64`이므로 interfaces의 `hostname raspberrypi4-64`와 같다. 실제 이미지의 `/etc/hostname`은 사용자 빌드 후 확인 대상이다.
  5. **PBKDF2와 입력 처리:** `~/yocto/poky/meta/recipes-connectivity/wpa-supplicant/wpa-supplicant/wpa_supplicant.conf:230–239`은 PSK가 32바이트·64자리 hex이고 passphrase가 8~63자이며 wpa_passphrase로 미리 생성할 수 있음을 설명한다. 호스트 `command -v wpa_passphrase`는 출력이 없어 실행 파일 직접 비교는 하지 못했다. 지시서의 대체 검증대로 [hostapd 공식 소스 문서](https://w1.fi/hostapd/devel/config_8c_source.html)의 소스 줄 `474–476`(passphrase·SSID·4096회)와 [wpa_passphrase 소스](https://android.googlesource.com/platform/external/wpa_supplicant_8/+/b4d426d/wpa_supplicant/wpa_passphrase.c)의 `pbkdf2_sha1` 호출(출력 32바이트)을 대조했다. 저장소 레시피의 함수 본문을 추출해 호스트 `python3 -I`에서 가짜 BitBake 데이터와 `/tmp` 임시 rootfs로 실행했다. 지시서의 테스트 입력에 대한 레시피 출력, `hashlib.pbkdf2_hmac("sha1", ..., 4096, 32)` 및 HMAC-SHA1 반복·XOR를 별도 구현한 결과가 일치했다. 실제 SSID·비밀번호·PSK는 사용하거나 출력하지 않았다. SSID는 UTF-8 바이트 hex로 기록하고, 비밀번호 길이는 wpa_passphrase의 바이트 길이 처리에 맞춰 UTF-8 인코딩 후 검사한다. 빈 SSID의 기본 파일 유지, 잘못된 길이의 쓰기 전 거부, 양 끝 길이 8·63 허용, 한글 테스트 SSID 인코딩, 원문 비밀번호 미포함, 비밀 값 로그 미출력도 확인했다.
  6. **Wi-Fi 레시피 형식:** `~/yocto/poky/meta/recipes-core/init-ifupdown/init-ifupdown_1.0.bb:13–18,20–28`을 따라 `SRC_URI = "file://bsp-study-wifi"`, `S = "${WORKDIR}"`, `${WORKDIR}`에서 `${D}${sysconfdir}/init.d`로 설치하는 `install -d`·`install -m 0755` 형식을 사용했다. MIT 체크섬은 `md5sum ~/yocto/poky/meta/files/common-licenses/MIT`로 확인한 `0835ade698e0bcf8506ecda2f7b4f302`다. `RDEPENDS:${PN}`에는 지시된 wpa-supplicant를 넣었다. 스크립트는 20초 장치 대기, ifup 후 각 30초 IPv4 확인, 재시도 전 ifdown, 최대 3회 ifup을 백그라운드 서브셸에서 수행하고 결과를 logger로 남긴다. IPv4 대기 시간은 각 ifup 호출이 반환된 뒤부터 계산한다.
  7. **펌웨어 플래그:** 이제 이미지가 펌웨어를 직접 설치하므로 `LICENSE_FLAGS_ACCEPTED = "synaptics-killswitch"`가 필요하다. 근거는 `~/yocto/meta-raspberrypi/recipes-kernel/linux-firmware-rpidistro/linux-firmware-rpidistro_git.bb:16,64,92–95`의 플래그·패키지·파일 목록이다. `~/yocto/poky/meta/recipes-kernel/wireless-regdb/wireless-regdb_2026.09.03.bb:18–19,26,29–32`에서 static 패키지의 regulatory.db와 서명 파일도 확인했다.

확인 명령은 저장소 루트에서 지시서대로 실행했다. interfaces의 diff는 의도한 차이로 종료 코드 1이며, `git diff --check`는 출력 없이 종료 코드 0이다.

```text
$ find yocto/meta-bsp-study -type f | sort
yocto/meta-bsp-study/conf/layer.conf
yocto/meta-bsp-study/conf/templates/bsp-study/bblayers.conf.sample
yocto/meta-bsp-study/conf/templates/bsp-study/conf-notes.txt
yocto/meta-bsp-study/conf/templates/bsp-study/local.conf.sample
yocto/meta-bsp-study/recipes-connectivity/bsp-study-wifi/bsp-study-wifi.bb
yocto/meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb
yocto/meta-bsp-study/recipes-core/init-ifupdown/files/interfaces
yocto/meta-bsp-study/recipes-core/init-ifupdown/init-ifupdown_%.bbappend
$ diff ~/yocto/poky/meta/recipes-core/init-ifupdown/init-ifupdown-1.0/interfaces \
  yocto/meta-bsp-study/recipes-core/init-ifupdown/files/interfaces
9,11c9
< 	wireless_mode managed
< 	wireless_essid any
< 	wpa-driver wext
---
> 	wpa-driver nl80211
12a11,12
> 	hostname raspberrypi4-64
> 	udhcpc_opts -t 5 -A 5
$ sh -n yocto/meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi && echo sh-ok
sh-ok
$ ls -l yocto/meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi
-rwxr-xr-x 1 nakta nakta 1613 Nov 13 18:42 yocto/meta-bsp-study/recipes-connectivity/bsp-study-wifi/files/bsp-study-wifi
$ grep -rnE 'WIFI_(SSID|PSK)' yocto/meta-bsp-study
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb:23:WIFI_SSID ??= ""
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb:24:WIFI_PSK ??= ""
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb:33:    ssid = d.getVar("WIFI_SSID") or ""
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb:35:        bb.warn("WIFI_SSID가 비어 있어 기본 Wi-Fi 설정을 유지합니다.")
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb:38:    passphrase = (d.getVar("WIFI_PSK") or "").encode("utf-8")
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb:40:        bb.fatal("WIFI_PSK는 UTF-8 기준 8~63바이트여야 합니다.")
$ grep -rnE '/home|~/|\$HOME|BB_NUMBER_THREADS|PARALLEL_MAKE' yocto/meta-bsp-study || echo none
none
$ git diff --check
```

추가 확인 출력(`python3 -I`, 레시피 함수 추출·임시 rootfs·테스트 값만 사용):

```text
empty-SSID: default preserved, warning only: OK
invalid passphrase lengths 0/7/64: rejected before write: OK
passphrase lengths 8/63: accepted: OK
test-input PSK: recipe = python3 -I = independent HMAC-SHA1 implementation: OK
config format, SSID hex, no plaintext passphrase, mode 0600, existing owner retained: OK
wpa_passphrase binary: unavailable; compared algorithm with upstream documentation
UTF-8 SSID encoding and no credential logging: OK
layer file set and untouched first-stage files: OK
interfaces changes confined to wlan0; auto wlan0 absent: OK
```

미검증: BitBake 파싱·빌드, 실제 rootfs의 소유권·rc5 링크·모듈·펌웨어, 보드의 대기·DHCP 재시도·Wi-Fi SSH 연결. `~/yocto/wifi-secret.conf`는 만들거나 읽지 않았고 `~/yocto` 소스도 수정하지 않았다. 금지된 빌드 도구·보드 조작·커밋은 실행하지 않았다. 기존 작업 노트와 1차 보고·리뷰는 유지했다.

## 리뷰 (Claude)
- 결과: 통과(정적 검토. 파싱·빌드·보드 동작은 작업 노트 3~7단계에서 확인)
- 범위: 지시한 6개 파일만 바뀜(`git status`). 1차 파일 중 다른 것·`~/yocto` 소스 변경 없음. 저장소에 SSID·비밀번호 값 없음(`WIFI_*`는 `??= ""`와 `getVar`만)
- 재빌드 판정: `image.bbclass` L124~130이 `ROOTFS_POSTPROCESS_COMMAND`의 값(함수 이름)을 vardeps로 넣고(L144 `do_rootfs[vardeps]`), bitbake가 python 함수 안의 `d.getVar("WIFI_SSID")`·`d.getVar("WIFI_PSK")`를 의존 변수로 잡음. 그래서 비밀번호를 바꾸면 `do_rootfs`가 다시 돎
- PSK 계산: IEEE 802.11i 테스트 벡터(passphrase `password`, SSID `IEEE`)를 호스트 `python3 -I`로 계산한 값이 `f42c6fc5…a12e`로 표준 값과 같음. 레시피의 `pbkdf2_hmac("sha1", passphrase, ssid, 4096, 32)`와 같은 식. 길이 검사를 UTF-8 바이트로 한 판단도 맞음
- `interfaces`: 원본과 비교하면 wlan0의 `wireless_*` 2줄 삭제, `wext`→`nl80211`, `hostname`·`udhcpc_opts` 추가뿐. bbappend는 `FILESEXTRAPATHS`만 두고 원본 `SRC_URI`의 `file://interfaces`를 바꿔치기하는 방식이라 맞음
- 대기 스크립트: 20초 장치 대기 → `ifup` → 30초 IPv4 확인 → `ifdown`/`ifup` 최대 2회 더. 전체가 백그라운드 서브셸이라 `S10dropbear`를 막지 않음. 쓰는 명령 `ip`·`logger`·`ifup`·`ifdown`은 모두 busybox에 켜져 있음(`.config`의 `CONFIG_IP`·`CONFIG_FEATURE_IP_ADDRESS`·`CONFIG_LOGGER`). `INITSCRIPT_PARAMS = "start 02 5 . stop 80 0 6 1 ."`는 init-ifupdown 형식과 같음
- 참고(수정 요구 아님):
  - 스크립트는 rc5 S02에서 시작하고 syslogd는 S20이라, 첫 `logger` 메시지("wlan0 장치 대기")는 syslogd가 뜨기 전에 보내져 사라질 수 있음. 이후 메시지는 `/var/log/messages`에 남음(busybox syslog `DESTINATION=file`이라 `logread` 버퍼는 비어 있음). 노트 7단계 진단에 `grep bsp-study-wifi /var/log/messages`를 넣음
  - 랜선이 없으면 S01 `ifup -a`의 eth0 `udhcpc`가 discover 3회(약 9초)를 보낸 뒤 백그라운드로 넘어가므로, 그만큼 wlan0 시작이 늦어짐. 동작에는 문제없음
  - PBKDF2 대조에 외부 문서를 참고했으나 위 표준 테스트 벡터로 따로 확인되므로 그대로 둠
- 수정 지시: 없음
