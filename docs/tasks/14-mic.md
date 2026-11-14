# 14 I2S 마이크 INMP441

상태: 완료 · 브랜치: `feat/mic` · Codex 지시서: [14-mic.codex.md](14-mic.codex.md) (B단계 overlay)

## 목표
INMP441 I2S MEMS 마이크 모듈을 Pi 4의 I2S 입력에 연결하고 ALSA 캡처 장치로 녹음함. 기존 드라이버 세 개(CPU DAI `bcm2835-i2s`, 코덱, machine `simple-audio-card`)를 DT로 묶는 구조를 설명할 수 있게 함. A단계는 기존 `adau7002-simple` overlay로 배선·녹음을 먼저 확인하고, B단계는 INMP441용 overlay를 직접 작성해 같은 결과를 확인함.

- 보드에서 바뀌는 것: 배선, `config.txt`의 `dtoverlay` 한 줄(백업 후 원복), B단계 dtbo를 `/boot/firmware/overlays-bsp-study01/`에 추가
- 하지 않는 것: 코덱 드라이버 작성, 커널 config 변경·재빌드, 재생(DAC), 다른 장치와 동시 사용 확인(18단계)
- 마무리 때 학습 문서 `docs/learning/14-mic.md`를 만든다.

## 알아둘 개념
1. **ASoC 세 부분:** CPU DAI `bcm2835-i2s`(`sound/soc/bcm/bcm2835-i2s.c`)가 I2S 레지스터와 DMA를 맡고, 코덱 드라이버는 마이크가 내는 형식만 알리며, machine `simple-audio-card`(`sound/soc/generic/simple-card.c`)가 DT에 적힌 대로 둘을 묶어 ALSA 카드를 만든다. INMP441 전용 드라이버는 없고 빠진 것은 DT 기술뿐이다.
2. **STM32와의 대응:** STM32에서 직접 한 I2S 레지스터 설정은 `bcm2835_i2s_hw_params()`(L338)·`set_fmt`(L786)가, DMA 원형 버퍼와 half/full 인터럽트는 ASoC dmaengine PCM(L921 `devm_snd_dmaengine_pcm_register`)과 ALSA ring buffer·period가 맡는다. 사용자는 `arecord`의 형식·버퍼·period 크기만 고른다.
3. **클럭 마스터와 채널:** INMP441은 BCLK·WS를 받기만 하므로 Pi가 클럭 마스터다(overlay의 `bitclock-slave`·`frame-slave` = 코덱). Pi I2S 캡처는 2채널 고정(`bcm2835-i2s.c` L804)이라 스테레오로 받고, L/R=GND면 왼쪽 슬롯에만 소리가 실린다. 24비트 샘플은 채널당 32 SCK 슬롯에 MSB부터 실린다([hardware.md](../hardware.md#inmp441-i2s-mems-마이크)). 하위 8비트가 0인지 3단계에서 확인한다.

## 진행 순서
출력은 **[붙여 주세요]** 단계만 채팅에 붙인다. 모든 Pi 명령은 `ssh pi`로 접속한 셸에서 실행한다.

### 시작 전 확인한 것 (Claude 읽기 확인)
| 항목 | 값 |
| --- | --- |
| config | `SND_BCM2835_SOC_I2S=m`, `SND_SOC_ICS43432=m`, `SND_SOC_ADAU7002=m`, `SND_SOC_DMIC=m`, `SND_SIMPLE_CARD=m` |
| I2S 핀 | `i2s_pins` = GPIO18·19·20·21, ALT0 (`bcm2711-rpi-ds.dtsi` L308) |
| 기존 overlay | `adau7002-simple`: `i2s_clk_producer`를 켜고 `/sound`를 simple-audio-card(형식 i2s, 코덱이 클럭 slave)로 만듦. `dtparam=i2s=on` 없이도 I2S를 켬. 배포 폴더에 `adau7002-simple.dtbo` 있음 |
| 코덱 형식 | `ics43432`: S24_LE·S32, 1~2채널, 연속 레이트. `adau7002`: 2채널, 8~96kHz, S16~S32_LE |
| CPU DAI 캡처 | 2채널, 8~384kHz, S16_LE·S24_LE·S32_LE |
| Pi 현재 | `arecord -l` 캡처 장치 없음, `config.txt`의 `dtparam=i2s=on`은 주석, `python3` 있음, `sox` 없음 |
| INMP441 ↔ `ics43432` 호환 | [hardware.md](../hardware.md#inmp441-i2s-mems-마이크) 기준 24비트·채널당 32 SCK·표준 I2S·마이크 슬레이브 → `ics43432`의 S24_LE·S32, `format = "i2s"`, 코덱 클럭 슬레이브와 맞음. 원본 데이터시트 직접 대조는 미확인 |

### 0. 배선 (전원 분리 상태)

| INMP441 | Pi 물리 핀 | BCM / 기능 |
| --- | --- | --- |
| VDD | 17 | 3.3V |
| GND | 39 | GND |
| SCK | 12 | GPIO18 / PCM_CLK (BCLK) |
| WS | 35 | GPIO19 / PCM_FS (LRCLK) |
| SD | 38 | GPIO20 / PCM_DIN |
| L/R | GND(예: 34) | 왼쪽 슬롯 출력 |

**[붙여 주세요]** 실제로 연결한 핀(위와 다르면 그 내용)

### 1. 연결 전 기준과 백업 (Pi)

```bash
# [Pi]
arecord -l
cat /proc/asound/cards
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-14
grep -n -E 'i2s|adau7002|inmp441|^dtoverlay' /boot/firmware/config.txt
mkdir -p ~/bsp/mic
```

### 2. A단계: 기존 `adau7002-simple` overlay (Pi)

```bash
# [Pi]
echo 'dtoverlay=adau7002-simple' | sudo tee -a /boot/firmware/config.txt
diff /boot/firmware/config.txt.bak-14 /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후
lsmod | grep -E '^(snd_soc_bcm2835_i2s|snd_soc_adau7002|snd_soc_simple_card) '
cat /proc/asound/cards
arecord -l
sudo dmesg | grep -i -E 'adau7002|i2s|asoc|simple' | tail -10
arecord -D hw:CARD=adau7002,DEV=0 --dump-hw-params -d 1 /dev/null 2>&1 | grep -E 'FORMAT|CHANNELS|RATE'
```

**[붙여 주세요]**

### 3. A단계 녹음과 진폭 확인 (Pi)
조용히 5초, 말하거나 손뼉을 치며 5초 녹음한다.

```bash
# [Pi]
arecord -D hw:CARD=adau7002,DEV=0 -f S32_LE -r 48000 -c 2 -d 5 ~/bsp/mic/a-quiet.wav
arecord -D hw:CARD=adau7002,DEV=0 -f S32_LE -r 48000 -c 2 -d 5 ~/bsp/mic/a-loud.wav
ls -l ~/bsp/mic/
```

```bash
# [Pi] 채널별 최대값·RMS(24비트로 환산)와 하위 8비트가 0인지
for f in ~/bsp/mic/a-quiet.wav ~/bsp/mic/a-loud.wav; do
python3 - "$f" <<'EOF'
import sys, wave, array, math
w = wave.open(sys.argv[1])
ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
a = array.array('i'); a.frombytes(w.readframes(n))
print(sys.argv[1].split('/')[-1], f"ch={ch} width={sw*8}bit rate={rate} frames={n}")
for c in range(ch):
    s = a[c::ch]
    v = [x >> 8 for x in s]
    rms = math.sqrt(sum(x * x for x in v) / len(v))
    low = sum(1 for x in s if x & 0xff)
    print(f"  ch{c}: peak={max(abs(x) for x in v)} rms={rms:.1f} low8_nonzero={low}")
EOF
done
```

**[붙여 주세요]**

```bash
# [WSL] ~/bspworkspace/bsp-study — 들어 보기(선택)
mkdir -p out/mic && scp 'pi:bsp/mic/*.wav' out/mic/ && explorer.exe "$(wslpath -w out/mic)"
```

### 4. B단계: 직접 작성한 overlay (Codex 구현·리뷰 통과 후)

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic
make
make deploy
```

```bash
# [Pi] dtbo를 overlay 폴더에 넣고 A단계 줄을 inmp441로 바꿈
sudo cp ~/bsp/overlays/inmp441.dtbo /boot/firmware/overlays-bsp-study01/
sudo sed -i 's/^dtoverlay=adau7002-simple$/dtoverlay=inmp441/' /boot/firmware/config.txt
diff /boot/firmware/config.txt.bak-14 /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후
lsmod | grep -E '^(snd_soc_bcm2835_i2s|snd_soc_ics43432|snd_soc_simple_card) '
cat /proc/asound/cards
arecord -l
arecord -D hw:CARD=inmp441,DEV=0 --dump-hw-params -d 1 /dev/null 2>&1 | grep -E 'FORMAT|CHANNELS|RATE'
arecord -D hw:CARD=inmp441,DEV=0 -f S32_LE -r 48000 -c 2 -d 5 ~/bsp/mic/b-loud.wav
```

3단계의 python 확인을 `~/bsp/mic/b-loud.wav`로 다시 실행한다.

**[붙여 주세요]**

### 5. 원복 (Pi)

```bash
# [Pi]
sudo cp /boot/firmware/config.txt.bak-14 /boot/firmware/config.txt
grep -c -E 'adau7002|inmp441' /boot/firmware/config.txt     # 0
sudo reboot
# 재부팅 후
arecord -l                                                  # 캡처 장치 없음
```

**[붙여 주세요]**

### 문제가 생기면
- `arecord -l`에 장치가 없음: `dmesg`의 ASoC 오류(`probe deferral`, `Failed to ...`)를 붙여 준다. `lsmod`에 세 모듈이 있는지 본다.
- 녹음은 되는데 값이 전부 0 또는 -1: SD 선(GPIO20)·VDD·GND, L/R 연결과 다른 채널을 확인한다.
- 부팅이 안 됨: SD 카드를 PC 리더로 열어 `config.txt.bak-14`를 `config.txt`로 덮어쓴다.

## 완료 기준
- [x] 전원을 끈 상태에서 배선, bringup.md에 기록
- [x] A: 캡처 카드 `adau7002` 등록, `--dump-hw-params`로 형식·채널·레이트 확인
- [x] A: 조용할 때와 소리 낼 때 한 채널의 peak·RMS가 뚜렷이 다름, 다른 채널은 거의 0 (재부팅 직후 첫 녹음 1회 이상, 결과 참고)
- [x] A: 24비트 데이터 위치 확인 — 하위 8비트는 0이 아니라 tri-state 구간이라 떠 있음, 상위 24비트만 유효
- [x] B: 직접 작성한 overlay로 카드 `inmp441` 등록, `ics43432` 코덱 적재 (녹음은 완료, 진폭 수치는 미검증)
- [x] 원복 후 캡처 장치가 없어짐

## 결과
대표 출력 몇 줄과 판단만 적는다.

**0~2단계 (A단계 `adau7002-simple`, 사용자 실행)**
- 배선: 노트 0단계 표대로 연결(물리 17·39·12·35·38, L/R → 34)
- 적용 전: 캡처 장치 없음, 카드는 `vc4hdmi0`·`vc4hdmi1`·`Headphones`(재생용) 3개. `config.txt`에 `dtparam=i2s=on`은 주석 상태
- `config.txt` 변경: `diff` 결과 `> dtoverlay=adau7002-simple` 한 줄만 추가
- 재부팅 후 모듈 자동 적재: `snd_soc_simple_card`, `snd_soc_bcm2835_i2s`, `snd_soc_adau7002` → DT의 compatible만으로 세 드라이버가 붙음
- 카드: `3 [adau7002]: simple-card - adau7002`, 캡처 장치 `card 3: adau7002 ... device 0: bcm2835-i2s-adau7002-hifi adau7002-hifi-0`
- dmesg: `adau7002-codec: supply IOVDD not found, using dummy regulator` — overlay에 IOVDD 공급 노드가 없어서 나오는 정보 메시지로 봄(마이크는 3.3V 핀에서 직접 전원)
- `--dump-hw-params`: `FORMAT: S16_LE S24_LE S32_LE`, `CHANNELS: 2`, `RATE: [8000 96000]` → CPU DAI(2채널 고정)와 adau7002 코덱(8~96kHz) 범위의 교집합

**3단계 녹음·진폭 (48kHz, S32_LE, 2채널)**
- `a-loud`: ch0 peak 482,728·RMS 45,308(최대치 8,388,608의 약 6%), ch1 전부 0 → L/R=GND라 왼쪽 슬롯(ch0)에만 데이터. 사용자가 들어 보고 목소리 확인
- 하위 8비트: `low8_nonzero`가 0이 아님 → INMP441은 24비트 뒤 남은 8 SCK 동안 SD를 tri-state로 두어 하위 8비트가 떠 있는 값으로 읽힘. 상위 24비트(`>> 8`)만 유효
- 시작 과도 구간(`q1`·`q2`, 조용히 3초): ch0 peak가 0~50ms 약 49만 → 50~100ms 약 26만 → 100~150ms 3~4만 → 150ms 이후 수천으로 줄어듦. ch1은 처음부터 0. 1초 이후 ch0 peak 약 2.2만(주변 소음). 녹음마다 Pi가 클럭을 새로 내보내 마이크가 깨어나는 구간으로 봄. 사용자가 들어 보면 이상한 소리는 없음 → 듣는 용도에서는 그대로 써도 되고, 소리 크기를 숫자로 재거나 자동 판단(레벨·음성 검출)에 쓸 때만 앞쪽 약 150~200ms를 제외
- 이상 1회(`a-quiet`, 재부팅 후 첫 녹음): 0~50ms는 0이었다가 이후 끝까지 ch0·ch1 모두 최대치(8,388,608) → 사용자가 "뿌직" 소리로 들음. 같은 조건의 `q1`·`q2`와 이어서 녹음한 `a-loud`는 정상이라 재현되지 않음. 원인(재부팅 직후 첫 캡처의 프레임 어긋남, 순간 접촉 불량 등)은 미확인

**4단계 B단계 (직접 작성한 `inmp441` overlay)**
- `config.txt`: `diff` 결과 `> dtoverlay=inmp441` 한 줄(A단계 줄을 대체)
- 모듈 자동 적재: `snd_soc_simple_card`, `snd_soc_bcm2835_i2s`, `snd_soc_ics43432` → 코덱만 바뀜
- 카드: `3 [inmp441]: simple-card - inmp441`, 캡처 장치 `bcm2835-i2s-ics43432-hifi ics43432-hifi-0`
- `--dump-hw-params`: `FORMAT: S24_LE S32_LE`, `CHANNELS: 2`, `RATE: [8000 52800]` → A단계(S16 포함, 8000~96000)와 다름. `ics43432.c` L22~23의 `RATE_MIN 7190`·`RATE_MAX 52800`("from data sheet")과 CPU DAI 하한 8000의 교집합. 상한이 384kHz로 넓어질 것이라는 사전 예상은 틀림. INMP441 사양 WS 7.8~50kHz와 가까운 범위
- `b-loud.wav` 녹음 시작(48kHz, S32_LE, 2채널). 진폭 확인 출력은 미전달
- 재부팅 직후 첫 캡처는 `--dump-hw-params -d 1 /dev/null`(1초 캡처)였고 오류 없이 끝남. `a-quiet` 같은 이상의 재현 여부는 이 캡처를 저장하지 않아 판단 못 함

**5단계 원복**
- `config.txt.bak-14`로 원복, `grep -c` 결과 0. 재부팅 후 재접속해 `arecord -l`에 캡처 장치 없음(원복 명령 직후 보인 `inmp441` 카드는 재부팅 완료 전 출력)

- 미검증: `b-loud` 채널별 진폭 수치(녹음 완료, 수치 미전달), `a-quiet` 이상 원인
- 마무리: 학습 문서 [mic.md](../learning/14-mic.md), bringup·hardware·worklog·roadmap 갱신
