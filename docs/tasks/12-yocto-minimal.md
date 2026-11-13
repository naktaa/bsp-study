# 12 Yocto 최소 이미지

상태: 계획 · 브랜치: `feat/yocto-minimal` · Codex 지시서: [12-yocto-minimal.codex.md](12-yocto-minimal.codex.md)
진행 장소: **데스크톱** WSL(빌드·Codex)과 데스크톱 Windows(카드 쓰기) → 노트북(Pi SSH 확인)

## 인수인계 (2026-11-13, 노트북 세션에서 작성)
- 현재 상태: 2부 11·13·14·16단계 완료, main에 머지됨. 이 노트와 지시서만 `feat/yocto-minimal`에 커밋·푸시함. 레이어 파일은 아직 없음.
- 결정:
  - 릴리스 scarthgap(5.0 LTS). poky + meta-raspberrypi(scarthgap) + `yocto/meta-bsp-study`. meta-openembedded는 쓰지 않음
  - `MACHINE = "raspberrypi4-64"`, 커널은 meta-raspberrypi 기본 `linux-raspberrypi`(버전은 3단계에서 확인)
  - 이미지는 `bsp-study-image`(`core-image-minimal` + dropbear SSH, 비밀번호 없는 root 로그인). U-Boot·Wi-Fi·센서 레시피는 넣지 않음. 센서는 18단계
  - 빌드 설정은 레이어 템플릿(`TEMPLATECONF`)으로 만듦. 경로는 `${TOPDIR}`·`##OEROOT##` 기준 상대 경로이고 홈 절대 경로를 쓰지 않음. 빌드 디렉터리는 저장소 밖 `~/yocto`
  - 병렬도는 템플릿에 넣지 않음. 데스크톱(RAM 16GB)은 다른 작업을 같이 하므로 `.wslconfig`를 memory=8GB/swap=8GB로 두고 빌드 디렉터리 `local.conf`에 병렬도 3. RAM 32GB 이상 PC면 memory=24GB, 병렬도는 bitbake 기본값
  - Windows PATH(`/mnt/...`)는 `wsl.conf`를 바꾸지 않고 빌드 셸에서만 export로 뺌(3단계)
  - `ENABLE_UART = "1"`로 시리얼 콘솔을 켜 둠(접속 대비책)
  - 보드 접속은 Pi를 공유기에 랜선으로 연결하고 노트북에서 `ssh root@<IP>`. 대비책은 HDMI·키보드, USB-시리얼(3.3V) 콘솔
- 다음 할 일: 데스크톱에서 저장소를 받고 `feat/yocto-minimal`로 전환 → 1·2단계 → Codex 지시서 실행 → 3단계부터
- 노트북에서는 같은 시기에 다른 장치 작업(PWM)을 별도 브랜치로 진행함. 이 브랜치와 파일이 겹치지 않게 함
- 참고: 초안 [T-B02](T-B02-yocto-minimal.md)(내용은 이 노트로 옮김), VEDA `../VEDA/임베디드리눅스/강의자료/임베디드리눅스2.pdf`(kirkstone·raspberrypi5·`runqemu` 흐름)

## 목표
scarthgap과 직접 만든 `meta-bsp-study` 레이어로 Pi 4 최소 이미지를 빌드하고, 새 32GB 카드로 부팅해 SSH로 확인함. 끝나면 레이어 → `MACHINE` → 이미지 레시피가 커널·DTB·부트 파일·rootfs로 이어지는 과정을 설명할 수 있음.

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

확인: **[붙여 주세요]**
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
  python3-pexpect python3-pip python3-subunit socat texinfo unzip wget xz-utils zstd
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

확인: **[붙여 주세요]** 두 커밋, 지원 호스트 목록에 1단계 Ubuntu가 있는지

여기까지 끝나면 Codex에 [지시서](12-yocto-minimal.codex.md)를 실행함(`~/yocto` 소스를 읽어 확인하므로 2단계 뒤에 실행).

### 3. 빌드 디렉터리 만들기와 설정 확인
```bash
# [데스크톱 WSL] ~/yocto — 빌드 셸에서만 Windows PATH(/mnt/...)를 뺌
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v '^/mnt/' | paste -sd:)
echo "$PATH" | tr ':' '\n' | grep -c ' '   # 0이면 됨
cd ~/yocto
TEMPLATECONF=$HOME/bspworkspace/bsp-study/yocto/meta-bsp-study/conf/templates/bsp-study \
  source poky/oe-init-build-env build
bitbake-layers show-layers
bitbake -e bsp-study-image | grep -E '^(MACHINE|DISTRO|DL_DIR|SSTATE_DIR|BB_NUMBER_THREADS|PARALLEL_MAKE)='
bitbake -e virtual/kernel | grep -E '^(PN|PV)='
```

확인: **[붙여 주세요]** 레이어 4개(`core`·`yocto`·`raspberrypi`·`bsp-study`), `MACHINE="raspberrypi4-64"`, 커널 `PN`·`PV`
- `build/conf`가 이미 있으면 템플릿이 다시 복사되지 않음. 템플릿을 고친 뒤에는 `build/conf`를 지우고 다시 실행함
- 새 셸에서는 위 PATH export 후 `cd ~/yocto && source poky/oe-init-build-env build`만 다시 실행함

병렬도(1단계 표에서 RAM 16GB일 때만):

