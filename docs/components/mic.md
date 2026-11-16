# 마이크: INMP441 I2S MEMS 마이크 (ASoC, 직접 작성 코덱 드라이버)

## 1. 요약
- **역할:** INMP441의 24비트 I2S 데이터를 ALSA 캡처 장치(`hw:CARD=inmp441study`)로 낸다.
- **상태:** 두 카드에서 `devcheck mic` PASS(48kHz·S32_LE·2ch, rms 약 9만, xrun 0). 재부팅 직후 첫 녹음이 최대치로 나온 1회는 원인 미확인([14 6장](../learning/14-mic.md#6-트러블슈팅과-한계)).
- **담당 파일:**
  - 현재 기준(C): 코덱 드라이버 [inmp441_codec.c](../../examples/mic_codec/inmp441_codec.c)(`nakta,inmp441`), overlay [inmp441-study-overlay.dts](../../examples/mic_codec/inmp441-study-overlay.dts)
  - 앱(D): [mic_capture.c](../../examples/mic_app/mic_capture.c), [mic_plot.py](../../examples/mic_app/mic_plot.py)
  - 비교용(B): mainline 코덱 `ics43432`를 쓰는 overlay [inmp441-overlay.dts](../../examples/mic/inmp441-overlay.dts)
  - Yocto: [inmp441-codec.bb](../../yocto/meta-bsp-study/recipes-kernel/inmp441-codec/inmp441-codec.bb)
- **학습 문서:** [14 마이크](../learning/14-mic.md)

## 2. 구성
흐름: `INMP441 SD → GPIO20 → bcm2835-i2s(I2S 수신 + DMA) → ALSA ring buffer → /dev/snd/pcmCND0c → arecord/mic_capture`. 클럭은 Pi가 GPIO18(BCLK)·GPIO19(WS)로 낸다.

| 항목 | 64GB 카드 (6.18.59 Debian) | Yocto 카드 (6.6.63) |
| --- | --- | --- |
| HW 연결 | SCK → 물리 12(GPIO18), WS → 35(GPIO19), SD → 38(GPIO20), VDD 3.3V, L/R → GND ([bringup](../bringup.md#40핀-한눈에-보기)) | 같음 |
| DT/overlay | `inmp441-study.dtbo`를 `overlays-bsp-study01/`에 두고 `config.txt` 한 줄 | `config.txt`의 `dtoverlay=inmp441-study` |
| 코덱 드라이버 | `inmp441_codec.ko`를 `insmod`(또는 `extra/` 설치 + `depmod`) | 레시피로 설치, `nakta,inmp441` modalias로 자동 적재 |
| CPU DAI·machine | 업스트림 `bcm2835-i2s`, `simple-audio-card` | 같음 |
| 6.6 차이 | — | 없음(같은 소스가 compat 헤더 없이 빌드됨) |
| 사용자 인터페이스 | `arecord -l`의 `inmp441study`, `mic_capture` | 카드 같음. 이미지에 `arecord`는 없고 `devcheck mic`으로 확인 |

- 소리 데이터는 코덱 드라이버를 지나지 않는다. 코덱은 형식(S24_LE·S32_LE)·채널·레이트(`[8000 50000]`)만 알린다.
- I2S가 GPIO18~21을 쓰므로 모터 PWM은 GPIO12/13으로 옮겼다([pwm](pwm.md)).

## 3. 빌드·배포
- **64GB 카드:** overlay와 코덱 모듈을 보내고 `config.txt`를 백업한 뒤 한 줄 추가한다.

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic_codec
make && make deploy                          # inmp441_codec.ko, inmp441-study.dtbo → pi:bsp/
# [Pi] 64GB
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-14-3
sudo cp ~/bsp/overlays/inmp441-study.dtbo /boot/firmware/overlays-bsp-study01/
echo 'dtoverlay=inmp441-study' | sudo tee -a /boot/firmware/config.txt && sudo reboot
sudo insmod ~/bsp/modules/inmp441_codec.ko   # 재부팅 후. 카드가 이때 생김
```

- **Yocto 카드:** `inmp441-codec.bb`가 `examples/mic_codec`의 `.c`를 빌드하고, 커널 bbappend가 overlay를 `inmp441-study.dtbo`로 만든다. 부팅하면 코덱 모듈이 자동 적재되고 카드가 생긴다.

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| 코덱 모듈 | `lsmod \| grep '^inmp441_codec '` | 있음 |
| 카드 | `cat /proc/asound/cards` | `[inmp441study ]` 행 |
| 지연 probe 없음 | `sudo cat /sys/kernel/debug/devices_deferred` | `soc:sound`가 없음 |
| 협상 범위(64GB) | `arecord -D hw:CARD=inmp441study,DEV=0 --dump-hw-params -d 1 /dev/null 2>&1 \| grep RATE` | `[8000 50000]` |
| 녹음(64GB) | `arecord -D hw:CARD=inmp441study,DEV=0 -f S32_LE -r 48000 -c 2 -d 5 test.wav` | ch0에만 소리, ch1 거의 0 |
| 한 번에 | `devcheck mic` | 48000/S32_LE/2ch, rms 약 9만, xrun 0, PASS |

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| 지원 레이트·형식 변경 | `inmp441_codec.c`의 DAI `capture` 설정(`INMP441_RATE_MAX` 등) | 데이터시트 범위, 등록 로그 문자열 | `--dump-hw-params` |
| 카드 이름 변경 | overlay의 `simple-audio-card,name` | `devcheck.c`의 `check_mic`가 `[inmp441study `로 찾음 | `/proc/asound/cards`, `devcheck mic` |
| mainline 코덱으로 비교 | 64GB `config.txt`를 `dtoverlay=inmp441`(B)로 | 직접 작성 overlay 줄과 동시에 두지 않음 | `arecord -l`의 `ics43432` |
| 핀 변경 | 사실상 불가(I2S는 GPIO18~21 고정 기능) | — | — |
| 소리 크기 | 앱 `mic_capture -g <배수>` | 증폭 없는 감도 −26 dBFS라 말소리는 최대치의 수 % | peak 값 |

## 6. 트러블슈팅
먼저 볼 계층: 4 모듈 적재 → 5 probe(카드 생성) → 7 ALSA.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 5 매칭·probe | 64GB 재부팅 후 카드 없음, `devices_deferred`에 `asoc-simple-card: parse error` | `lsmod`에 `inmp441_codec` | 외부 코덱 모듈이 `/lib/modules`에 없어 machine이 코덱을 못 찾음(`-EPROBE_DEFER`) | `insmod`하면 자동으로 다시 probe. 부팅 때 올리려면 [조치 절차: 코덱 자동 적재](#조치-절차-코덱-자동-적재64gb) |
| 5 매칭·probe | 코덱 없이 `asoc-simple-card/bind`가 `I/O error` | 같음 | probe가 `-EPROBE_DEFER`로 끝난 것으로 봄 | 코덱 적재 후 자동으로 다시 probe |
| 4 모듈 적재 | `rmmod inmp441_codec`가 `Module inmp441_codec is in use` | `lsmod` Used by | 카드가 코덱 모듈 참조를 잡고 있음 | 카드를 먼저 unbind: `sudo sh -c 'echo soc:sound > /sys/bus/platform/drivers/asoc-simple-card/unbind'` |
| 3 DT·overlay | B(mainline 코덱) dmesg `supply IOVDD not found, using dummy regulator` | — | overlay에 코덱 전원 노드가 없음 | 조치 없음. 마이크는 3.3V 핀 직접 전원 |
| 8 사용자 공간 | 녹음 시작 직후 0~150ms 값이 큼 | 파형 앞부분 | 녹음마다 Pi가 클럭을 새로 내보내 마이크가 깨어나는 구간 | 레벨 측정에서 앞 150~200ms 제외 |
| 8 사용자 공간 | 하위 8비트가 0이 아님 | 샘플 hex | 24비트 뒤 SD tri-state 구간을 읽음 | `>> 8`로 상위 24비트만 사용 |
| 8 사용자 공간 | 녹음 소리가 작음 | peak 값 | 감도 −26 dBFS, 증폭 단계 없음 | `mic_capture -g 8` |
| 7 서브시스템 | `mic_plot.py` 실행 직후 `[XRUN]` 2회 | 창이 뜬 뒤에도 느는지 | python이 창을 띄우는 동안 stdin을 안 읽어 파이프가 참 | 창이 뜬 뒤 늘지 않으면 조치 없음 |
| 8 사용자 공간 | 재부팅 직후 첫 녹음이 ch0·ch1 모두 최대치 | ch1도 최대인지 | 마이크 소리가 아니라 데이터가 잘못 읽힘. 프레임 어긋남·접촉 불량 의심(재현 안 됨) | 다시 녹음. 재현되면 배선부터 확인 |

### 조치 절차: 코덱 자동 적재(64GB)
1. 코덱 모듈을 실행 커널의 `extra/`에 설치하고 별칭 표를 갱신한다.
2. 재부팅하면 DT 노드의 `nakta,inmp441`로 udev가 모듈을 올린다.
3. 원복은 설치 파일 삭제 → `depmod -a` → `config.txt` 백업 복원 → 재부팅.

```bash
# [Pi] 64GB
sudo install -D -m 644 ~/bsp/modules/inmp441_codec.ko /lib/modules/$(uname -r)/extra/inmp441_codec.ko
sudo depmod -a && sudo reboot
cat /proc/asound/cards                       # 재부팅 후 inmp441study
```

**점검 포인트**
- ch1에도 소리가 나면 L/R 핀 연결을 본다(GND면 왼쪽 슬롯 ch0).
- `devcheck mic`이 카드를 못 찾으면 `/proc/asound/cards`의 이름이 `inmp441study`인지 본다(B overlay는 `inmp441`).

## 7. 알려진 제한
- CPU DAI(I2S·DMA)와 machine은 직접 작성하지 않았다(선택 과제).
- mmap·poll 캡처, 재생은 하지 않았다.
