# 14-4 alsa-lib 캡처 앱과 실시간 파형

상태: 완료 · 브랜치: `feat/mic-app` · Codex 지시서: [14-4-mic-app.codex.md](14-4-mic-app.codex.md)

## 목표
`arecord`가 하던 일(장치 열기, hw_params 협상, 반복 읽기, xrun 복구)을 직접 작성한 alsa-lib 앱으로 하고, period·buffer 크기가 지연·xrun에 어떻게 영향을 주는지 보드에서 관찰함. 끝나면 14-3 노트의 C~E 단계(DMA → ring buffer → period마다 앱을 깨움)를 사용자 공간 쪽에서 설명할 수 있고, 소리가 작은 문제를 소프트웨어 이득으로 보완한 녹음과 실시간 파형을 확인함.

- 드라이버: mainline 그대로. 14-2 overlay `inmp441`(`ics43432` 코덱, 카드 `inmp441`)을 다시 적용한다
- Codex 구현: 캡처 앱 C 파일, 파형 스크립트, Makefile
- 사용자 준비: WSL에 ARM64 alsa-lib 개발 패키지 설치(multiarch, 0단계)
- 하지 않는 것: 드라이버·overlay 수정, mmap·poll 기반 구조, 재생, 필터·FFT
- 마무리 때 [mic.md](../../examples/mic/mic.md)에 14-4 절을 추가한다.

