# 12 Yocto 최소 이미지

상태: 완료(8단계 원복 확인은 생략) · 브랜치: `feat/yocto-minimal` · Codex 지시서: [12-yocto-minimal.codex.md](12-yocto-minimal.codex.md) · 학습 문서: [Yocto 최소 이미지](../../yocto/yocto-minimal.md)
진행 장소: **데스크톱** WSL(빌드·Codex)과 데스크톱 Windows(카드 쓰기) → 노트북(Pi SSH 확인)

## 인수인계 (2026-11-13, 노트북 세션에서 작성)
- 현재 상태: 2부 11·13·14·16단계 완료, main에 머지됨. 이 노트와 지시서만 `feat/yocto-minimal`에 커밋·푸시함. 레이어 파일은 아직 없음.
- 결정:
  - 릴리스 scarthgap(5.0 LTS). poky + meta-raspberrypi(scarthgap) + `yocto/meta-bsp-study`. meta-openembedded는 쓰지 않음
  - `MACHINE = "raspberrypi4-64"`, 커널은 meta-raspberrypi 기본 `linux-raspberrypi`(버전은 3단계에서 확인)
  - 이미지는 `bsp-study-image`(`core-image-minimal` + dropbear SSH, 비밀번호 없는 root 로그인). U-Boot·센서 레시피는 넣지 않음. 센서는 18단계
  - 빌드 설정은 레이어 템플릿(`TEMPLATECONF`)으로 만듦. 경로는 `${TOPDIR}`·`##OEROOT##` 기준 상대 경로이고 홈 절대 경로를 쓰지 않음. 빌드 디렉터리는 저장소 밖 `~/yocto`
  - 병렬도는 템플릿에 넣지 않음. 데스크톱(RAM 16GB)은 다른 작업을 같이 하므로 `.wslconfig`를 memory=8GB/swap=8GB로 두고 빌드 디렉터리 `local.conf`에 병렬도 3. RAM 32GB 이상 PC면 memory=24GB, 병렬도는 bitbake 기본값
  - Windows PATH(`/mnt/...`)는 `wsl.conf`를 바꾸지 않고 빌드 셸에서만 export로 뺌(3단계)
  - `ENABLE_UART = "1"`로 시리얼 콘솔을 켜 둠(접속 대비책)
  - 보드 접속은 랜선 없이 집 Wi-Fi로 함. 노트북에서 `ssh root@<IP>`. 대비책은 HDMI·키보드 콘솔에서 수동 연결(7단계), 그다음 USB-시리얼(3.3V) 콘솔
  - Wi-Fi(2차): 이미지에 `kernel-modules`·`linux-firmware-rpidistro-bcm43455`(`LICENSE_FLAGS_ACCEPTED = "synaptics-killswitch"`)·`wireless-regdb-static`·`wpa-supplicant`·`iw`와 wlan0 대기 스크립트를 넣음. SSID·비밀번호는 저장소 밖 `~/yocto/wifi-secret.conf`(`WIFI_SSID`·`WIFI_PSK`)를 `local.conf`에서 `include`하고, 이미지 후처리에서 `/etc/wpa_supplicant.conf`(권한 600)를 만듦. `interfaces`의 wlan0은 `nl80211`·`hostname`·`udhcpc` 재시도 옵션으로 바꿈(init-ifupdown bbappend). systemd·NetworkManager·connman·Bluetooth는 넣지 않음
  - 주의: 이미지에는 비밀번호 원문 대신 PBKDF2로 만든 `psk` 값이 들어가지만, 이 값만으로도 그 Wi-Fi에 접속할 수 있음. 카드·`.img`를 남에게 주지 않고, 카드에 쓴 뒤 `.img`는 지움. 원문은 `~/yocto/wifi-secret.conf`, `bitbake -e` 출력, `build/tmp` 기록에 남음(모두 저장소 밖). 비밀번호는 저장소 파일·커밋·노트 결과 칸에 적지 않음
  - 3·4단계(빌드 셸)는 Windows Terminal의 tmux 세션 `yocto`에서 실행함. tmux 없이 VSCode 통합 터미널에서 돌리면 VSCode를 닫을 때 빌드도 종료됨. WSL 창 하나는 빌드가 끝날 때까지 열어 둠(WSL 창이 모두 닫히면 WSL이 꺼져 tmux도 종료될 수 있음. WSL 3.0.5에서 실제 동작은 확인 필요)
