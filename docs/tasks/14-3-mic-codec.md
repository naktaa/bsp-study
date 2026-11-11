# 14-3 INMP441 코덱 드라이버 작성

상태: 완료 · 브랜치: `feat/mic-codec` · Codex 지시서: [14-3-mic-codec.codex.md](14-3-mic-codec.codex.md)

## 목표
14단계 2에서 빌려 쓴 mainline `ics43432` 코덱을 이 프로젝트에서 작성한 `inmp441` 코덱 드라이버로 바꾸고, 같은 녹음이 되는지와 무엇이 달라지는지 비교함. 끝나면 ASoC에서 코덱 드라이버가 맡는 일(지원 형식·레이트를 알리는 DAI 등록)과, 드라이버가 늦게 올라올 때 카드 등록이 미뤄지는 흐름(probe 지연)을 설명할 수 있음.

- Codex 구현: 코덱 드라이버 C 파일, Kbuild Makefile, 코덱 compatible만 바꾼 overlay. 사용자는 구현된 코드를 아래 '읽어 볼 것'으로 이해하고 보드에서 확인한다
- 프레임워크에 맡기는 것: platform 드라이버 매칭(`of_match_table`), ASoC component·DAI 등록(`devm_snd_soc_register_component`), 카드 구성(simple-audio-card), DMA·버퍼(bcm2835-i2s·ALSA)
- 사용자 확인 추가: 직접 작성한 모듈을 `/lib/modules/<버전>/extra/`에 설치해 mainline 드라이버처럼 부팅 때 자동 적재되는지 확인하고, 원복 때 제거한다(3-1단계)
- 하지 않는 것: CPU DAI(I2S 컨트롤러)·DMA 직접 구현, 커널 config 변경, `modules_install`
- 마무리 때 [mic.md](../../examples/mic/mic.md)에 14-3 절을 추가한다.

