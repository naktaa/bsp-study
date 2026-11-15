# 13-3 최소 IMX219 센서 드라이버 직접 작성

상태: 완료 · 브랜치: `feat/camera-sensor` · Codex 지시서: [13-3-camera-sensor.codex.md](13-3-camera-sensor.codex.md)

## 목표
mainline `imx219` 대신 직접 작성한 최소 센서 드라이버 `imx219_study`로 unicam과 연결해 RAW 프레임을 받는다. 새 센서를 붙일 때 BSP가 하는 일(DT 파싱, 전원 순서, I2C 레지스터 설정, subdev 포맷·컨트롤, async 등록)을 설명할 수 있게 한다. 영상 처리와 libcamera 지원은 범위 밖이다.

## 알아둘 개념
1. **unicam이 센서에서 쓰는 것과 상태 API:** 레거시 unicam(`drivers/media/platform/bcm2835/bcm2835-unicam.c`)은 source pad 개수(L3099), `get_fmt`(자기 state L644 또는 NULL L2032), `video.s_stream`(L2570)만 쓰고 PIXEL_RATE·LINK_FREQ는 읽지 않는다. `get_mbus_config`는 없어도 된다(L2503). 코어 `check_state()`(`v4l2-core/v4l2-subdev.c` L214)는 TRY 포맷에만 state를 요구하므로 subdev 상태 API는 필수가 아니다.
   - mainline 표준(6.18 `imx219.c`): `init_state` + `v4l2_subdev_init_finalize()`(L1268)로 코어가 포맷·crop을 state에 보관하고, `enable_streams`/`disable_streams`(L1000)를 `v4l2_subdev_s_stream_helper`(L991)가 이어 준다. 모드 전환·스트림 단위 제어에 맞춘 구조다.
   - 이번 방식: 모드가 하나라 포맷을 드라이버 상수로 두고 `get_fmt`/`set_fmt`가 그대로 돌려준다. `.s_stream`을 직접 구현한다. 새 upstream 드라이버라면 상태 API를 써야 한다.
2. **전원 순서와 DT 속성:** `imx219.dtsi`에는 `reset-gpios`가 없다. 전원은 `VANA-supply = <&cam1_reg>`(`bcm2711-rpi-4-b.dts` L27 regulator-fixed, `expgpio 5`)가 켜고, VDIG·VDDL은 dummy다. 켜는 순서는 regulator → `xclk`(24MHz) → 6.2ms 대기(`imx219.c` L306 `IMX219_XCLR_MIN_DELAY_US`)다. endpoint의 `data-lanes`(2레인)와 `link-frequencies`(456MHz)는 probe에서 확인만 하고 값은 고정이다.
3. **async 등록과 모듈 참조:** 센서가 `v4l2_async_register_subdev_sensor()`로 등록되면 unicam notifier의 `.bound` → `.complete`(L3157)가 `/dev/video0`·subdev 노드를 만든다. 이때 `v4l2-async.c` L350 → `__v4l2_device_register_subdev()`(`v4l2-device.c` L130)가 센서 모듈에 `try_module_get`을 한다. 그래서 unicam이 붙어 있는 동안에는 센서 모듈을 단독으로 rmmod할 수 없다("in use"). 반복 시험은 unicam을 먼저 내린다.

## 결정
- 레지스터: 6.18 `imx219.c`에는 벤더 init 테이블이 없다. 고정 테이블은 `imx219_common_regs`(L160) 23개(MODE_SELECT, 0x3000~ 접근 해제 시퀀스, 문서화되지 않은 레지스터 12개, X/Y 증가값, DPHY·EXCK), `imx219_2lane_regs`(L194) 8개, 그리고 `imx219_set_framefmt()`(L669)가 3280×2464 RAW10에서 계산하는 값이다. 값과 출처는 [Codex 지시서](13-3-camera-sensor.codex.md) 표에 있다.
- 모드: 3280×2464 `SRGGB10_1X10`, 2레인, flip 없음. 13-2 측정과 같은 모드다. `hblank`는 168(라인 3448)로 고정한 읽기 전용 컨트롤이다.
- reset GPIO는 쓰지 않는다(DT에 없음). runtime PM 없이 `s_stream`에서 전원을 켜고 끈다.
- mainline 비교값은 13-2 실측(같은 카드·모드)을 쓴다. 원복 뒤 mainline 1회 재측정은 선택이다.
- 엔티티 이름이 `imx219-study 10-0010`이 되어 libcamera rpi CamHelper가 인식하지 못할 가능성이 크다. libcamera는 완료 기준에 넣지 않는다.

## 진행 순서

### 0. [Pi] 64GB 카드 config 백업·변경

```bash
# [WSL] ~/bspworkspace/bsp-study — Codex 구현·리뷰 통과 후
make -C examples/camera_sensor W=1
make -C examples/camera_sensor deploy        # .ko → pi:bsp/modules/, dtbo → pi:bsp/overlays/
```

```bash
# [Pi] ~ — 64GB 카드
C=/boot/firmware/config.txt
sudo cp $C $C.bak-13-3
sudo cp ~/bsp/overlays/imx219-study.dtbo /boot/firmware/overlays-bsp-study01/
sudo sed -i 's/^camera_auto_detect=1$/camera_auto_detect=0/' $C
echo "dtoverlay=imx219-study" | sudo tee -a $C
grep -nE 'camera_auto_detect|dtoverlay' $C
sudo reboot
```

확인: **[붙여 주세요]** `grep` 결과. `camera_auto_detect=0`과 맨 끝 `dtoverlay=imx219-study`가 있어야 한다.
- 복구: `sudo cp $C.bak-13-3 $C && sudo reboot`. 부팅이 안 되면 카드 리더로 부트 파티션의 `config.txt.bak-13-3`을 `config.txt`로 복사한다.

### A1. [Pi] probe와 연결

```bash
# [Pi] ~ — 재부팅 후. 이후 단계는 같은 셸
lsmod | grep -E '^(imx219|bcm2835_unicam_legacy) '          # unicam만 있고 imx219 없음
sudo modprobe -a v4l2_cci v4l2_fwnode                        # insmod는 의존 모듈을 올리지 않음
sudo insmod ~/bsp/modules/imx219_study.ko
dmesg | grep -iE 'imx219|unicam' | tail -8
lsmod | grep -E '^(imx219_study|bcm2835_unicam_legacy) '     # imx219_study Used by 1
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
media-ctl -d "$m" -p | grep -A6 'imx219-study'
sd=$(media-ctl -d "$m" -e "imx219-study 10-0010"); vd=$(media-ctl -d "$m" -e unicam-image)
echo "m=$m sd=$sd vd=$vd"
sudo rmmod imx219_study                                      # 실패해야 함: in use
```

확인: **[붙여 주세요]**
- dmesg의 chip ID 확인 줄과 오류 없음
- `lsmod`에 mainline `imx219`가 없고 `imx219_study`의 Used by가 1
- `media-ctl`에서 `imx219-study 10-0010` pad 0 → `unicam-image` 링크
- 단독 rmmod가 `Module imx219_study is in use`로 실패

### A2. [Pi] 테스트 패턴 캡처

```bash
# [Pi] A1 셸
mkdir -p ~/bsp/cam3
media-ctl -d "$m" -V '"imx219-study 10-0010":0 [fmt:SRGGB10_1X10/3280x2464]'
v4l2-ctl -d "$vd" --set-fmt-video=width=3280,height=2464,pixelformat=RG10
v4l2-ctl -d "$vd" --get-fmt-video | grep -E "Width|Pixel Format|Bytes per Line"
v4l2-ctl -d "$sd" --set-ctrl test_pattern=1
~/bsp/apps/v4l2_capture "$vd" 5 ~/bsp/cam3/colorbar.raw | tail -2
v4l2-ctl -d "$sd" --set-ctrl test_pattern=0
python3 ~/bsp/apps/raw_tool.py ~/bsp/cam3/colorbar.raw 3280 2464 --stride 6560 --png ~/bsp/cam3/colorbar.png
```

```bash
# [WSL] ~/bspworkspace/bsp-study
mkdir -p out/cam3 && scp 'pi:bsp/cam3/*.png' out/cam3/
explorer.exe "$(wslpath -w out/cam3)"
```

확인: **[붙여 주세요]** `Bytes per Line` 6560, 캡처 마지막 줄(`sequence_gaps=0`), PNG가 세로 색 띠로 보이는지.
- 첫 스트림에서 프레임이 오지 않거나 unicam 오류가 나면 LP-11 진입 문제를 먼저 의심한다. mainline은 probe에서 streaming → standby 토글을 한다(`imx219.c` L1236). 이 경우 Codex에 토글 추가를 지시한다.

### A3. [Pi] 모듈 해제·적재 3회