```bash
# [데스크톱 WSL] ~/yocto/build (3단계 환경)
cat >> conf/local.conf <<'EOF'
BB_NUMBER_THREADS = "3"
PARALLEL_MAKE = "-j 3"
EOF
bitbake -e bsp-study-image | grep -E '^(BB_NUMBER_THREADS|PARALLEL_MAKE)='
```

### 4. 빌드
3단계와 같은 셸에서 실행함. 새 셸이면 3단계 PATH export와 `source`부터 다시 함.

```bash
# [데스크톱 WSL] ~/yocto/build (3단계 환경)
time bitbake bsp-study-image
```

확인: **[붙여 주세요]** 마지막 요약 몇 줄(`Tasks Summary`)과 `time` 결과. 첫 빌드는 몇 시간 걸림. 셸을 닫으면 중단되므로 끝날 때까지 창을 열어 둠. 실패하면 `ERROR:` 줄과 실패한 태스크 로그 경로를 붙임

### 5. 산출물 확인
```bash
# [데스크톱 WSL] ~/yocto/build
D=tmp/deploy/images/raspberrypi4-64
ls -lhL $D/bsp-study-image-raspberrypi4-64*.wic.bz2 $D/Image $D/*bcm2711-rpi-4-b.dtb
ls $D/bootfiles/
cat $D/bootfiles/config.txt | grep -v '^#' | grep -v '^$'
```

확인: **[붙여 주세요]** `.wic.bz2` 크기, 커널 `Image`·DTB 존재, `bootfiles`의 firmware(`start4.elf` 등)·`config.txt` 주요 줄

### 6. 카드 쓰기 (데스크톱 Windows)
```bash
# [데스크톱 WSL] ~/yocto/build
WIN_DL=/mnt/c/Users/<Windows 사용자>/Downloads
bzcat tmp/deploy/images/raspberrypi4-64/bsp-study-image-raspberrypi4-64.rootfs.wic.bz2 > $WIN_DL/bsp-study-image.img
ls -lh $WIN_DL/bsp-study-image.img
```

- `.rootfs` 이름이 다르면 5단계 `ls`에 나온 이름을 씀
- Windows Raspberry Pi Imager에서 장치: Raspberry Pi 4, OS: "Use custom" → `bsp-study-image.img`, 저장소: **32GB 카드(용량 확인)**. OS 커스터마이즈 질문에는 "아니요"
- 쓰기·검증이 끝나면 꺼내고 카드에 "yocto" 표시

### 7. 부팅·SSH 확인 (노트북)
Pi 전원을 끄고 새 카드로 바꿈 → 공유기에 랜선 연결 → 전원. 1~2분 뒤 공유기 관리 페이지의 DHCP 목록에서 새 IP를 찾음(호스트 이름 `raspberrypi4-64`).

```bash
# [노트북 WSL]
PI_Y=<공유기에서 찾은 IP>
ssh -o StrictHostKeyChecking=accept-new root@$PI_Y
```

```bash
# [Pi, Yocto]
uname -r
cat /proc/device-tree/model; echo
grep -E '^(NAME|VERSION)=' /etc/os-release
df -h /
ls /lib/modules/$(uname -r) | head; lsmod | head
ip -br addr
```

확인: **[붙여 주세요]** 위 출력 전체
- 3단계 커널 `PV`와 `uname -r`이 같은지, model이 Raspberry Pi 4인지, os-release가 Poky scarthgap인지
- `df -h /`는 카드 전체가 아니라 이미지 크기만큼임(파티션을 늘리지 않음, 정상)
- IP가 안 보이면 아래 중 가능한 방법으로 root 로그인 → `ip addr`로 유선 DHCP 여부 확인 후 붙임. 둘 다 없으면 8단계로 원복하고 멈춤
  - HDMI·키보드
  - USB-시리얼(3.3V 레벨) 어댑터: 어댑터 RX ↔ 물리 8(GPIO14 TXD), TX ↔ 물리 10(GPIO15 RXD), GND ↔ 물리 6. 노트북·데스크톱에서 115200 8N1로 접속(`ENABLE_UART = "1"`이 켜 둠). 어댑터 보유 여부는 확인 필요
- `lsmod`가 비어 있어도 정상임. core-image-minimal은 모듈을 자동으로 많이 올리지 않음. 모듈 파일이 rootfs에 있는지는 `ls /lib/modules` 결과로만 기록함

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
- [ ] 데스크톱 환경(Ubuntu·nproc·메모리·디스크)과 poky·meta-raspberrypi 커밋 기록, 지원 호스트 확인
- [ ] `bitbake-layers show-layers`에 `bsp-study` 레이어, `MACHINE="raspberrypi4-64"`
- [ ] `bitbake bsp-study-image` 성공, `.wic.bz2` 크기·빌드 시간 기록
- [ ] 산출물에 커널·DTB·firmware·`config.txt` 존재
- [ ] 새 카드에서 SSH 접속, `uname -r` = 3단계 커널 버전, model Pi 4, os-release Poky
- [ ] `bootfiles/config.txt`에 `enable_uart=1`(5단계). core-image-minimal 기반이라 `lsmod`가 비어 있어도 정상으로 봄
- [ ] 64GB 카드로 바꾸면 `6.18.59-v8-bsp-study01` 정상 부팅

## 결과
대표 출력 몇 줄과 판단만 적는다. 원문 로그는 쌓지 않는다.

- 1 환경:
- 2 소스:
- 3 설정:
- 4 빌드:
- 5 산출물:
- 7 부팅:
- 8 원복:
- 미검증:
