# I2S 마이크 INMP441: ASoC 세 드라이버를 DT로 묶기

- [1. 한눈에 보기](#1-한눈에-보기)
- [2. 구조](#2-구조)
- [3. 알아야 할 개념](#3-알아야-할-개념)
- [4. 구현 흐름](#4-구현-흐름)
- [5. 재현](#5-재현)
- [6. 트러블슈팅과 한계](#6-트러블슈팅과-한계)

## 1. 한눈에 보기

- **목적:** INMP441 I2S MEMS 마이크를 커널의 기존 드라이버로 연결해 ALSA로 녹음하고, ASoC가 CPU DAI·코덱·machine 세 드라이버를 DT로 어떻게 묶는지 이해한다. STM32에서 I2S·DMA를 직접 다뤘던 일이 리눅스에서는 어디서 처리되는지 대응시킨다.
- **기능:** A단계는 기존 rpi overlay `adau7002-simple`로 배선과 녹음을 확인했다. B단계는 같은 구조의 overlay [inmp441-overlay.dts](inmp441-overlay.dts)를 직접 써서 코덱을 `invensense,ics43432`로 바꾸고 카드 이름을 `inmp441`로 했다. 드라이버 코드는 쓰지 않았다.
- **결과(사용자 보드 실행):** 두 overlay 모두 DT만으로 세 모듈이 자동 적재되고 캡처 카드가 등록됐다. 48kHz·S32_LE·2채널 녹음에서 ch0(L/R=GND)에만 소리가 실리고 목소리를 귀로 확인했다. B단계 코덱은 레이트를 8000~52800Hz, 형식을 S24_LE·S32_LE로 좁혔다. 원복 후 캡처 장치가 없어졌다.
- **범위:** 코덱·machine 드라이버 작성, alsa-lib 앱 작성, 재생(DAC), 다른 장치와 동시 사용은 하지 않았다.
- **관련 파일:** [inmp441-overlay.dts](inmp441-overlay.dts), [Makefile](Makefile), 작업 노트 [14-mic](../../docs/tasks/14-mic.md)·[Codex 지시서](../../docs/tasks/14-mic.codex.md), 사양 [hardware.md](../../docs/hardware.md#inmp441-i2s-mems-마이크), 배선 [bringup.md](../../docs/bringup.md#40핀-한눈에-보기). 커널 소스(`external/linux`, Git 대상 아님): `sound/soc/bcm/bcm2835-i2s.c`, `sound/soc/codecs/ics43432.c`, `sound/soc/generic/simple-card.c`

## 2. 구조

**흐름:** INMP441 SD → GPIO20(PCM_DIN) → `bcm2835-i2s`(I2S 수신 + DMA) → ALSA PCM ring buffer → `/dev/snd/pcmC3D0c` → `arecord` → WAV. 클럭은 반대로 Pi가 GPIO18(BCLK)·GPIO19(WS)로 마이크에 공급한다.

```text
 +-------------+  SCK (BCLK)  <---  +---------------------+          +------------------+
 | INMP441     |  WS  (LRCLK) <---  | bcm2835-i2s         |  DMA     | ALSA PCM core    |
 | I2S MEMS    |                    | CPU DAI, clock      |--------->| ring buffer,     |
 | L/R = GND   |  SD  (DIN)   --->  | provider            |          | periods, xrun    |
 +-------------+                    +----------^----------+          +--------+---------+
                                               | dai link                     |
 +---------------------+            +----------+----------+          +--------v---------+
 | ics43432 codec      |------------| simple-audio-card   |          | arecord          |
 | formats/rates only  |  dai link  | machine: card       |          | (alsa-lib)       |
 +---------------------+            | "inmp441"           |          +------------------+
                                    +---------------------+
```

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 장치 | INMP441 | SCK·WS를 받아 24비트 2의 보수 데이터를 SD로 출력. L/R=GND면 왼쪽 슬롯 |
| DT | `inmp441` overlay | I2S 노드 활성화, 코덱 노드 추가, `/sound`를 simple-audio-card로 정의(형식 i2s, 이름, CPU·코덱 DAI 연결) |
| 드라이버 | `bcm2835-i2s` (CPU DAI) | I2S 레지스터(마스터 모드·프레임·채널) 설정, DMA 등록, 시작·정지 |
| 드라이버 | `ics43432` (코덱) | 레지스터 없음. 지원 형식·채널·레이트만 알림 |
| 드라이버 | `simple-audio-card` (machine) | DT대로 CPU DAI와 코덱을 묶어 ALSA 카드 등록 |
| 프레임워크 | ALSA PCM 코어 | hw_params 협상, ring buffer·period, xrun 감지, `/dev/snd` 장치 |
| 사용자 공간 | `arecord`(alsa-lib) | 형식·레이트·채널 요청, 읽어서 WAV 저장 |

## 3. 알아야 할 개념

### 왜 드라이버를 새로 쓰지 않았는가

INMP441 전용 드라이버는 없지만 필요한 세 드라이버가 모두 있다. Pi의 I2S는 `bcm2835-i2s`가, 둘을 묶는 일은 범용 `simple-audio-card`가 맡는다. 레지스터가 없는 I2S 마이크의 코덱은 "어떤 형식을 내는가"만 알리면 되므로, 같은 InvenSense 계열 I2S MEMS 마이크용 `ics43432`를 쓴다. 빠진 것은 "이 보드에 이렇게 연결돼 있다"는 DT 기술뿐이었다.

### STM32에서 직접 한 일은 리눅스에서 어디에 있는가

| STM32에서 직접 한 것 | 리눅스에서 맡는 곳 |
| --- | --- |
| I2S 레지스터(마스터, 프레임, 데이터 비트, 극성) | `bcm2835-i2s`의 `hw_params`(L338)·`set_fmt`(L786), `MODE_A` 레지스터(L586) |
| 마스터 여부·I2S 형식 선택(CubeMX) | DT: `simple-audio-card,format = "i2s"`, 클럭 provider 정보 |
| DMA 채널·원형 버퍼·half/full 인터럽트 | ASoC dmaengine PCM(`devm_snd_dmaengine_pcm_register`, L921) + ALSA ring buffer·period |
| 버퍼 크기·처리 단위 | `arecord --buffer-size`·`--period-size`(사용자 공간에서 선택) |
| 시작·정지 | ALSA trigger → `bcm2835_i2s_trigger`(L684) |

### 지원 형식·레이트는 누가 정하는가

`arecord --dump-hw-params`가 보여 주는 범위는 CPU DAI와 코덱이 알린 값의 교집합이다. CPU DAI 캡처는 2채널, 8~384kHz, S16/S24/S32_LE다(`bcm2835-i2s.c` L803~811). A단계 `adau7002`는 2채널·8~96kHz·S16~S32_LE라 결과가 `8000~96000`, B단계 `ics43432`는 데이터시트 기준 `RATE_MIN 7190`·`RATE_MAX 52800`(`ics43432.c` L22~23)과 S24_LE·S32라 결과가 `8000~52800`, S24_LE·S32_LE였다. INMP441 사양(WS 7.8~50kHz)에 가까운 범위를 거는 것이 맞는 코덱을 쓰는 의미다.

### 왜 2채널로 받고 한쪽만 값이 있는가

Pi I2S 수신은 2채널 고정이다(`channels_min = 2`, L804). I2S 한 프레임에는 왼쪽·오른쪽 슬롯이 있고, INMP441은 L/R 핀이 LOW면 왼쪽 슬롯에만 데이터를 내고 오른쪽 슬롯에서는 SD를 놓는다. 그래서 ch0에만 소리가 있고 ch1은 0이다. 같은 SD 선에 L/R을 반대로 둔 마이크를 하나 더 달면 스테레오가 된다.

### 24비트는 32비트 슬롯 어디에 있는가

채널 슬롯은 32 SCK이고 INMP441은 MSB부터 24비트를 보낸 뒤 남은 8 SCK 동안 SD를 tri-state로 둔다. S32_LE로 받으면 상위 24비트가 데이터이고 하위 8비트는 떠 있는 선을 읽은 값이라 0이 아닐 수 있다(실측 `low8_nonzero` > 0). 값은 `>> 8`로 상위 24비트만 쓴다.

### 클럭 방향은 실제로 무엇이 정하는가

overlay에는 기존 rpi overlay를 따라 `bitclock-slave`·`frame-slave`를 적었지만, 대상 커널은 이 속성을 읽지 않는다. `snd_soc_daifmt_parse_clock_provider_raw()`(`soc-core.c` L3433)는 `*-master`만 보고, master 속성이 없으면 코덱이 두 클럭을 받는 쪽(CBC_CFC)으로 정해진다. 결과적으로 Pi가 BCLK·WS를 공급한다(Codex 구현 보고에서 지적, 소스로 확인).

## 4. 구현 흐름

1. **firmware가 DT를 합친다:** `config.txt`의 `dtoverlay=inmp441`로 [inmp441-overlay.dts](inmp441-overlay.dts)가 기본 DTB에 적용된다.
   - [L7](inmp441-overlay.dts#L7) fragment@0: `i2s_clk_producer`(= `&i2s`)를 `okay`로. 핀 GPIO18~21 ALT0은 기본 DT의 `i2s_pins`(`bcm2711-rpi-ds.dtsi` L308)에 이미 있다.
   - [L14](inmp441-overlay.dts#L14) fragment@1: 루트에 코덱 노드 `inmp441-codec`, `compatible = "invensense,ics43432"`([L19](inmp441-overlay.dts#L19)).
   - [L25](inmp441-overlay.dts#L25) fragment@2: 꺼져 있던 `/sound`를 `simple-audio-card`로. 형식 i2s([L29](inmp441-overlay.dts#L29)), 이름 `inmp441`([L30](inmp441-overlay.dts#L30)), cpu는 `&i2s_clk_producer`([L37](inmp441-overlay.dts#L37)), codec은 `&inmp441_codec`([L41](inmp441-overlay.dts#L41)).
   - [L46](inmp441-overlay.dts#L46) `__overrides__`: `dtoverlay=inmp441,card-name=...`으로 카드 이름을 바꿀 수 있다.
2. **커널이 compatible로 드라이버를 붙인다:** `snd_soc_bcm2835_i2s`, `snd_soc_ics43432`, `snd_soc_simple_card`가 자동 적재된다. A단계와 비교하면 코덱 모듈만 `snd_soc_adau7002` → `snd_soc_ics43432`로 바뀌었다.
3. **machine이 카드를 등록한다:** simple-card가 DT의 cpu·codec DAI를 찾아 dai link를 만들고 카드 `inmp441`을 등록한다. 장치 이름 `bcm2835-i2s-ics43432-hifi`가 CPU DAI와 코덱 DAI의 결합을 보여 준다.
4. **녹음:** `arecord -f S32_LE -r 48000 -c 2`가 hw_params를 협상하면 `bcm2835-i2s`가 마스터 모드로 BCLK(48kHz × 64 = 3.072MHz)·WS를 내보내고 DMA가 수신 데이터를 ring buffer에 채운다. 클럭이 시작되면 마이크가 깨어나므로 녹음 앞부분에 짧은 과도 구간이 생긴다.

## 5. 재현

전체 단계와 출력은 [작업 노트](../../docs/tasks/14-mic.md)에 있다. 배선은 [bringup.md 40핀 한눈에 보기](../../docs/bringup.md#40핀-한눈에-보기)(VDD 17, GND 39, SCK 12, WS 35, SD 38, L/R → 34).

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic — overlay 빌드·전송
make
make deploy                       # inmp441.dtbo → pi:bsp/overlays/
```

```bash
# [Pi] 백업 후 부팅 시 적용
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-14
sudo cp ~/bsp/overlays/inmp441.dtbo /boot/firmware/overlays-bsp-study01/
echo 'dtoverlay=inmp441' | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후
lsmod | grep -E '^(snd_soc_bcm2835_i2s|snd_soc_ics43432|snd_soc_simple_card) '
arecord -l                                         # card N: inmp441 ... bcm2835-i2s-ics43432-hifi
arecord -D hw:CARD=inmp441,DEV=0 --dump-hw-params -d 1 /dev/null 2>&1 | grep -E 'FORMAT|CHANNELS|RATE'
mkdir -p ~/bsp/mic
arecord -D hw:CARD=inmp441,DEV=0 -f S32_LE -r 48000 -c 2 -d 5 ~/bsp/mic/test.wav
```

채널별 peak·RMS 확인용 python 블록은 작업 노트 3단계에 있다(`>> 8`로 24비트 환산).

```bash
# [Pi] 원복
sudo cp /boot/firmware/config.txt.bak-14 /boot/firmware/config.txt
sudo reboot
# 재부팅 후 arecord -l 에 캡처 장치 없음
```

**확인한 값 (A단계, 48kHz, S32_LE)**

| 파일 | ch0 peak / RMS | ch1 | 비고 |
| --- | --- | --- | --- |
| 소리 냄 (`a-loud`) | 482,728 / 45,308 | 0 | 최대치 8,388,608의 약 6%, 목소리 확인 |
| 조용히 (`q1`) 0~50ms → 150ms 이후 | 약 49만 → 수천 | 0 | 시작 과도 구간 후 가라앉음 |
| 조용히 (`q1`) 1초 이후 | 약 2.2만 | 0 | 주변 소음 |

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| 녹음 시작 직후 0~150ms 값이 큼 | 녹음마다 Pi가 클럭을 새로 내보내 마이크가 깨어나는 구간 | 듣는 용도는 그대로 사용. 레벨 측정·자동 판단에는 앞 150~200ms 제외 |
| 하위 8비트가 0이 아님 | 24비트 뒤 SD tri-state 구간을 읽음 | `>> 8`로 상위 24비트만 사용 |
| dmesg `supply IOVDD not found, using dummy regulator`(A단계) | overlay에 코덱 전원 노드가 없음 | 조치 없음. 마이크는 3.3V 핀에서 직접 전원 |
| `--dump-hw-params` 레이트 상한이 52800 | `ics43432` 코덱이 데이터시트 범위를 알림 | 정상. 48kHz 이하 사용 |
| 원복 명령 직후 `arecord -l`에 카드가 남아 보임 | 재부팅이 끝나기 전에 실행된 출력 | 재접속 후 다시 확인 |

**한 번 나온 이상:** 재부팅 직후 첫 녹음(`a-quiet`, A단계)에서 0~50ms는 0이었다가 이후 끝까지 ch0·ch1 모두 최대치(8,388,608)였고 "뿌직" 소리로 들렸다. ch1까지 최대치라 마이크 소리가 아니라 데이터가 잘못 읽힌 것이다. 이어서 녹음한 `a-loud`·`q1`·`q2`와 B단계 녹음은 정상이라 재현되지 않았다. 재부팅 직후 첫 캡처의 프레임 어긋남이나 순간 접촉 불량을 의심하지만 원인은 확인하지 않았다.

- **미검증:** `a-quiet` 이상의 원인과 재현 여부, B단계 녹음(`b-loud`)의 채널별 진폭 수치(녹음은 완료, 수치 미확인), 실행 중 `dtoverlay` 적용·제거, 레이트별(16k·44.1k) 녹음, alsa-lib 앱으로 period·buffer 크기와 xrun 관찰.
- **한계:** GPIO18~21을 I2S가 점유하므로 17단계 모터 PWM은 GPIO12/13으로 옮겨야 한다([bringup.md](../../docs/bringup.md#40핀-한눈에-보기)). 오른쪽 슬롯은 비어 있어 마이크 하나는 모노다.
