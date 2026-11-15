# I2S 마이크 INMP441: ASoC 연결, 코덱 드라이버, alsa-lib 캡처 앱

## 1. 한눈에 보기
- **목적:** INMP441 I2S MEMS 마이크를 ALSA로 녹음하면서 ASoC가 CPU DAI·코덱·machine 세 드라이버를 DT로 묶는 방식을 이해한다. STM32에서 I2S·DMA를 직접 다뤘던 일이 리눅스에서는 어디서 처리되는지 대응시킨다.
- **기능:** 단계별로 하나씩 직접 작성 범위를 넓혔다.
  - A: 기존 rpi overlay `adau7002-simple`로 배선·녹음 확인
  - B: 같은 구조의 overlay [inmp441-overlay.dts](../../examples/mic/inmp441-overlay.dts)를 직접 작성(코덱 `invensense,ics43432`, 카드 `inmp441`)
  - C(14-3): INMP441 사양으로 코덱 드라이버 [inmp441_codec.c](../../examples/mic_codec/inmp441_codec.c)(`nakta,inmp441`) 직접 작성
  - D(14-4): `arecord` 대신 alsa-lib 앱 [mic_capture.c](../../examples/mic_app/mic_capture.c)와 실시간 파형 [mic_plot.py](../../examples/mic_app/mic_plot.py)