- 다음 할 일: 1~7단계 완료, 8단계 원복 확인은 생략(64GB 카드는 이 단계에서 건드리지 않음). 학습 문서 작성, main 머지. 18단계는 Yocto 기본 커널(6.6.63)에 맞춰 드라이버·overlay를 레시피로 다시 빌드하는 방향
- 노트북에서는 같은 시기에 다른 장치 작업(PWM)을 별도 브랜치로 진행함. 이 브랜치와 파일이 겹치지 않게 함
- 참고: 초안 [T-B02](T-B02-yocto-minimal.md)(내용은 이 노트로 옮김), VEDA `../VEDA/임베디드리눅스/강의자료/임베디드리눅스2.pdf`(kirkstone·raspberrypi5·`runqemu` 흐름)

## 목표
scarthgap과 직접 만든 `meta-bsp-study` 레이어로 Pi 4 최소 이미지를 빌드하고, 새 32GB 카드로 부팅해 Wi-Fi SSH로 확인함. 끝나면 레이어 → `MACHINE` → 이미지 레시피가 커널·DTB·부트 파일·rootfs로 이어지는 과정을 설명할 수 있음.

## 알아둘 개념
1. **레이어와 `TEMPLATECONF`:** poky(OE-core·bitbake) 위에 BSP 레이어(meta-raspberrypi)와 내 레이어(meta-bsp-study)를 `bblayers.conf`로 쌓음. `oe-init-build-env`는 빌드 디렉터리에 conf가 없을 때 `TEMPLATECONF`의 `local.conf.sample`·`bblayers.conf.sample`을 복사함. 그래서 설정이 저장소에 남고 다른 PC에서도 같게 재현됨. 근거: poky `scripts/oe-setup-builddir`
2. **`MACHINE`과 BSP:** `raspberrypi4-64`가 커널 레시피, DTB·overlay 목록, firmware(`rpi-bootfiles`), `config.txt`(`rpi-config`)를 정함. 근거: meta-raspberrypi `conf/machine/raspberrypi4-64.conf`
3. **이미지와 wic:** 이미지 레시피가 rootfs 패키지를 정하고, wic가 부트(FAT)와 rootfs(ext4) 파티션을 묶어 `.wic`를 만듦. 이 파일로 카드 **전체를 덮어씀**. 기존 카드처럼 파일을 추가하는 방식이 아님

VEDA 강의와 흐름은 같음(clone → `oe-init-build-env` → conf → `bitbake` → `deploy/images`). 다른 점: kirkstone → scarthgap, raspberrypi5 → raspberrypi4-64, meta-oe 없음, conf를 직접 고치지 않고 레이어 템플릿으로 만듦, `runqemu` 대신 실제 카드 부팅.

## 백업·복구
- 새 32GB 카드에만 씀. 기존 64GB 카드(Debian + 6.18.59, 장치 작업용)는 건드리지 않음
- 실패하면 전원 분리 → 64GB 카드로 교체(8단계). 기존 카드 백업(노트북 `out/boot-backup-tb01/`)은 그대로 유지
- 카드 쓰기 전 대상 드라이브가 **32GB 카드인지 용량으로 확인**함. 잘못 고르면 다른 디스크를 덮어씀
- 두 카드에 "yocto"/"sensor" 표시

## 진행 순서

### 1. 데스크톱 환경 확인
```bash
# [데스크톱 WSL] ~
lsb_release -ds; uname -r; nproc; free -g; df -h ~
echo "$PATH" | tr ':' '\n' | grep -c ' '      # 공백 있는 경로 수(Windows PATH)
ssh -V
```

확인: Ubuntu 22.04.5, WSL 커널 6.18.58.1, `nproc` 6, `free -g` Mem 7·Swap 8(`.wslconfig` 8GB/8GB 적용), `~` 여유 950G, 공백 경로 15개(3단계에서 제거), OpenSSH 8.9p1, 호스트 패키지 모두 설치됨, `en_US.utf8` 있음 → 통과. 병렬도는 3(RAM 16GB)
- Ubuntu 22.04 또는 24.04: scarthgap 지원 여부는 2단계 후 poky `meta-poky/conf/distro/poky.conf`의 `SANITY_TESTED_DISTROS`로 확인
- `~` 여유 100GB 이상. 빌드는 리눅스 홈에 두고 `/mnt/c`에는 두지 않음
- 공백 있는 경로 수는 3단계 PATH export 뒤 0이 되면 됨
- 메모리: 빌드 전에 Windows RAM을 확인하고 `.wslconfig`를 먼저 적용함. 병렬도는 템플릿에 넣지 않고 3단계 끝에 빌드 디렉터리 `local.conf`에서 정함