```bash
# [Pi] ~ — unicam을 먼저 내려 센서 모듈 참조를 푼다(개념 3)
for i in 1 2 3; do
  sudo rmmod bcm2835_unicam_legacy && sudo rmmod imx219_study &&
  sudo insmod ~/bsp/modules/imx219_study.ko && sudo modprobe bcm2835_unicam_legacy || { echo "fail $i"; break; }
  sleep 1
  m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
  echo "$i m=$m $(media-ctl -d "$m" -p | grep -c 'imx219-study')"
done
dmesg | grep -iE 'imx219|unicam|error|warn' | tail -10
```

확인: **[붙여 주세요]** 3회 모두 `imx219-study` 엔티티가 보이는지, dmesg에 WARN·오류가 없는지.
- 반복 뒤 media 번호가 바뀔 수 있다. B는 `m`·`sd`·`vd`를 A1 방식으로 다시 구한다.

### B1. [Pi] 노출·프레임 간격

```bash
# [Pi] ~ — m·sd·vd를 다시 구한 셸. hblank는 읽기 전용(168 고정)이라 설정하지 않는다
media-ctl -d "$m" -V '"imx219-study 10-0010":0 [fmt:SRGGB10_1X10/3280x2464]'
v4l2-ctl -d "$vd" --set-fmt-video=width=3280,height=2464,pixelformat=RG10
v4l2-ctl -d "$sd" --set-ctrl vertical_blanking=181
v4l2-ctl -d "$sd" --get-ctrl horizontal_blanking,vertical_blanking,pixel_rate,link_frequency
lines=$(awk 'BEGIN { print int(12000 * 182.4 / 3448) }'); echo "lines=$lines"
v4l2-ctl -d "$sd" --set-ctrl exposure=$lines,analogue_gain=0
~/bsp/apps/v4l2_capture "$vd" 20 ~/bsp/cam3/exp12000.raw | tail -1
v4l2-ctl -d "$sd" --get-ctrl exposure,analogue_gain
```

확인: **[붙여 주세요]** `horizontal_blanking` 168, `vertical_blanking` 181, `pixel_rate` 182400000, `lines=634`, 캡처 `average_interval_ms`(기대 50.000), `sequence_gaps=0`, 다시 읽은 `exposure` 634.

### B2. [Pi] v4l2-compliance

```bash
# [Pi] B1 셸
command -v v4l2-compliance || dpkg -l v4l-utils | tail -1
v4l2-compliance -u "$sd" 2>&1 | grep -E 'fail|Total'
```

확인: **[붙여 주세요]** `Total` 줄과 `fail` 항목. 실패는 이유만 기록하고 통과시키려고 기능을 늘리지 않는다. 명령이 없으면 `sudo apt install v4l-utils` 후 실행한다.

### B3. [Pi] 원복

```bash
# [Pi] ~
sudo cp /boot/firmware/config.txt.bak-13-3 /boot/firmware/config.txt
grep -nE 'camera_auto_detect|dtoverlay' /boot/firmware/config.txt
sudo reboot
# 재부팅 후
lsmod | grep -E '^(imx219|imx219_study) '                      # imx219만
rpicam-still -n --immediate -o ~/bsp/cam3/after.jpg && ls -l ~/bsp/cam3/after.jpg
```

확인: **[붙여 주세요]** `camera_auto_detect=1` 복귀, mainline `imx219` 적재, `rpicam-still` 저장.
- 선택: mainline으로 B1을 1회 다시 측정한다. 엔티티 이름은 `imx219 10-0010`이다. mainline은 `horizontal_blanking`이 쓰기 가능하지만 기본값 168이라 설정하지 않아도 같다. 결과를 13-2 값과 함께 비교표에 넣는다.

## 완료 기준
- [x] 6.18 크로스 빌드 `W=1` 경고 0, checkpatch 결과 기록(Codex)
- [x] chip ID 확인, mainline `imx219` 미적재, `imx219-study` → unicam 링크
- [x] `lsmod` Used by 1, 센서 단독 rmmod가 in use로 실패
- [x] 테스트 패턴 컬러바 PNG, `sequence_gaps=0`
- [x] unicam → 센서 순 해제·적재 3회에 오류 없음
- [x] 노출 634라인, `vertical_blanking=181`에서 50.000ms로 13-2 mainline 값과 일치(50.001ms)
- [x] v4l2-compliance 결과와 실패 이유 기록
- [x] 원복 후 mainline `rpicam-still` 정상

## 결과
- 0 config: `camera_auto_detect=0`(L24), 맨 끝 `dtoverlay=imx219-study`(L63). 기존 `vc4-kms-v3d`·`dwc2,dr_mode=host`·`nospi10` 유지
- A1 probe·연결: 부팅 후 `bcm2835_unicam_legacy`만 있고 mainline `imx219` 없음. insmod 후 `imx219-study 10-0010: chip ID: 0x0219`, 오류 없음. `lsmod` `imx219_study` Used by 1. `media-ctl`에 `imx219-study 10-0010`(1 pad, `/dev/v4l-subdev0`, `SRGGB10_1X10/3280x2464`) → `unicam-image` `[ENABLED,IMMUTABLE]`, `m=/dev/media1`. 단독 `rmmod`는 `Module imx219_study is in use`로 실패 → 개념 3과 일치
  - 부팅 로그 `Fixed dependency cycle(s)`(csi ↔ `imx219@10`)는 13단계 mainline과 같은 fw_devlink 정보 메시지(`drivers/base/core.c` L2209)
- A2 테스트 패턴: `RG10` 3280/2464, `Bytes per Line` 6560. 첫 스트림부터 프레임 수신(LP-11 토글 없이 동작) → 5프레임, 평균 66.651ms(기본 vblank, 3526라인 = 66.654ms), `sequence_gaps=0`. raw_tool R·Gr·Gb·B 모두 479.5, 포화 50% → 13-2 mainline 컬러바와 같은 값(8색 바에서 채널마다 4개 띠가 1023)
- A3 해제·적재 3회: 3회 모두 `/dev/media1`에 `imx219-study` 2줄(엔티티 + unicam 쪽 링크), 매번 `chip ID: 0x0219`, dmesg에 WARN·오류 없음
- B1 노출·간격: `horizontal_blanking` 168, `vertical_blanking` 181, `pixel_rate` 182400000, `link_frequency` 456000000, `lines=634` → 다시 읽은 `exposure` 634, `analogue_gain` 0. 20프레임 평균 50.001ms, `sequence_gaps=0`

  | 항목 | 13-3 `imx219_study` | 13-2 mainline `imx219` |
  | --- | --- | --- |
  | 노출 12000µs | 634라인 | 634라인 |
  | 기본 프레임 간격 | 66.651ms (5프레임) | 66.654ms |
  | `vertical_blanking=181` 간격 | 50.001ms | 50.000ms |
  | 컬러바 R·Gr·Gb·B / 포화 | 479.5 / 50% | 479.5 / 50% |
  | `sequence_gaps` | 0 | 0 |

- B2 v4l2-compliance(`-u /dev/v4l-subdev0`): 46개 중 45 통과, 1 실패 `subscribe event for control 'User Controls' failed`(`v4l2-test-controls.cpp` L1128). 원인: 컨트롤 이벤트 미지원(`V4L2_SUBDEV_FL_HAS_EVENTS`와 `core.subscribe_event`/`unsubscribe_event` 없음). 6.18 mainline `imx219.c`도 둘 다 없어 같은 항목이 실패할 것으로 봄(mainline 실행은 안 함). 범위상 추가하지 않음
- 추가 실제 장면(선택): B2 뒤 `exposure=2641,analogue_gain=200`만 바꿔 찍은 `scene.png`가 `colorbar.png`와 md5까지 같았음. 원인 추정: v4l2-compliance가 컨트롤 시험 중 `test_pattern`을 바꾼 채 끝남(확인 안 함). `test_pattern=0`을 명시하고 다시 찍은 `real_open.png`는 실제 장면(Claude 확인: 글자가 상하 반전, 초록빛, 어두움), `real_covered.png`와 md5가 다름 → 드라이버로 실제 영상 수신 확인
- B3 원복: `config.txt.bak-13-3` 복사 → `camera_auto_detect=1`, `dtoverlay`는 `vc4-kms-v3d`·`dwc2,dr_mode=host`·`nospi10` 3줄. 재부팅 후 `lsmod`에 mainline `imx219`만(Used by 3). `rpicam-still`: libcamera v0.7.2, 튜닝 파일 `imx219.json`, 센서 포맷 `SBGGR10_1X10`(libcamera가 flip 적용), `after.jpg` 2,727,441 B, 사용자가 노트북에서 정상 사진 확인. mainline 1회 재측정(선택)은 하지 않음
- 미검증: mainline `imx219`의 v4l2-compliance(같은 이벤트 항목 실패 여부), compliance 후 `test_pattern`이 남은 원인, 장시간 스트림, 컨트롤 변경 중 스트리밍(스트림 중 노출 변경 반영), libcamera로 `imx219_study` 사용