- **직접 작성 / 가져다 쓴 것:** overlay 2개, 코덱 드라이버, 캡처 앱·그래프는 직접 작성했다. CPU DAI(`bcm2835-i2s`)·DMA·machine(`simple-audio-card`)·ALSA PCM 코어는 커널 것을 쓴다.
- **결과(실측):** 48kHz·S32_LE·2채널 녹음에서 ch0에만 소리(peak 482,728), 목소리 확인. C단계 레이트가 직접 작성한 값 `[8000 50000]`으로 바뀜. D단계 xrun 횟수가 계산과 일치. 상세는 [6장](#6-트러블슈팅과-한계).
- **범위 밖:** CPU DAI·DMA·machine 직접 작성, mmap·poll 캡처, 재생(DAC), 다른 장치와 동시 사용(→ [18](18-integration.md)).
- **관련 파일:** 작업 노트 [14-mic](../tasks/14-mic.md)·[14-3-mic-codec](../tasks/14-3-mic-codec.md)·[14-4-mic-app](../tasks/14-4-mic-app.md)(각 Codex 지시서), 사양 [hardware.md](../hardware.md#inmp441-i2s-mems-마이크), 배선 [bringup.md](../bringup.md#40핀-한눈에-보기). 커널 소스(`external/linux`): `sound/soc/bcm/bcm2835-i2s.c`, `sound/soc/codecs/ics43432.c`, `sound/soc/generic/simple-card.c`

## 2. 구조

**흐름:** INMP441 SD → GPIO20(PCM_DIN) → `bcm2835-i2s`(I2S 수신 + DMA) → ALSA PCM ring buffer → `/dev/snd/pcmC3D0c` → `arecord`/`mic_capture` → WAV. 클럭은 반대로 Pi가 GPIO18(BCLK)·GPIO19(WS)로 공급한다.

```text
 +-------------+  SCK (BCLK)  <---  +---------------------+          +------------------+
 | INMP441     |  WS  (LRCLK) <---  | bcm2835-i2s         |  DMA     | ALSA PCM core    |
 | I2S MEMS    |                    | CPU DAI, clock      |--------->| ring buffer,     |
 | L/R = GND   |  SD  (DIN)   --->  | provider            |          | periods, xrun    |
 +-------------+                    +----------^----------+          +--------+---------+
                                               | dai link                     |
 +---------------------+            +----------+----------+          +--------v---------+
 | ics43432 codec      |------------| simple-audio-card   |          | arecord          |
 | formats/rates only  |  dai link  | machine: card       |          | mic_capture      |
 +---------------------+            | "inmp441"           |          +------------------+
                                    +---------------------+
```

소리 데이터는 I2S 컨트롤러 → DMA → ring buffer로 가고 코덱 드라이버를 지나지 않는다. 코덱은 형식·레이트를 알리는 역할만 한다.

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 장치 | INMP441 | SCK·WS를 받아 24비트 2의 보수 데이터를 SD로. L/R=GND면 왼쪽 슬롯 |
| DT | `inmp441` overlay | I2S 노드 활성화, 코덱 노드 추가, `/sound`를 simple-audio-card로(형식 i2s, 이름, CPU·코덱 DAI 연결) |
| 드라이버 | `bcm2835-i2s` (CPU DAI) | I2S 레지스터(마스터·프레임·채널), DMA 등록, 시작·정지 |
| 드라이버 | `ics43432`(B) / `inmp441_codec`(C, 직접 작성) | 레지스터 없음. 지원 형식·채널·레이트만 알림 |
| 드라이버 | `simple-audio-card` (machine) | DT대로 CPU DAI와 코덱을 묶어 ALSA 카드 등록 |
| 프레임워크 | ALSA PCM 코어 | hw_params 협상, ring buffer·period, xrun 감지 |
| 사용자 공간 | `arecord` / `mic_capture`(D) | 형식·레이트·period·buffer 요청 → 읽어서 저장. D는 ch0 변환·이득·xrun 복구 |

## 3. 알아야 할 개념

### STM32에서 직접 한 일은 리눅스에서 어디에 있는가
INMP441 전용 드라이버는 없지만 필요한 세 드라이버가 모두 있어 빠진 것은 "이 보드에 이렇게 연결돼 있다"는 DT 기술뿐이었다. 레지스터가 없는 I2S 마이크의 코덱은 같은 InvenSense 계열 `ics43432`로 충분했다.

| STM32에서 직접 한 것 | 리눅스에서 맡는 곳 |
| --- | --- |
| I2S 레지스터(마스터, 프레임, 비트, 극성) | `bcm2835-i2s`의 `hw_params`(L338)·`set_fmt`(L786) |
| 마스터 여부·I2S 형식(CubeMX) | DT `simple-audio-card,format = "i2s"`, 클럭 provider |
| DMA 채널·원형 버퍼·half/full 인터럽트 | ASoC dmaengine PCM(L921) + ALSA ring buffer·period |
| 버퍼 크기·처리 단위 | 앱이 period·buffer를 요청 |
| 시작·정지 | ALSA trigger → `bcm2835_i2s_trigger`(L684) |

### 지원 형식·레이트는 누가 정하는가
`arecord --dump-hw-params`의 범위는 CPU DAI와 코덱이 알린 값의 교집합이다. CPU DAI는 2채널·8~384kHz·S16/S24/S32_LE(`bcm2835-i2s.c` L803)이고, `ics43432`는 `7190~52800`Hz·S24_LE·S32(`ics43432.c` L22)라 결과가 `8000~52800`이었다. C단계에서 직접 작성한 코덱이 `rate_max`를 INMP441 데이터시트 값 50000으로 알리자 `RATE: [8000 50000]`으로 바뀌었고, `arecord -r 52000`은 `set_rate_near`로 50000Hz에 맞춰졌다([inmp441_codec.c#L11](../../examples/mic_codec/inmp441_codec.c#L11)).

### 24비트는 32비트 슬롯 어디에 있는가
채널 슬롯은 32 SCK이고 INMP441은 MSB부터 24비트를 보낸 뒤 남은 8 SCK 동안 SD를 tri-state로 둔다. 그래서 S32_LE의 하위 8비트는 떠 있는 선을 읽은 값이라 0이 아닐 수 있고, 값은 `>> 8`로 상위 24비트만 쓴다. Pi I2S 수신은 2채널 고정(L804)이고 L/R=GND인 마이크는 왼쪽 슬롯에만 데이터를 내므로 ch0에만 소리가 있다.

### 카드는 모든 component가 모여야 만들어진다
simple-audio-card는 DT의 cpu·codec 노드에 등록된 component를 찾는다. 코덱 모듈이 없으면 `-EPROBE_DEFER`(`soc-core.c` L3535)로 `devices_deferred`에 `asoc-simple-card: parse error`가 남는데, 오류가 아니라 대기다. 카드가 만들어지면 ASoC가 각 component 모듈의 참조를 잡으므로(`soc-component.c` L310 `try_module_get`) 카드가 있는 동안 `rmmod inmp441_codec`은 `in use`로 실패하고, `asoc-simple-card/unbind`로 카드를 먼저 내려야 한다.

### 외부 모듈은 왜 자동 적재되지 않았는가
DT 노드가 생기면 uevent의 `MODALIAS=of:N...Cnakta,inmp441`을 udev가 받아 `modprobe`하고, `modprobe`는 `/lib/modules/<버전>/modules.alias`에서 모듈을 찾는다. `~/bsp/modules`의 외부 모듈은 이 표에 없어서 매 부팅 `insmod`가 필요했다. `extra/`에 설치하고 `depmod -a`로 표를 다시 만들자 부팅 6.5초에 자동 적재되어 대기 없이 카드가 만들어졌다.

## 4. 구현 흐름

### 4-1. overlay가 세 드라이버를 묶는다(B단계)
`config.txt`의 `dtoverlay=inmp441`로 [inmp441-overlay.dts](../../examples/mic/inmp441-overlay.dts)가 기본 DTB에 적용된다.

| fragment | 하는 일 | 코드 |
| --- | --- | --- |
| @0 | `i2s_clk_producer`(= `&i2s`)를 `okay`로. 핀 GPIO18~21 ALT0은 기본 DT의 `i2s_pins`에 이미 있음 | [L7](../../examples/mic/inmp441-overlay.dts#L7) |
| @1 | 루트에 코덱 노드 `compatible = "invensense,ics43432"` | [L19](../../examples/mic/inmp441-overlay.dts#L19) |
| @2 | `/sound`를 simple-audio-card로: 형식 i2s, 이름 `inmp441`, cpu `&i2s_clk_producer`, codec `&inmp441_codec` | [L25](../../examples/mic/inmp441-overlay.dts#L25) |
| `__overrides__` | `dtoverlay=inmp441,card-name=...`으로 카드 이름 변경 | [L46](../../examples/mic/inmp441-overlay.dts#L46) |

compatible로 `snd_soc_bcm2835_i2s`·`snd_soc_ics43432`·`snd_soc_simple_card`가 자동 적재되고, simple-card가 dai link를 만들어 카드 `inmp441`(장치 `bcm2835-i2s-ics43432-hifi`)을 등록한다. `arecord -f S32_LE -r 48000 -c 2`가 협상하면 `bcm2835-i2s`가 BCLK(48kHz × 64 = 3.072MHz)·WS를 내보내고 DMA가 ring buffer를 채운다.

### 4-2. 코덱 드라이버만 직접 작성(C단계)
[inmp441-study-overlay.dts](../../examples/mic_codec/inmp441-study-overlay.dts)는 B단계 overlay에서 코덱 compatible `nakta,inmp441`([L19](../../examples/mic_codec/inmp441-study-overlay.dts#L19))과 카드 이름 `inmp441study` 두 줄만 다르다.
- `of_device_id`의 `nakta,inmp441`이 코덱 노드와 매칭된다([L44](../../examples/mic_codec/inmp441_codec.c#L44)).
- probe는 `devm_snd_soc_register_component()` 한 번이다([L31](../../examples/mic_codec/inmp441_codec.c#L31)). 이 등록이 끝나야 대기하던 simple-card가 카드를 만든다. 실패는 `dev_err_probe()`로 돌려줘 `-EPROBE_DEFER`도 조용히 전달된다.
- remove가 없다. devm이 component 등록을 풀어 준다.
- component 드라이버는 비어 있다. `ics43432`의 `endianness`를 넣으면 BE 형식까지 광고되고, `use_pmdown_time`은 재생에만 쓰여 넣지 않았다.

### 4-3. alsa-lib 앱으로 캡처(D단계)
B단계 overlay 그대로, 앱이 `S32_LE`·2채널을 고정하고 레이트·period·buffer를 `snd_pcm_hw_params_set_*_near`로 **요청**한 뒤 실제 값을 다시 읽는다([L170](../../examples/mic_app/mic_capture.c#L170)). 레지스터·DMA 설정은 커널이 하고 앱은 협상 결과만 받는다는 점이 STM32와 다르다.
- `snd_pcm_readi()`는 period가 찰 때까지 잠든다. buffer가 넘치면 `-EPIPE`가 오고 `snd_pcm_recover()`로 다시 시작하며, 넘친 동안의 샘플은 잃는다([L312](../../examples/mic_app/mic_capture.c#L312)).
- ch0만 `>> 8`로 24비트 값을 얻고 정수 이득을 곱해 ±8388607로 자른다([L244](../../examples/mic_app/mic_capture.c#L244)). 1초마다 peak·RMS·clip·xrun을 stderr로 낸다.
- 출력은 WAV(모노 32비트) 또는 `-o -`(int32 원시)이고, `mic_capture -o - | python3 mic_plot.py`로 실시간 파형을 그린다.
- WSL에서 `libasound2-dev:arm64`(multiarch)로 크로스 빌드하고 Pi의 `libasound.so.2`로 실행한다.

48kHz, period 1024(21.3ms)에서 매 읽기 뒤 쉬는 시간(`-s`)을 바꾼 결과(5초):

| 쉬는 시간 | buffer | xrun | 해석 |
| --- | --- | --- | --- |
| 15ms | 4096 (85ms) | 0 | period보다 짧아 따라감 |
| 30ms | 4096 (85ms) | 19 | 초당 약 13,900프레임 부족 → 약 0.29초마다 넘침 |
| 30ms | 16384 (341ms) | 4 | 같은 부족분 → 약 1.18초마다 넘침 |

buffer를 키우면 넘치는 간격만 늘어나고, 계속 느린 소비는 막지 못한다. STM32에서 DMA circular 버퍼를 키워도 처리 루프가 느리면 결국 덮어쓰는 것과 같다.

## 5. 재현

<details><summary>B단계 overlay 녹음 → C단계 코덱 모듈 → D단계 앱 (펼치기)</summary>

선행 조건: 배선은 [bringup](../bringup.md#40핀-한눈에-보기)(VDD 17, GND 39, SCK 12, WS 35, SD 38, L/R → 34), Pi 커널 `6.18.59-v8-bsp-study01`. 전체 단계는 [작업 노트](../tasks/14-mic.md)에 있다.

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic — overlay 빌드·전송
make && make deploy                      # inmp441.dtbo → pi:bsp/overlays/
```

```bash
# [Pi] 백업 후 부팅 시 적용 → 재부팅
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-14
sudo cp ~/bsp/overlays/inmp441.dtbo /boot/firmware/overlays-bsp-study01/
echo 'dtoverlay=inmp441' | sudo tee -a /boot/firmware/config.txt && sudo reboot
```

```bash
# [Pi] 재부팅 후 — 카드·협상 범위·녹음
arecord -l                                         # card N: inmp441 ... bcm2835-i2s-ics43432-hifi
arecord -D hw:CARD=inmp441,DEV=0 --dump-hw-params -d 1 /dev/null 2>&1 | grep -E 'FORMAT|CHANNELS|RATE'
mkdir -p ~/bsp/mic && arecord -D hw:CARD=inmp441,DEV=0 -f S32_LE -r 48000 -c 2 -d 5 ~/bsp/mic/test.wav
```

채널별 peak·RMS 확인용 python 블록은 작업 노트 3단계에 있다. 원복은 `config.txt.bak-14`를 되돌리고 재부팅한다.

```bash
# [Pi] C단계 — 14-3 overlay를 같은 방식으로 적용·재부팅한 뒤(백업 config.txt.bak-14-3)
sudo cat /sys/kernel/debug/devices_deferred        # soc:sound  asoc-simple-card: parse error
sudo insmod ~/bsp/modules/inmp441_codec.ko
arecord -D hw:CARD=inmp441study,DEV=0 --dump-hw-params -d 1 /dev/null 2>&1 | grep RATE   # [8000 50000]
sudo sh -c 'echo soc:sound > /sys/bus/platform/drivers/asoc-simple-card/unbind'          # 카드 먼저
sudo rmmod inmp441_codec
```

자동 적재를 보려면 `sudo install -D -m 644 ~/bsp/modules/inmp441_codec.ko /lib/modules/$(uname -r)/extra/` → `sudo depmod -a` → 재부팅. 원복은 설치 파일 삭제 → `depmod -a` → `config.txt` 원복 → 재부팅.

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic_app — D단계(libasound2-dev:arm64 필요)
make && make deploy                      # mic_capture, mic_plot.py → pi:bsp/apps/
```

```bash
# [Pi] B단계 overlay 적용 상태
~/bsp/apps/mic_capture -d 5 -g 8 -o ~/bsp/mic/d-g8.wav           # 이득 8 녹음
~/bsp/apps/mic_capture -d 5 -s 30 -b 16384 -o /dev/null          # 느린 소비 → xrun 4
~/bsp/apps/mic_capture -d 0 -g 8 -o - | python3 ~/bsp/apps/mic_plot.py   # [Pi 데스크톱] 실시간 파형
```

</details>

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| 녹음 시작 직후 0~150ms 값이 큼 | 녹음마다 Pi가 클럭을 새로 내보내 마이크가 깨어나는 구간 | 레벨 측정에는 앞 150~200ms 제외 |
| 하위 8비트가 0이 아님 | 24비트 뒤 SD tri-state 구간을 읽음 | `>> 8`로 상위 24비트만 사용 |
| `supply IOVDD not found, using dummy regulator`(A) | overlay에 코덱 전원 노드가 없음 | 조치 없음. 마이크는 3.3V 핀에서 직접 전원 |
| C: 재부팅 후 카드 없음, `parse error` | 외부 코덱 모듈이 `/lib/modules`에 없음 | `insmod`하거나 `extra/`에 설치 후 `depmod -a` |
| C: `rmmod: Module inmp441_codec is in use` | 카드가 코덱 모듈 참조를 잡고 있음 | `asoc-simple-card/unbind` 후 `rmmod` |
| C: 코덱 없이 `asoc-simple-card/bind`가 `I/O error` | probe가 `-EPROBE_DEFER`로 끝난 것으로 봄 | 코덱 적재 후 자동으로 다시 probe됨 |
| 녹음 소리가 작음 | 감도 −26 dBFS(94 dB SPL)라 말소리가 최대치의 수 %, 증폭 단계 없음 | D단계 `-g 8`(말소리 peak 2.8만 × 8 ≈ 23만, 한계 838만) |
| D: 그래프 실행 직후 `[XRUN]` 2회 | python이 창을 띄우는 동안 stdin을 안 읽어 파이프가 참 | 창이 뜬 뒤에는 늘지 않음 |
| 재부팅 직후 첫 녹음 `a-quiet`가 ch0·ch1 모두 최대치("뿌직") | ch1까지 최대치라 마이크 소리가 아니라 데이터가 잘못 읽힘. 재부팅 직후 프레임 어긋남·접촉 불량 의심 | 이어진 녹음은 정상, 재현 안 됨. 원인 미확인 |

**현재 버전 검증 (A·B·C 2026-11-11, D 2026-11-12)**

| 항목 | 결과 |
| --- | --- |
| A·B 연결 | DT만으로 세 모듈 자동 적재, 캡처 카드 등록, 원복 후 장치 없음 |
| A 녹음(48kHz, S32_LE) | 소리 냄: ch0 peak 482,728 / RMS 45,308, ch1 0. 조용히 1초 이후 약 2.2만 |
| B 협상 | `8000~52800`Hz, S24_LE·S32_LE |
| C 코덱 | `insmod` 전 `parse error` 대기 → 직후 카드 `inmp441study`, `RATE [8000 50000]`, unbind 후 `rmmod`, `extra/` 설치 후 부팅 6.5초 자동 적재 |
| D 앱 | 협상 값 출력, 이득 8 녹음, xrun 0 / 19 / 4(계산과 일치), Pi 데스크톱 실시간 파형 |

- 미검증: `a-quiet` 이상의 원인, B·C 녹음의 채널별 진폭 수치(청취만 함), D 이득 후 peak 수치, 실행 중 `dtoverlay` 적용·제거, 16k·44.1k 녹음, mmap·poll 캡처.
- 한계: GPIO18~21을 I2S가 점유하므로 모터 PWM은 GPIO12/13(`pwm-2chan` ALT0)으로 옮겼다([bringup](../bringup.md#40핀-한눈에-보기)). 오른쪽 슬롯이 비어 있어 마이크 하나는 모노다.

**더 보기**
- 클럭 방향은 무엇이 정하나 → overlay의 `bitclock-slave`·`frame-slave`는 대상 커널이 읽지 않는다. `snd_soc_daifmt_parse_clock_provider_raw()`(`soc-core.c` L3433)는 `*-master`만 보고, 없으면 코덱이 두 클럭을 받는 쪽(CBC_CFC)이라 Pi가 BCLK·WS를 공급한다
- 스테레오로 받으려면? → 같은 SD 선에 L/R을 VDD로 둔 마이크를 하나 더 달면 오른쪽 슬롯에 들어간다
- A단계 `adau7002`의 범위는? → 2채널·8~96kHz·S16~S32_LE라 교집합이 `8000~96000`이었다. INMP441 사양(WS 7.8~50kHz)에 가까운 범위를 거는 것이 맞는 코덱을 쓰는 의미다
- 휴대폰 음악에 그래프 반응이 약한 이유 → 실내 소음 RMS 2천~5천 대비 신호가 작고 y축이 선형(±838만 고정)이다. dB 눈금은 확인 필요
- 6.6 Yocto 이미지에서 같은 코덱 모듈은? → [18 문서](18-integration.md)에서 레시피로 빌드해 자동 적재, rms 92051.9·xrun 0