```powershell
# [데스크톱 Windows PowerShell]
(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB
```

| Windows RAM | `%UserProfile%\.wslconfig` `[wsl2]` | 병렬도(3단계 끝) |
| --- | --- | --- |
| 16GB | `memory=8GB`, `swap=8GB` | 3 |
| 32GB 이상 | `memory=24GB` | bitbake 기본값(`nproc`) |

```powershell
# [데스크톱 Windows PowerShell] 16GB 예. 첫 줄에 기존 내용이 나오면 덮어쓰기 전에 합칠지 정함
Get-Content $env:USERPROFILE\.wslconfig -ErrorAction SilentlyContinue
Set-Content -Path $env:USERPROFILE\.wslconfig -Encoding ascii -Value "[wsl2]`nmemory=8GB`nswap=8GB"
Get-Content $env:USERPROFILE\.wslconfig
wsl --shutdown
```

메모장으로 만들면 `.wslconfig.txt`로 저장될 수 있어서 PowerShell로 만듦. `wsl --shutdown`은 열린 WSL 창을 모두 닫음.

```bash
# [데스크톱 WSL] ~ (새로 연 셸)
cd ~
free -g
```

16GB 기준 Mem total 7·Swap 8이면 적용됨(8GB 한도에서 커널 몫을 뺀 값). `memory`는 상한이라 WSL이 쉴 때는 Windows가 씀. 동시 컴파일 수는 최대 `BB_NUMBER_THREADS`×`PARALLEL_MAKE`(3×3=9)

호스트 패키지(scarthgap Quick Build 목록, 설치 전 공식 문서와 대조):

```bash
# [데스크톱 WSL]
sudo apt update
sudo apt install -y build-essential chrpath cpio debianutils diffstat file gawk gcc git \
  iputils-ping libacl1 liblz4-tool locales python3 python3-git python3-jinja2 \
  python3-pexpect python3-pip python3-subunit socat texinfo tmux unzip wget xz-utils zstd
sudo locale-gen en_US.UTF-8
```

저장소:

```bash
# [데스크톱 WSL]
mkdir -p ~/bspworkspace && cd ~/bspworkspace
git clone git@github.com:naktaa/bsp-study.git   # 이미 있으면 git -C bsp-study fetch
git -C bsp-study switch feat/yocto-minimal
```

### 2. 소스 받기
```bash
# [데스크톱 WSL]
mkdir -p ~/yocto && cd ~/yocto
git clone -b scarthgap https://git.yoctoproject.org/poky
git clone -b scarthgap https://github.com/agherzan/meta-raspberrypi
git -C poky log -1 --format='%h %cd' --date=short
git -C meta-raspberrypi log -1 --format='%h %cd' --date=short
grep SANITY_TESTED_DISTROS -A8 poky/meta-poky/conf/distro/poky.conf
```

확인: poky `6b7c1066f7`(2026-11-10), meta-raspberrypi `018d444`(2026-10-08), 둘 다 scarthgap. `SANITY_TESTED_DISTROS`에 `ubuntu-22.04` 있음, meta-raspberrypi `LAYERSERIES_COMPAT`에 scarthgap 있음 → 통과

여기까지 끝나면 Codex에 [지시서](12-yocto-minimal.codex.md)를 실행함(`~/yocto` 소스를 읽어 확인하므로 2단계 뒤에 실행).

### 3. 빌드 디렉터리 만들기와 설정 확인
Windows Terminal의 WSL 창에서 tmux 세션을 열고 그 안에서 진행함. `-A`는 세션이 이미 있으면 새로 만들지 않고 들어감.

```bash
# [데스크톱 WSL, Windows Terminal] ~
tmux new -A -s yocto
```

Wi-Fi 비밀 파일(2차, 처음 한 번). 저장소 밖에 권한 600으로 만들고 vim으로 직접 적음(셸 기록에 남지 않음):

```bash
# [데스크톱 WSL] ~/yocto
( umask 077; touch ~/yocto/wifi-secret.conf )
vim ~/yocto/wifi-secret.conf
```