## 알아둘 개념
1. **코덱 드라이버의 실제 일:** 레지스터가 없는 I2S 마이크의 코덱은 "캡처 스트림이 어떤 채널·레이트·형식을 지원하는가"를 `struct snd_soc_dai_driver`의 `.capture`(`struct snd_soc_pcm_stream`, `include/sound/soc.h` L598~604)에 적어 등록하는 것이 전부다. ALSA는 이 값과 CPU DAI 값의 교집합으로 `hw_params` 범위를 만든다. 참고: `sound/soc/codecs/ics43432.c`(74줄) 전체.
2. **매칭과 등록 순서:** overlay의 코덱 노드 `compatible`이 드라이버의 `of_match_table`과 같아야 probe된다. probe에서 component를 등록하면, 그 component를 기다리던 simple-audio-card가 카드를 완성한다. 코덱이 아직 없으면 카드는 만들어지지 않고 미뤄진다(probe 지연). 외부 모듈은 `/lib/modules`에 설치하지 않으면 자동 적재되지 않으므로 `insmod` 전후로 이 흐름을 직접 볼 수 있다.
3. **INMP441 값으로 정하기:** [hardware.md](../hardware.md#inmp441-i2s-mems-마이크) 기준 WS 7.8~50kHz, 24비트를 채널당 32 SCK 슬롯에 실음. 따라서 `rate_min 7800`, `rate_max 50000`, 형식 `S24_LE`·`S32_LE`. `ics43432`(7190~52800)와 상한이 달라 `--dump-hw-params`로 내 드라이버 값이 반영됐는지 구별된다.

## 데이터가 들어오는 흐름
직접 작성한 코덱 드라이버는 **등록(A)과 협상(B)에만 참여하고, 실제 소리 데이터가 지나가는 길(C~E)에는 없다.** 데이터는 I2S 컨트롤러 → DMA → ALSA 버퍼로 바로 간다.

```text
 setup (once)                                   data (every sample / period)
 ------------                                   ----------------------------
 DT overlay                                     INMP441 SD --(I2S)--> PCM block RX FIFO
   |                                                                      | DREQ
   v                                                                      v
 bcm2835-i2s probe  (CPU DAI + DMA)              DMA engine copies FIFO -> ring buffer (RAM)
 inmp441_codec probe (codec DAI: caps)                                    | period full: IRQ
   |                                                                      v
   v                                             snd_pcm_period_elapsed() -> wake reader
 simple-audio-card -> ALSA card                                           |
   |                                                                      v
   v                                             arecord / alsa-lib snd_pcm_readi() -> WAV
 open + hw_params: CPU caps AND codec caps
```

| 단계 | 언제 | 누가 무엇을 하나 | 내 코덱 드라이버 |
| --- | --- | --- | --- |
| A. 등록 | 부팅·`insmod` | `bcm2835-i2s`가 CPU DAI와 DMA PCM을 등록(`devm_snd_dmaengine_pcm_register`, `bcm2835-i2s.c` L921). 내 드라이버가 코덱 DAI를 등록하면 기다리던 simple-audio-card가 둘을 묶어 ALSA 카드·`/dev/snd/pcmC?D0c`를 만든다 | **참여.** 없으면 카드가 미뤄짐 |
| B. 열기·협상 | `arecord` 시작 | ALSA가 CPU DAI 능력(2채널, 8~384kHz, S16/S24/S32)과 코덱 능력(내가 적은 1~2채널, 7800~50000Hz, S24/S32)의 교집합을 만든다. 요청(S32_LE, 48kHz, 2채널, period·buffer 크기)이 그 안이면 `bcm2835_i2s_hw_params`(L338)가 PCM 클럭을 BCLK 3.072MHz(48kHz × 64)로, 프레임을 채널당 32비트로 설정하고 DMA 채널을 FIFO 주소·DREQ로 설정한다 | **참여.** 범위만 제공(52kHz 요청은 여기서 거절) |
| C. 시작 | trigger START | ALSA dmaengine 공용 코드가 ring buffer 전체를 도는 **순환(cyclic) DMA**를 걸고(`pcm_dmaengine.c` L164), `bcm2835_i2s_trigger`(L684)가 RX를 켠다. Pi가 BCLK·WS를 내보내기 시작하면 INMP441이 깨어난다 | 없음 |
| D. 샘플마다 | 48,000번/초 | INMP441이 WS 반주기마다 24비트를 MSB부터 SD로 보낸다(L/R=GND라 왼쪽 슬롯만). PCM 블록이 받아 RX FIFO에 쌓고, FIFO가 차면 DREQ로 DMA를 부른다. DMA가 CPU 개입 없이 FIFO → RAM ring buffer로 복사한다 | 없음 |
| E. period마다 | period 크기만큼 찰 때 | DMA 완료 인터럽트 → 콜백이 `snd_pcm_period_elapsed()`(`pcm_dmaengine.c` L147)를 불러 ALSA가 읽기 대기 중인 앱을 깨운다. `arecord`(4단계에서는 직접 쓴 앱)가 `snd_pcm_readi()`로 ring buffer에서 꺼내 WAV에 쓴다. 앱이 늦으면 ring buffer가 넘쳐 xrun | 없음 |
| F. 정지 | trigger STOP | DMA 중지, RX·클럭 정지 → 마이크 절전 | 없음 |
| 해제 | `rmmod` | 내 코덱 DAI가 사라지면 카드도 해제된다 | **참여**(devm이 자동 해제) |

**STM32와 비교:** STM32에서는 C~E를 직접 짰다(I2S 레지스터, DMA circular, half/full 콜백, 버퍼 처리). 여기서는 C~E를 `bcm2835-i2s`와 ALSA dmaengine 공용 코드가 하고, period가 half/full 단위에 해당한다. 4단계 앱에서 period·buffer 크기를 바꾸면 STM32에서 DMA 버퍼 크기를 바꿨을 때처럼 콜백 주기·지연·xrun이 달라진다. 이번 3단계에서 직접 바꾸는 것은 A·B에서 "이 마이크는 이런 데이터를 낸다"고 알리는 부분이다.

## 진행 순서
보드 단계 출력은 **[붙여 주세요]**만 채팅에 붙인다.

### 0. 구현 (Codex)
브랜치 `feat/mic-codec`(생성 완료). Codex에 붙여 넣기:
`docs/tasks/14-3-mic-codec.codex.md를 읽고 AGENTS.md 규칙에 따라 구현 계획을 먼저 설명해줘.`

```text
examples/mic_codec/
  inmp441_codec.c            # 코덱 드라이버
  inmp441-study-overlay.dts  # examples/mic/inmp441-overlay.dts에서 코덱 compatible·카드 이름만 변경
  Makefile                   # 모듈 + overlay + deploy
```

구현이 끝나면 Claude에게 리뷰를 요청한다(`review`).

### 1. 읽어 볼 것 (리뷰 통과 후, 10~20분)
`examples/mic_codec/inmp441_codec.c`에서 아래를 찾아 위 '데이터가 들어오는 흐름'의 A·B와 연결해 본다.

- [ ] `.capture`의 채널·레이트·형식 값 → B단계 협상에서 CPU DAI와 교집합이 되는 값
- [ ] `of_device_id`의 `"nakta,inmp441"` ↔ overlay 코덱 노드 `compatible` → A단계 매칭
- [ ] probe의 `devm_snd_soc_register_component()` → 이 호출이 끝나야 simple-audio-card가 카드를 만든다
- [ ] remove 함수가 없는 이유(devm), component 필드 선택 이유 → Codex 구현 보고 '판단' 항목
- [ ] `ics43432.c`와 비교해 다른 줄이 무엇인지

### 2. 보드: probe 지연 확인 (Pi, 리뷰 통과 후)

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic_codec
make deploy
```

```bash
# [Pi] 백업 후 직접 작성한 overlay를 부팅 시 적용(코덱 모듈은 아직 넣지 않음)
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-14-3
sudo cp ~/bsp/overlays/inmp441-study.dtbo /boot/firmware/overlays-bsp-study01/
echo 'dtoverlay=inmp441-study' | sudo tee -a /boot/firmware/config.txt
diff /boot/firmware/config.txt.bak-14-3 /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후: 코덱이 없어 카드가 미뤄진 상태
arecord -l                                              # 캡처 장치 없음 예상
lsmod | grep -E '^(snd_soc_bcm2835_i2s|snd_soc_simple_card|inmp441_codec) '
sudo cat /sys/kernel/debug/devices_deferred 2>/dev/null # sound 장치가 대기 중인지
```

```bash
# [Pi] 코덱 모듈 적재 → 카드 등록
sudo insmod ~/bsp/modules/inmp441_codec.ko
sudo dmesg | grep -i -E 'inmp441|CODEC' | tail -5
arecord -l                                              # inmp441study 카드 예상
arecord -D hw:CARD=inmp441study,DEV=0 --dump-hw-params -d 1 /dev/null 2>&1 | grep -E 'FORMAT|CHANNELS|RATE'
```

**[붙여 주세요]**

### 3. 보드: 녹음 비교와 해제 (Pi)

```bash
# [Pi]
mkdir -p ~/bsp/mic
arecord -D hw:CARD=inmp441study,DEV=0 -f S32_LE -r 48000 -c 2 -d 5 ~/bsp/mic/c-loud.wav
```

[14-mic 3단계](14-mic.md#3-a단계-녹음과-진폭-확인-pi)의 python 확인을 `~/bsp/mic/c-loud.wav`로 실행한다.

```bash
# [WSL] ~/bspworkspace/bsp-study — wav 파일 가져와서 들어 보기
mkdir -p out/mic
scp 'pi:bsp/mic/*.wav' out/mic/
ls -l out/mic/
explorer.exe "$(wslpath -w out/mic)"     # Windows 탐색기에서 열어 재생
```

`out/`은 Git 대상이 아니라 wav가 커밋되지 않는다. 하나만 받을 때는 `scp pi:bsp/mic/c-loud.wav out/mic/`.

```bash
# [Pi] 범위 밖 레이트 요청: arecord는 가장 가까운 허용 값으로 맞춤
arecord -D hw:CARD=inmp441study,DEV=0 -f S32_LE -r 52000 -c 2 -d 1 /dev/null   # Rate 50000 Hz로 녹음
# 모듈 해제: 카드가 코덱 모듈을 잡고 있으므로 카드를 먼저 내린다
lsmod | grep '^inmp441_codec '                          # 사용 수 1
sudo sh -c 'echo soc:sound > /sys/bus/platform/drivers/asoc-simple-card/unbind'
arecord -l                                              # 카드 사라짐
lsmod | grep '^inmp441_codec '                          # 사용 수 0
sudo rmmod inmp441_codec
# 카드를 다시 붙이면 코덱이 없어 다시 대기
sudo sh -c 'echo soc:sound > /sys/bus/platform/drivers/asoc-simple-card/bind' 2>&1
sudo cat /sys/kernel/debug/devices_deferred
sudo dmesg | tail -5
```

**[붙여 주세요]**

### 3-1. 보드: `/lib/modules` 설치와 자동 적재 (Pi)
지금까지 매 부팅 `insmod`가 필요했던 것은 모듈이 `~/bsp/modules`에 있어 udev가 찾지 못했기 때문이다. 설치 위치에 넣고 `depmod`로 별칭 표(`modules.alias`)를 다시 만들면 부팅 때 흐름이 아래처럼 바뀐다.

`DT 코덱 노드 생성 → uevent MODALIAS=of:N...Cnakta,inmp441 → udev → modprobe → modules.alias 조회 → inmp441_codec 적재 → 카드 등록`

설치는 새 파일 하나 추가와 `depmod` 표 갱신뿐이다. 기존 파일은 덮어쓰지 않고, 되돌릴 때는 파일 삭제 후 `depmod -a`를 다시 실행한다(4단계).

```bash
# [Pi] 3단계에서 rmmod한 상태에서 시작. 같은 이름 파일이 이미 있는지 먼저 확인(없어야 함)
ls -l /lib/modules/$(uname -r)/extra/ 2>&1
sudo install -D -m 644 ~/bsp/modules/inmp441_codec.ko /lib/modules/$(uname -r)/extra/inmp441_codec.ko
sudo depmod -a
grep inmp441 /lib/modules/$(uname -r)/modules.alias    # alias of:N*T*Cnakta,inmp441 inmp441_codec 예상
modinfo -n inmp441_codec                               # .../extra/inmp441_codec.ko
sudo reboot
```

```bash
# [Pi] 재부팅 후: insmod 없이 확인
lsmod | grep '^inmp441_codec '                         # 사용 수 1 (카드가 잡고 있음)
arecord -l                                             # inmp441study 카드
sudo cat /sys/kernel/debug/devices_deferred            # 비어 있음 예상
sudo dmesg | grep -i -E 'inmp441|CODEC|deferred'
```

**[붙여 주세요]**

### 4. 원복 (Pi)

```bash
# [Pi] 설치한 모듈 제거 → 별칭 표 재생성 → config 원복
sudo rm /lib/modules/$(uname -r)/extra/inmp441_codec.ko
sudo depmod -a
modinfo -n inmp441_codec 2>&1                           # not found 예상
sudo cp /boot/firmware/config.txt.bak-14-3 /boot/firmware/config.txt
grep -c 'inmp441' /boot/firmware/config.txt             # 0
sudo reboot
```

### 문제가 생기면
- 재부팅 후 카드가 없음(3-1 전): 외부 모듈은 `/lib/modules`에 설치하지 않아 자동 적재되지 않는다. 매번 `sudo insmod ~/bsp/modules/inmp441_codec.ko`를 먼저 실행한다.
- 3-1 설치 후에도 자동 적재 안 됨: `modules.alias`에 별칭이 있는지, `uname -r`이 모듈 vermagic과 같은지, `cat /sys/bus/platform/devices/inmp441-codec/modalias`가 별칭과 맞는지 본다.
- `insmod` 후에도 카드가 없음: overlay의 코덱 `compatible`과 드라이버 `of_match_table` 문자열이 같은지, dmesg의 probe 오류를 본다.
- `insmod: invalid module format`: vermagic이 실행 커널과 같은지 확인한다(`KDIR`).
- 녹음 값이 모두 0: 14-2 overlay로 돌아가 배선을 다시 확인한다.

## 완료 기준
- [x] Codex 구현, WSL 빌드와 checkpatch 결과 기록
- [x] Claude 리뷰 통과 ('읽어 볼 것' 체크는 노트에 별도 기록 없음)
- [x] 코덱 모듈 적재 전 카드 없음 → `insmod` 후 카드 `inmp441study` 등록
- [x] `--dump-hw-params` 레이트가 `8000~50000`(14-2 `8000~52800`과 다름)
- [x] 녹음이 14-2와 같은 양상(ch0에 소리, ch1 0) — 녹음·청취 확인, 채널별 수치는 미확인
- [x] 52kHz 요청이 50kHz로 맞춰짐, 카드 unbind 후 `rmmod`·카드 제거
- [x] `/lib/modules` 설치 후 재부팅에서 `insmod` 없이 카드 등록, 설치 제거와 원복

## 결과
대표 출력 몇 줄과 판단만 적는다.

**2단계 probe 지연 (overlay만 적용, 코덱 모듈 미적재)**
- `config.txt`: `diff` 결과 `> dtoverlay=inmp441-study` 한 줄
- `arecord -l`: 캡처 장치 없음
- `lsmod`: `snd_soc_simple_card 0`, `snd_soc_bcm2835_i2s 0`(사용 수 0, 카드에 묶이지 않음), `inmp441_codec` 없음
- `devices_deferred`: `soc:sound  asoc-simple-card: parse error` → simple-audio-card가 코덱 노드에 등록된 component를 찾지 못해 `snd_soc_get_dlc()`가 `-EPROBE_DEFER`를 돌려주고(`soc-core.c` L3535), simple-card가 `dev_err_probe(..., "parse error")`(`simple-card.c` L749·L811)로 대기 사유를 남긴 상태. 오류가 아니라 대기

**2단계 `insmod` → 카드 등록**
- dmesg: `inmp441_codec: loading out-of-tree module taints kernel.`, `inmp441-codec inmp441-codec: [CODEC] registered: 7800-50000 Hz, S24_LE/S32_LE`
- `arecord -l`: `card 3: inmp441study ... device 0: bcm2835-i2s-inmp441-hifi inmp441-hifi-0` → 코덱 등록 직후 대기 중이던 simple-card가 다시 probe되어 카드 완성
- `--dump-hw-params`: `FORMAT: S24_LE S32_LE`, `CHANNELS: 2`, `RATE: [8000 50000]` → 14-2(`[8000 52800]`)와 달리 상한이 내 드라이버의 `INMP441_RATE_MAX`로 바뀜. 내 코덱 값이 협상에 반영됨
- 부팅 로그에도 `platform soc:sound: deferred probe pending: asoc-simple-card: parse error`(17.6초)가 남아 있음

**3단계 레이트 범위 밖 요청과 해제 (예상과 달랐던 두 가지)**
- `-r 52000` 요청: 거절되지 않고 `Rate 50000 Hz`로 녹음 → arecord는 허용 범위 안의 가장 가까운 값(`snd_pcm_hw_params_set_rate_near`)을 고름. 범위 밖 요청이 상한으로 맞춰지는 것도 내 드라이버의 `rate_max`가 쓰였다는 근거
- `rmmod inmp441_codec`: `Module inmp441_codec is in use`, 카드 그대로 → 카드가 만들어질 때 ASoC가 각 component의 모듈 참조를 잡는다(`soc-core.c` L1628 `snd_soc_component_module_get_when_probe` → `soc-component.c` L310 `try_module_get`). 카드를 먼저 내려야(`asoc-simple-card/unbind`) 모듈을 뺄 수 있음 → 노트 3단계 명령을 고침

**3단계 재시도 (재부팅 후 다시 적용)**
- 재부팅 후 `insmod` 전: `soc:sound  asoc-simple-card: parse error`로 다시 대기 → 외부 모듈은 부팅마다 자동 적재되지 않음
- `insmod` 직후 `arecord -l`은 비어 있었으나 이어서 실행한 녹음은 성공 → 코덱 등록 뒤 카드가 만들어지기까지 짧은 시간차로 봄
- `c-loud.wav`: 48kHz·S32_LE·2채널 녹음 성공, 사용자가 들어 보고 "소리가 작다". python 진폭 수치는 미전달
- 해제: `inmp441_codec` 사용 수 1 → `asoc-simple-card/unbind` 후 카드 사라짐·사용 수 0 → `rmmod` 오류 없음
- 다시 `bind`: `echo: I/O error` → 코덱이 없어 probe가 대기(`-EPROBE_DEFER`)로 끝나 bind 쓰기가 오류를 돌려준 것으로 봄. 뒤이은 `devices_deferred` 출력은 미전달

**3-1단계 `/lib/modules` 설치와 자동 적재**
- 설치 전 `extra/` 디렉터리 없음 → 덮어쓴 파일 없음. `depmod -a` 후 `modules.alias`에 `alias of:N*T*Cnakta,inmp441 inmp441_codec`(`C*` 포함 2줄), `modinfo -n`이 `extra/inmp441_codec.ko`를 가리킴
- 재부팅 후 `insmod` 없이 `lsmod`에 `inmp441_codec 1`, `arecord -l`에 `card 3: inmp441study`
- dmesg: 6.52초에 `[CODEC] registered`. `devices_deferred`는 비어 있고 `deferred probe pending` 줄도 없음 → 3단계에서 17.6초까지 `parse error`로 대기하던 것과 달리, udev가 MODALIAS로 모듈을 찾아 바로 적재해서 카드 등록이 미뤄지지 않음
- 같은 grep에 걸린 `bcm2835-codec`·`rpi-hevc-dec`는 VPU 영상 코덱(V4L2)이라 오디오와 관계없음

**4단계 원복**
- 설치 파일 삭제·`depmod -a` 후 `modinfo: ERROR: Module inmp441_codec not found.`, `config.txt`의 `inmp441` 0줄
- 재부팅 후 `arecord -l`: 캡처 장치 없음

- 미검증: `c-loud` 채널별 진폭 수치, 수동 `bind` 후 대기 목록