## 알아둘 개념
1. **period·buffer와 블로킹 읽기:** ring buffer(buffer)는 period 여러 개로 나뉘고, DMA가 period 하나를 채울 때마다 인터럽트 → `snd_pcm_period_elapsed()`로 앱을 깨운다. `snd_pcm_readi()`는 요청한 프레임이 모일 때까지 잠든다. STM32 DMA circular의 half/full 콜백이 period 2개짜리 buffer에 해당한다. 48kHz에서 period 1024프레임 = 21.3ms 주기.
2. **xrun:** 앱이 period를 제때 꺼내지 않아 buffer가 가득 차면 캡처가 넘친다(overrun). `snd_pcm_readi()`가 `-EPIPE`를 돌려주고, `snd_pcm_recover()`(내부적으로 `snd_pcm_prepare`)로 다시 시작한다. 그 사이 샘플은 잃는다. buffer가 클수록 앱이 잠깐 늦어도 버티지만 지연이 커지고, 앱이 계속 느리면 buffer 크기와 관계없이 결국 넘친다.
3. **24비트 값과 이득:** S32_LE 프레임의 ch0에서 `>> 8`(산술 시프트)로 24비트 값을 얻는다(하위 8비트는 tri-state 잡음, [mic.md](../../examples/mic/mic.md#24비트는-32비트-슬롯-어디에-있는가)). 소리가 작으면 이 값에 정수 배율을 곱하고 ±8,388,607로 자른다(클리핑). 배율이 크면 잡음도 같이 커진다.

## 진행 순서
보드 단계 출력은 **[붙여 주세요]**만 채팅에 붙인다.

### 0. WSL에 ARM64 alsa-lib 설치 (WSL, Codex 빌드 전)
크로스 컴파일러(glibc 2.35)와 같은 Ubuntu 22.04 arm64 패키지를 받아 쓴다. Pi의 `libasound.so.2`(Debian 13)는 하위 호환이라 실행 때는 Pi 라이브러리가 쓰인다. 기존 저장소 줄은 amd64 전용으로 표시하고 arm64는 ports 저장소에서 받는다.

```bash
# [WSL] 백업
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak-14-4

# 기존 줄을 amd64 전용으로, arm64는 ports 저장소 추가
sudo sed -i 's/^deb http/deb [arch=amd64] http/' /etc/apt/sources.list
sudo tee /etc/apt/sources.list.d/arm64-ports.list >/dev/null <<'EOF'
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-updates main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports jammy-security main restricted universe multiverse
EOF
sudo dpkg --add-architecture arm64
sudo apt update
sudo apt install libasound2-dev:arm64

# 확인
ls /usr/include/alsa/asoundlib.h /usr/lib/aarch64-linux-gnu/libasound.so
dpkg -l | grep -E 'libasound2(-dev)?:arm64'
```

복구: `sudo apt remove libasound2-dev:arm64 libasound2:arm64` → `sudo dpkg --remove-architecture arm64` → `sudo rm /etc/apt/sources.list.d/arm64-ports.list` → `sudo cp /etc/apt/sources.list.bak-14-4 /etc/apt/sources.list` → `sudo apt update`.

**[붙여 주세요]** 확인 두 줄 출력

### 1. 구현 (Codex)
Codex에 붙여 넣기:
`docs/tasks/14-4-mic-app.codex.md를 읽고 AGENTS.md 규칙에 따라 구현 계획을 먼저 설명해줘.`

```text
examples/mic_app/
  mic_capture.c   # alsa-lib 캡처 앱: 협상 값 출력, 1초 통계, xrun 복구, WAV 또는 stdout
  mic_plot.py     # stdin 샘플로 실시간 파형·레벨 그래프 (Pi 데스크톱)
  Makefile        # app + deploy + clean
```

구현이 끝나면 Claude에게 리뷰를 요청한다(`review`).

### 2. 읽어 볼 것 (리뷰 통과 후)
`examples/mic_app/mic_capture.c`에서 아래를 찾아 위 개념과 연결한다.

- [ ] `snd_pcm_open` → `snd_pcm_hw_params_*` 순서와 `_near` 함수가 실제 값을 돌려주는 방식
- [ ] `snd_pcm_readi` 반복과 `-EPIPE` → `snd_pcm_recover` 처리
- [ ] ch0 추출(`>> 8`), 이득과 클리핑
- [ ] WAV 헤더를 끝에서 다시 쓰는 이유(녹음 길이를 끝나야 앎)

### 3. 보드: overlay 다시 적용 (Pi)

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic_app
make deploy
```

```bash
# [Pi] 14-2 dtbo가 배포 폴더에 남아 있는지 확인 → 백업 후 적용
ls -l /boot/firmware/overlays-bsp-study01/inmp441.dtbo   # 없으면 examples/mic에서 make deploy 후 복사
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-14-4
echo 'dtoverlay=inmp441' | sudo tee -a /boot/firmware/config.txt
diff /boot/firmware/config.txt.bak-14-4 /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후
arecord -l                                   # card N: inmp441 ... bcm2835-i2s-ics43432-hifi
```

### 4. 보드: 기본 녹음과 이득 (Pi)

```bash
# [Pi] 말하면서 녹음. 이득 1과 8 비교
mkdir -p ~/bsp/mic
~/bsp/apps/mic_capture -d 5 -o ~/bsp/mic/d-g1.wav
~/bsp/apps/mic_capture -d 5 -g 8 -o ~/bsp/mic/d-g8.wav
```

확인: 시작 시 협상된 rate·period·buffer 출력, 1초마다 peak·RMS·clip·xrun 출력, xrun 0.

```bash
# [WSL] ~/bspworkspace/bsp-study — 가져와서 들어 보기
mkdir -p out/mic
scp 'pi:bsp/mic/d-*.wav' out/mic/
explorer.exe "$(wslpath -w out/mic)"
```

**[붙여 주세요]** 두 실행의 출력, 들어 본 소감(g8이 더 크게 들리는지)

### 5. 보드: period·buffer와 xrun (Pi)
`-s`는 매 읽기 뒤에 쉬는 시간이다. 48kHz에서 period 1024프레임은 21.3ms마다 하나씩 쌓이므로, 쉬는 시간이 이보다 짧으면 앱이 따라가고 길면 계속 뒤처진다. buffer는 일시적인 지연만 흡수하고, 계속 느린 소비는 막지 못한다.

```bash
# [Pi] (a) 지연 15ms < period 21.3ms → 따라감
~/bsp/apps/mic_capture -d 5 -s 15 -o /dev/null
# (b) 지연 30ms > period → 계속 뒤처져 xrun 반복
~/bsp/apps/mic_capture -d 5 -s 30 -o /dev/null
# (c) (b)와 같고 buffer만 16384(≈ 341ms) → 첫 xrun이 늦어질 뿐 결국 발생
~/bsp/apps/mic_capture -d 5 -s 30 -b 16384 -o /dev/null
# (d) period 256, buffer 1024, 지연 없음 → 짧은 주기로 정상 동작
~/bsp/apps/mic_capture -d 5 -p 256 -b 1024 -o /dev/null
```

확인: (a) xrun 0, (b) xrun이 늘고 복구 후 계속 동작, (c) (b)보다 첫 `[XRUN]`이 늦고 횟수가 적음, (d) 협상 값과 xrun 0. 요청 값과 협상 값이 다르면 그대로 적는다.

**[붙여 주세요]**

### 6. 보드: 실시간 파형 (Pi 데스크톱)
VNC 등으로 Pi 데스크톱 터미널에서 실행한다(`ssh` 셸에서는 창이 뜨지 않는다).

```bash
# [Pi 데스크톱 터미널]
python3 -c 'import matplotlib; print(matplotlib.__version__)'   # 없으면 sudo apt install python3-matplotlib
~/bsp/apps/mic_capture -d 0 -g 8 -o - | python3 ~/bsp/apps/mic_plot.py
```

확인: 말하거나 손뼉을 치면 파형과 레벨이 바로 반응함. 창을 닫거나 Ctrl+C로 둘 다 끝남.

**[붙여 주세요]** 반응 여부(스크린샷은 필요 없음), 터미널 출력

### 7. 원복 (Pi)

```bash
# [Pi]
sudo cp /boot/firmware/config.txt.bak-14-4 /boot/firmware/config.txt
grep -c 'inmp441' /boot/firmware/config.txt             # 0
sudo reboot
# 재부팅 후 arecord -l 에 캡처 장치 없음
```

WSL의 arm64 alsa-lib은 다음 앱에도 쓰므로 그대로 둔다(제거는 0단계 복구).

### 문제가 생기면
- 빌드 때 `alsa/asoundlib.h: No such file`: 0단계 설치 확인. 크로스 gcc가 `/usr/include`를 찾지 못하면 Codex 구현 보고의 Makefile 경로를 본다.
- Pi에서 `error while loading shared libraries: libasound.so.2`: `sudo apt install libasound2t64` 또는 `libasound2`(Debian 13 패키지 이름 확인 필요).
- `Device or resource busy`: 다른 `arecord`·앱이 같은 장치를 열고 있다.
- 그래프 창이 안 뜸: `ssh` 셸이 아닌 데스크톱 터미널에서 실행한다.

## 완료 기준
- [x] 0단계 arm64 alsa-lib 설치
- [x] Codex 구현, WSL 빌드·`file`·`readelf` 확인, 리뷰 통과
- [x] 협상된 rate·period·buffer 출력, 5초 녹음 xrun 0, WAV 재생됨
- [x] 이득 8 녹음이 이득 1보다 크게 들림 (청취 확인, 이득 후 수치는 미출력)
- [x] period보다 짧은 지연은 xrun 0, 긴 지연은 xrun 반복·복구 후 계속 동작, 큰 buffer는 첫 xrun만 늦춤
- [x] Pi 데스크톱에서 실시간 파형이 소리에 반응
- [x] 원복 후 캡처 장치 없음

## 결과
대표 출력 몇 줄과 판단만 적는다.

- 협상: `actual rate=48000 period=1024 (21.333 ms) buffer=4096 (85.333 ms)` — 요청 그대로
- 이득: g1·g8 모두 xrun 0·clip 0, g8이 확실히 크게 들림. `[LEVEL]`은 이득 전 값이라 수치는 비슷함. 말소리 peak 2.8만 × 8 ≈ 23만으로 한계(838만)까지 여유 있음
- xrun (5초):

| 실험 | buffer | xrun | 계산 |
| --- | --- | --- | --- |
| (a) `-s 15` | 4096 | 0 | 쉬는 시간 < period 21.3ms |
| (b) `-s 30` | 4096 | 19 | 부족분 ≈ 13,900fps → 약 0.29초마다 넘침 → 약 17회 |
| (c) `-s 30 -b 16384` | 16384 | 4 | 약 1.18초마다 넘침 → 약 4회 |
| (d) `-p 256 -b 1024` | 1024 | 0 | period 5.333ms |

- 판단: buffer는 넘치는 간격만 늘리고 계속 느린 소비는 막지 못함
- 파형: 목소리·박수에 반응. 시작 직후 xrun 2회는 python이 창을 띄우는 동안 파이프가 찬 것으로 봄. 휴대폰 음악에는 반응이 약함 → 실내 소음 RMS 2천~5천 대비 신호가 작고, 선형 y축(파형 ±838만 고정, 레벨은 10초 최댓값 기준)이라 작은 변화가 안 보이는 것으로 봄
- 원복: `config.txt`의 `inmp441` 0줄, 재부팅 후 캡처 장치 없음
- 미검증: 이득 후 peak 수치, 휴대폰을 가까이 둔 레벨 변화