```text
WIFI_SSID = "<SSID>"
WIFI_PSK = "<비밀번호>"
```

```bash
# [데스크톱 WSL] ~/yocto
stat -c '%a' ~/yocto/wifi-secret.conf     # 600
grep -c '^WIFI_' ~/yocto/wifi-secret.conf # 2(내용은 출력하지 않음)
ls ~/yocto                                # wifi-secret.conf~ 같은 백업 파일이 있으면 지움
```

- 값은 큰따옴표로 감쌈. SSID의 한글·공백은 괜찮음. 비밀번호에 `"`·`\`·`$`가 있으면 bitbake 문법과 겹치므로 확인 필요

```bash
# [데스크톱 WSL, tmux yocto] ~/yocto — 빌드 셸에서만 Windows PATH(/mnt/...)를 뺌
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
echo "$PATH" | tr ':' '\n' | grep -c ' '   # 0이면 됨
cd ~/yocto
TEMPLATECONF=$HOME/bspworkspace/bsp-study/yocto/meta-bsp-study/conf/templates/bsp-study \
  source poky/oe-init-build-env build
bitbake-layers show-layers
bitbake -e bsp-study-image | grep -E '^(MACHINE|DISTRO|DL_DIR|SSTATE_DIR|BB_NUMBER_THREADS|PARALLEL_MAKE)='
bitbake -e virtual/kernel | grep -E '^(PN|PV)='
```

확인(1차): 레이어 4개(`core`·`yocto`·`raspberrypi`·`bsp-study`), `MACHINE="raspberrypi4-64"`, `DISTRO="poky"`, `DL_DIR`·`SSTATE_DIR`는 `build/../`, 커널 `linux-raspberrypi` `6.6.63+git` → 통과
- `build/conf`가 이미 있으면 템플릿이 다시 복사되지 않음. 템플릿을 고친 뒤에는 `build/conf`를 지우고 다시 실행함
- 새 셸이나 새 tmux 창에서는 위 PATH export 후 `cd ~/yocto && source poky/oe-init-build-env build`만 다시 실행함
- tmux 안에서 새 창(`Ctrl-b c`)이나 분할 창(`Ctrl-b %`)을 열면 그 창은 PATH export·source가 안 된 상태임. 빌드 명령을 쓰려면 그 창에서 다시 함

병렬도(1단계 표에서 RAM 16GB일 때만):

```bash
# [데스크톱 WSL, tmux yocto] ~/yocto/build (3단계 환경)
cat >> conf/local.conf <<'EOF'
BB_NUMBER_THREADS = "3"
PARALLEL_MAKE = "-j 3"
EOF
bitbake -e bsp-study-image | grep -E '^(BB_NUMBER_THREADS|PARALLEL_MAKE)='
```

확인(1차): `BB_NUMBER_THREADS="3"`, `PARALLEL_MAKE="-j 3"`

Wi-Fi 설정(2차). 1차에서 만든 `build/conf/local.conf`에는 템플릿 변경이 들어가지 않으므로 두 줄을 직접 더함(`build/conf`를 지우고 다시 만들면 병렬도도 다시 넣어야 함):

```bash
# [데스크톱 WSL, tmux yocto] ~/yocto/build (3단계 환경)
cat >> conf/local.conf <<'EOF'
LICENSE_FLAGS_ACCEPTED = "synaptics-killswitch"
include ${TOPDIR}/../wifi-secret.conf
EOF
bitbake-layers show-appends | grep -A1 init-ifupdown
bitbake -e bsp-study-image | grep -E '^(LICENSE_FLAGS_ACCEPTED|WIFI_SSID)='
bitbake -e bsp-study-image | grep -c '^WIFI_PSK=".\+"'   # 1이면 값 있음(값은 출력하지 않음)
bitbake -e bsp-study-image | grep '^IMAGE_INSTALL=' | tr ' "' '\n\n' | grep -E '^(kernel-modules|linux-firmware-rpidistro-bcm43455|wireless-regdb-static|wpa-supplicant|iw|bsp-study-wifi)$'
```

확인(2차): `local.conf` 끝에 병렬도 2줄 + 새 2줄이 한 번씩, `init-ifupdown_1.0.bb`에 bsp-study `init-ifupdown_%.bbappend`, `LICENSE_FLAGS_ACCEPTED="synaptics-killswitch"`, `WIFI_PSK` 개수 1, `IMAGE_INSTALL`에 패키지 6개 → 통과. `WIFI_SSID` 줄과 비밀번호는 결과 칸에 붙이지 않음

### 4. 빌드
3단계와 같은 tmux 창에서 실행함. 새 창이면 3단계 PATH export와 `source`부터 다시 함. 빌드 전에 Windows 설정 → 전원에서 절전 모드가 "안 함"인지 확인함(화면 꺼짐은 상관없음. 절전에 들어가면 WSL도 멈춤).

```bash
# [데스크톱 WSL, tmux yocto] ~/yocto/build (3단계 환경)
time bitbake bsp-study-image
```

확인(1차, 유선): 3776 태스크 모두 성공, `real 127m24s`(병렬도 3). 경고 3개는 원래 주소에서 받기 실패 후 미러에서 받은 것(ca-certificates·squashfs-tools·debianutils, `downloads`에 `.done` 있음). `tmp` 38G, `downloads` 7.3G, `sstate-cache` 3.4G

확인(2차, Wi-Fi): 4642 태스크 중 3748 재사용, 모두 성공, `real 12m59s`(예상 30분~1시간보다 짧음. 새로 빌드한 것은 wpa-supplicant·libnl·dbus·iw·firmware와 rootfs). `NATIVELSBSTRING="universal"`은 uninative 사용 표시

확인(2차 재빌드, SSID 수정 후): 4642 중 4631 재사용, `real 2m27s`(`do_rootfs`·이미지 태스크만 다시 실행)

재빌드가 실패하면 `ERROR:` 줄과 실패한 태스크 로그 경로를 붙임
- 빠져나오기: `Ctrl-b`를 누르고 뗀 뒤 `d`(빌드는 계속됨). 다시 들어가기: `tmux attach -t yocto`
- 메모리 보기(선택): `Ctrl-b`를 누르고 뗀 뒤 `%`로 창을 나누고 `free -h`
- 콘솔 로그는 `~/yocto/build/tmp/log/cooker/raspberrypi4-64/`에 자동 저장됨(poky `bitbake.conf`의 `BB_CONSOLELOG`)

### 5. 산출물 확인
```bash
# [데스크톱 WSL] ~/yocto/build
D=tmp/deploy/images/raspberrypi4-64
ls -lhL $D/bsp-study-image-raspberrypi4-64*.wic.bz2 $D/Image $D/*bcm2711-rpi-4-b.dtb
ls $D/bootfiles/
cat $D/bootfiles/config.txt | grep -v '^#' | grep -v '^$'
```

확인(1차): `.wic.bz2` 28M(풀면 181199872바이트, FAT 130M + ext4 36.8M), `Image` 27M, `bcm2711-rpi-4-b.dtb` 55K, `start4.elf`·`fixup4*.dat`, `config.txt`에 `enable_uart=1` → 통과

Wi-Fi 구성(2차). 비밀번호·`psk` 값은 출력하지 않고 개수·권한만 봄:

```bash
# [데스크톱 WSL] ~/yocto/build
D=tmp/deploy/images/raspberrypi4-64
R=tmp/work/raspberrypi4_64-poky-linux/bsp-study-image/1.0/rootfs
grep -E '^(kernel-modules|linux-firmware-rpidistro-bcm43455|wireless-regdb-static|wpa-supplicant|iw|bsp-study-wifi|init-ifupdown) ' $D/bsp-study-image-raspberrypi4-64.rootfs.manifest
ls $R/lib/firmware/brcm/ | grep 43455; ls -l $R/lib/firmware/cypress/cyfmac43455-sdio.bin
ls $R/lib/firmware/ | grep regulatory
find $R/lib/modules \( -name 'brcmfmac*.ko*' -o -name 'cfg80211.ko*' \)
stat -c '%a %n' $R/etc/wpa_supplicant.conf; grep -c '^[[:space:]]*psk=' $R/etc/wpa_supplicant.conf
grep -o 'ssid=[0-9a-f]*' $R/etc/wpa_supplicant.conf | cut -d= -f2 | xxd -r -p; echo   # 이미지 안 SSID를 글자로(공유기 SSID와 대소문자까지 같은지, psk는 출력 안 함)
grep -A5 '^iface wlan0' $R/etc/network/interfaces
ls $R/etc/rc5.d/
find tmp/work/cortexa72-poky-linux/wpa-supplicant -path '*wpa_supplicant/.config' -exec grep '^CONFIG_DRIVER_NL80211' {} +
```

확인(2차): 패키지 7개(`wpa-supplicant` 2.10, `iw` 6.7 등), firmware `brcmfmac43455-sdio.raspberrypi,4-model-b.*`·`cyfmac43455-sdio.bin`(→ minimal), `regulatory.db`·`.p7s`, `cfg80211`·`brcmfmac`·`brcmfmac-cyw` 모듈, ext 이미지 안 `wpa_supplicant.conf` 0600 root(`debugfs`), `psk=` 1개, wlan0 항목, rc5 `S01networking` → `S02bsp-study-wifi` → `S10dropbear`, `CONFIG_DRIVER_NL80211=y`, `.wic.bz2` 60M(풀면 295278592바이트, `kernel-modules`로 늘어남) → 통과. SSID 수정 후 다시 만든 이미지의 SSID가 공유기 이름과 대소문자까지 같음

확인 항목: 패키지 7개, firmware(`brcmfmac43455-sdio.*`, `cyfmac43455-sdio.bin`), `regulatory.db`·`.p7s`, `brcmfmac.ko.xz`·`brcmfmac-cyw.ko.xz`·`cfg80211.ko.xz`, `600`과 `psk=` 1개, SSID가 공유기 이름과 글자·대소문자까지 같음(결과 칸에는 적지 않음), wlan0 항목(`nl80211`·`hostname`·`udhcpc_opts`), `S02bsp-study-wifi`가 `S10dropbear` 앞, `CONFIG_DRIVER_NL80211=y`

### 6. 카드 쓰기 (데스크톱 Windows)
```bash
# [데스크톱 WSL] ~/yocto/build
WIN_IMG=/mnt/d/bsp-images
mkdir -p $WIN_IMG
bzcat tmp/deploy/images/raspberrypi4-64/bsp-study-image-raspberrypi4-64.rootfs.wic.bz2 > $WIN_IMG/bsp-study-image.img
ls -lh $WIN_IMG/bsp-study-image.img
```

- `.rootfs` 이름이 다르면 5단계 `ls`에 나온 이름을 씀
- 이미지는 Windows에서 열 수 있는 D:(여유 공간이 많은 드라이브)에 풂. 크기는 약 173M(`.wic.bmap`의 `ImageSize`). 카드에 쓴 뒤 지워도 됨(원본 `.wic.bz2`는 `tmp/deploy`에 남음)
- Windows Raspberry Pi Imager에서 장치: Raspberry Pi 4, OS: "Use custom" → `D:\bsp-images\bsp-study-image.img`, 저장소: **32GB 카드(용량 확인)**. OS 커스터마이즈 질문에는 "아니요"
- 쓰기·검증이 끝나면 꺼내고 카드에 "yocto" 표시
- 쓴 뒤 `.img`를 지움(Wi-Fi `psk`가 들어 있음):

```bash
# [데스크톱 WSL]
rm /mnt/d/bsp-images/bsp-study-image.img
ls /mnt/d/bsp-images/
```

### 7. 부팅·Wi-Fi SSH 확인 (노트북)
Pi 전원을 끄고 새 카드로 바꿈 → 랜선 없이 전원. 1~2분 뒤 공유기 관리 페이지의 DHCP 목록에서 새 IP를 찾음. 호스트 이름은 `raspberrypi4-64`(wlan0 `hostname` 옵션으로 보냄). 이름이 안 보이면 MAC으로 찾음(Pi 4는 보통 `dc:a6:32`·`e4:5f:01`로 시작). 노트북은 같은 Wi-Fi에 연결돼 있어야 함.

```bash
# [노트북 WSL]
PI_Y=<공유기에서 찾은 IP>
ssh -o StrictHostKeyChecking=accept-new root@$PI_Y
```

```bash
# [Pi, Yocto]
uname -r
cat /proc/device-tree/model; echo
cat /etc/issue
df -h /
lsmod | grep -E 'brcmfmac|cfg80211'
iw dev wlan0 link | head -n 3
ip addr show wlan0
```

확인: **[붙여 주세요]** 위 출력 전체(`iw` 출력의 SSID 줄은 지워도 됨)
- 3단계 커널 `PV`와 `uname -r`이 같은지, model이 Raspberry Pi 4인지, `/etc/issue`가 Poky 5.0.x인지(core-image-minimal에는 `/etc/os-release`가 없음)
- `df -h /`는 카드 전체가 아니라 이미지 크기만큼임(파티션을 늘리지 않음, 정상)
- `lsmod`에 `brcmfmac`·`cfg80211`, `iw`에 `Connected to`, `ip addr show wlan0`에 `inet`(IPv4)
- Pi 쪽 명령은 busybox라 `ip -br`, `head -3` 같은 짧은 옵션이 없음. `ip addr show`, `head -n 3`을 씀

자동 연결이 안 되면 HDMI·키보드를 연결하고 root로 로그인(비밀번호 없음)해 원인을 나눔:

```bash
# [Pi, Yocto, HDMI 콘솔]
ip link show wlan0                     # 없으면 (a)
dmesg | grep -i brcmfmac | tail -n 5
iw dev wlan0 link                      # Not connected면 (b)
ip addr show wlan0                     # inet 없으면 (c)
ps | grep -E 'wpa_supplicant|udhcpc' | grep -v grep
grep bsp-study-wifi /var/log/messages  # 대기 스크립트 기록(syslog가 버퍼가 아니라 파일로 씀)
iw dev wlan0 scan | grep 'SSID:'       # (b)면 아래 설정 SSID와 글자·대소문자 비교
printf "$(grep -o 'ssid=[0-9a-f]*' /etc/wpa_supplicant.conf | cut -d= -f2 | sed 's/../\\x&/g')\n"   # 설정 SSID(hex → 글자)
```

| 증상 | 원인 후보 | 확인 |
| --- | --- | --- |
| (a) wlan0 없음 | 모듈 미로드, firmware 로드 실패 | `lsmod \| grep brcmfmac`. 비어 있으면 `modprobe brcmfmac` 후 `dmesg`. firmware 오류면 5단계 firmware 줄 확인 |
| (b) wlan0은 있는데 연결 안 됨 | SSID 오타(대소문자 포함)·비밀번호 오류, 공유기가 WPA3 전용, `wpa_supplicant` 미실행 | `iw dev wlan0 scan`의 SSID와 설정 SSID(hex를 글자로)를 비교. `ps`에 `wpa_supplicant`가 있는지, 공유기 보안 방식(WPA2 또는 WPA2/WPA3 혼합) |
| (c) 연결됐는데 IP 없음 | DHCP 응답 없음 | `ps`에 `udhcpc`가 있는지. `udhcpc -i wlan0 -n -q`로 한 번 직접 요청 |

수동 연결. `wpa_passphrase`는 비밀번호를 화면에 보이지 않게 입력받고, 평문 주석 줄(`#psk="..."`)은 지움:

```bash
# [Pi, Yocto, HDMI 콘솔]
ifdown wlan0 2>/dev/null; killall wpa_supplicant udhcpc 2>/dev/null
wpa_passphrase "<SSID>" > /tmp/w.conf      # 비밀번호 입력 후 Enter
sed -i '/#psk=/d' /tmp/w.conf
wpa_supplicant -B -i wlan0 -D nl80211 -c /tmp/w.conf
sleep 5; iw dev wlan0 link
udhcpc -i wlan0 -n -q
ip addr show wlan0
```

연결되면 설정을 저장하고 재부팅해서 자동 연결을 확인함:

```bash
# [Pi, Yocto, HDMI 콘솔]
{ echo 'ctrl_interface=/var/run/wpa_supplicant'; echo 'country=KR'; cat /tmp/w.conf; } > /etc/wpa_supplicant.conf
chmod 600 /etc/wpa_supplicant.conf; rm /tmp/w.conf
reboot
```

재부팅 1~2분 뒤 노트북에서 위 `ssh root@$PI_Y`부터 다시 함. 수동으로 고쳤다면 원인(a~c)과 고친 내용을 결과 칸에 적음(비밀번호 제외). HDMI로도 안 되면 USB-시리얼(3.3V) 콘솔: 어댑터 RX ↔ 물리 8(GPIO14 TXD), TX ↔ 물리 10(GPIO15 RXD), GND ↔ 물리 6, 115200 8N1(`ENABLE_UART = "1"`). 어댑터 보유 여부는 확인 필요. 그래도 안 되면 8단계로 원복하고 멈춤

### 8. 원복
```bash
# [Pi, Yocto]
poweroff
```

전원 분리 → 64GB 카드로 교체 → 전원(Wi-Fi 연결).

```bash
# [노트북 WSL]
ssh pi uname -r
```

확인: **[붙여 주세요]** `6.18.59-v8-bsp-study01`

## 완료 기준
- [x] 데스크톱 환경(Ubuntu·nproc·메모리·디스크)과 poky·meta-raspberrypi 커밋 기록, 지원 호스트 확인
- [x] `bitbake-layers show-layers`에 `bsp-study` 레이어, `MACHINE="raspberrypi4-64"`
- [x] `bitbake bsp-study-image` 성공, `.wic.bz2` 크기·빌드 시간 기록
- [x] 산출물에 커널·DTB·firmware·`config.txt` 존재
- [x] 새 카드에서 랜선 없이 Wi-Fi SSH 접속, `uname -r` = 3단계 커널 버전, model Pi 4, `/etc/issue` Poky
- [x] rootfs에 Wi-Fi firmware·`brcmfmac` 모듈·`wpa_supplicant.conf`(600) 존재(5단계), 재부팅 후에도 자동 연결
- [x] `bootfiles/config.txt`에 `enable_uart=1`(5단계)
- [ ] 64GB 카드로 바꾸면 `6.18.59-v8-bsp-study01` 정상 부팅(확인 생략)

## 결과
대표 출력 몇 줄과 판단만 적는다. 원문 로그는 쌓지 않는다.

- 1 환경: 데스크톱 Ubuntu 22.04.5(WSL2), `nproc` 6, WSL 메모리 8GB·swap 8GB, `~` 여유 950G
- 2 소스: poky `6b7c1066f7`, meta-raspberrypi `018d444`(scarthgap), 22.04는 지원 호스트
- 3 설정: 레이어 4개, `MACHINE="raspberrypi4-64"`, 커널 `linux-raspberrypi` 6.6.63, 병렬도 3. 2차에 라이선스 플래그·`wifi-secret.conf` include 추가
- 4 빌드: 1차(유선) 3776 태스크 `127m24s`, 2차(Wi-Fi) `12m59s`, SSID 수정 후 `2m27s`. 디스크 `tmp` 38G·`downloads` 7.3G·`sstate-cache` 3.4G(1차 기준)
- 5 산출물: `.wic.bz2` 28M → 2차 60M(`kernel-modules` 포함). firmware·규정 DB·`brcmfmac` 모듈·`wpa_supplicant.conf`(0600 root)·rc5 순서 확인
- 7 부팅: 1회차(2차 이미지) wlan0은 생기고 firmware(BCM4345/6)·`brcmfmac`·대기 스크립트·`wpa_supplicant`·`udhcpc` 모두 동작했으나 Not connected(`udhcpc` discover 반복). `iw dev wlan0 scan`의 SSID와 설정 SSID를 비교해 `wifi-secret.conf`의 SSID 대소문자 오류를 찾음 → 수정 후 재빌드·카드 다시 쓰기. `p2p-dev-wlan0` 생성 실패 메시지는 Wi-Fi Direct 관련이라 무시함
  - 2회차(SSID 수정 후 재빌드): `uname -r` `6.6.63-v8`(3단계 `PV` 6.6.63+git과 같음), Raspberry Pi 4 Model B Rev 1.5, Poky 5.0.21, `/dev/root` 135.5M(이미지 크기 그대로). `brcmfmac`·`cfg80211` 로드, `iw` Connected(5GHz, 5220), wlan0 `inet` IPv4 받음 → 노트북에서 랜선 없이 `ssh root@<IP>` 접속 성공
  - 재부팅 후에도 자동 연결·SSH 접속 확인(사용자 확인)
- 8 원복: 확인 생략(사용자 판단). 64GB 카드는 이 단계에서 쓰거나 고치지 않음
- 미검증: HDMI 콘솔 수동 연결 절차(`wpa_passphrase` → 저장·재부팅)와 Pi에서 SSID hex를 글자로 바꾸는 `printf` 명령, USB-시리얼 콘솔, 공유기 DHCP 목록에 `raspberrypi4-64` 이름이 뜨는지, 대기 스크립트의 IPv4 재시도 경로(1회차는 SSID 오류라 재시도가 돌았을 것이나 로그는 보지 않음), WSL 창을 모두 닫았을 때 tmux 빌드가 유지되는지
