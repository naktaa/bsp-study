# 13-2 카메라 노출·게인·테스트 패턴 (libcamera 없이)

상태: 완료 · 브랜치: `feat/camera-ctrl` · Codex 지시서: [13-2-camera-ctrl.codex.md](13-2-camera-ctrl.codex.md)

## 목표
[13단계](13-camera.md)에서 libcamera(`rpicam-*`)로 하던 노출·게인 설정과 프레임 수신을 커널 인터페이스로 직접 한다. 센서 subdev 컨트롤로 VEDA 설정(노출 12000µs, 게인 1)을 적용하고, 직접 작성한 V4L2 캡처 앱으로 RAW를 받아 노출 환산값, 밝기 비, 테스트 패턴, 프레임 간격을 확인한다. 끝나면 libcamera가 대신해 주던 것 중 커널 인터페이스로 직접 할 수 있는 것과 없는 것을 구분해 설명할 수 있다.

- 모드: 3280×2464 하나만 쓴다(13단계 노출 메타데이터가 이 모드에서 나옴, 아래 '시작 전 확인')
- 하지 않는 것: 센서 드라이버 작성, 자동 노출·화이트밸런스, 컬러 영상 출력, 인코딩, embedded data(이유는 '시작 전 확인')
- 부트 파일·`config.txt`는 바꾸지 않는다. 컨트롤은 저장되지 않는다

## 알아둘 개념
1. **subdev 컨트롤과 적용 시점:** `exposure`(라인 수)·`analogue_gain`(코드)·`vblank`·`hblank`·`test_pattern`은 `/dev/v4l-subdevN`의 V4L2 컨트롤이다. `imx219_set_ctrl()`은 스트리밍 중일 때만 레지스터에 쓰고(`imx219.c` L474 `pm_runtime_get_if_in_use`), 그 전에 설정한 값은 스트림 시작 때 한꺼번에 적용된다
2. **노출 µs 환산:** 한 라인 시간 = `(width + hblank) / pixel_rate`, 노출(µs) = `exposure × 한 라인 시간`. 3280×2464 모드는 라인 길이 3448(`IMX219_LLP_MIN`, L77), pixel_rate 182.4M(L134)이라 한 라인 18.9035µs. 프레임 주기 = `(height + vblank) × 한 라인 시간`. 640×480은 analog binning이라 rate_factor 2(L432), 라인 길이 최소 3560(L78·L918)으로 식의 값이 달라진다
3. **V4L2 스트리밍:** 앱이 `VIDIOC_REQBUFS`로 버퍼를 만들고 `mmap` → `QBUF`로 드라이버에 넘기고 → `STREAMON` → `DQBUF`로 채워진 버퍼를 받아 다시 `QBUF`. 버퍼의 `sequence`는 unicam이 프레임마다 1씩 늘리고, `timestamp`는 프레임 시작 인터럽트에서 찍은 MONOTONIC 시각이다(13단계 개념 3). libcamera의 `SensorTimestamp`와 같은 값이다

## 시작 전 확인 (Claude 읽기 확인)

| 항목 | 내용 |
| --- | --- |
| 13단계 11984µs의 모드 | Pi `~/bsp/cam/exp12ms.json`: `ScalerCrop` `(0, 0)/3280x2464`, `FrameDuration` 47183µs = 2496라인 × 18.9035µs(vblank 최소 32) → **3280×2464 모드** |
| 3280×2464 환산 | 12000µs → `floor(12000 / 18.9035)` = 634라인 → 11984.8µs(메타데이터 11984). 2000µs → 105라인 → 1984.9µs(메타데이터 1984) |
| 640×480 환산(비교) | 한 라인 3560 / 364.8M = 9.7588µs(hblank 최소 기준). 12000µs → 1229라인 → 11993.6µs, 2000µs → 204라인 → 1990.8µs. 메타데이터 11984·1984와 달라 두 모드가 구분됨 |
| black level | 같은 메타데이터 `SensorBlackLevels` 4096(16bit 기준) = 10bit 64 |
| 13단계 타임스탬프 | `~/bsp/cam/pts.txt`(640×480, 20fps, libcamera): 평균 49.985ms |
| embedded data | 불가. 6.18 `imx219`는 pad 1개(`imx219.c` L1261), unicam은 센서 source pad가 2개 이상일 때만 메타데이터 노드 등록(`bcm2835-unicam.c` L3122). 13단계 media 그래프도 `unicam-image`만 있음 |
| 테스트 패턴 | `test_pattern` 메뉴 0 Disabled, 1 Color Bars, 2 Solid Color, 3 Grey Color Bars, 4 PN9(`imx219.c` L235) |
| Pi 도구 | `media-ctl`·`v4l2-ctl`(13단계), python3 numpy 2.2.4·Pillow 11.1.0 |

## 진행 순서
모든 Pi 명령은 `ssh pi` 셸에서, `rpicam-*`를 실행하지 않은 상태로 한다(libcamera가 카메라를 잡으면 `Device or resource busy`).

### 0. 빌드·배포 (Codex 구현·리뷰 후)
```bash
# [WSL] ~/bspworkspace/bsp-study
make -C examples/camera
make -C examples/camera deploy       # pi:bsp/apps/v4l2_capture, raw_tool.py
```

### 1. 장치 찾기와 포맷 확인
```bash
# [Pi] 같은 셸에서 이후 단계 진행(m·sd·vd 변수 사용)
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
sd=$(media-ctl -d "$m" -e "imx219 10-0010"); vd=$(media-ctl -d "$m" -e unicam-image)
echo "m=$m sd=$sd vd=$vd"
v4l2-ctl -d "$vd" --list-formats | grep -E "RG10|pRAA"
```

확인: **[붙여 주세요]** 세 장치 경로, 포맷 목록에 unpacked `RG10`(16bit 컨테이너)이 있는지. 없으면 여기서 멈춤

### 2. 포맷과 타이밍 설정
```bash
# [Pi] 1단계 셸
media-ctl -d "$m" -V '"imx219 10-0010":0 [fmt:SRGGB10_1X10/3280x2464]'
v4l2-ctl -d "$vd" --set-fmt-video=width=3280,height=2464,pixelformat=RG10
v4l2-ctl -d "$vd" --get-fmt-video | grep -E "Width|Pixel Format|Bytes per Line|Size Image"
v4l2-ctl -d "$sd" --set-ctrl horizontal_blanking=168,vertical_blanking=181   # 라인 3448, 프레임 2645라인 = 50.0ms(20fps)
v4l2-ctl -d "$sd" --get-ctrl horizontal_blanking,vertical_blanking,pixel_rate,exposure,analogue_gain
```

확인: **[붙여 주세요]** `Bytes per Line`(raw_tool의 stride), `hblank` 168, `vblank` 181, `pixel_rate` 182400000
- v4l2-ctl 컨트롤 이름은 `horizontal_blanking`·`vertical_blanking`이다(`hblank`는 `unknown control`)
- 포맷을 바꾸면 드라이버가 `vblank`를 모드 기본값으로 되돌리므로(`imx219.c` L900) `media-ctl -V` 뒤에 `vblank`를 설정한다
- 프레임 주기 = 2645 × 18.9035 = 49999.8µs. 13단계(640×480, libcamera 20fps) 평균 49.985ms와 간격을 비교한다

### 3. 노출·게인과 캡처
```bash
# [Pi] 2단계 셸
mkdir -p ~/bsp/cam2
for us in 12000 2000; do
  lines=$(awk -v us=$us 'BEGIN { print int(us * 182.4 / 3448) }')
  awk -v l=$lines 'BEGIN { printf "lines=%d -> %.1f us\n", l, l * 3448 / 182.4 }'
  v4l2-ctl -d "$sd" --set-ctrl exposure=$lines,analogue_gain=0
  ~/bsp/apps/v4l2_capture "$vd" 20 ~/bsp/cam2/exp${us}.raw | tail -4
  v4l2-ctl -d "$sd" --get-ctrl exposure,analogue_gain
done
```

확인: **[붙여 주세요]** 두 노출의 `lines`·µs, 캡처 앱 마지막 줄(프레임 수, 평균 간격, sequence 끊김 수), 다시 읽은 컨트롤
- 기대: 12000 → 634라인·11984.8µs, 2000 → 105라인·1984.9µs. 평균 간격 약 50.0ms, sequence 끊김 0
- `analogue_gain=0`이 게인 1(데이터시트 식 `256 / (256 − code)`, 확인 필요)

### 4. 테스트 패턴
```bash
# [Pi] 3단계 셸
v4l2-ctl -d "$sd" --set-ctrl test_pattern=1             # Color Bars
~/bsp/apps/v4l2_capture "$vd" 5 ~/bsp/cam2/colorbar.raw | tail -2
v4l2-ctl -d "$sd" --set-ctrl test_pattern=0
v4l2-ctl -d "$sd" --get-ctrl test_pattern
```

### 5. RAW 확인
```bash
# [Pi] stride는 2단계 Bytes per Line
cd ~/bsp/cam2
S=6560
for f in exp12000 exp2000 colorbar; do
  python3 ~/bsp/apps/raw_tool.py $f.raw 3280 2464 --stride $S --png $f.png
done
```

```bash
# [WSL] ~/bspworkspace/bsp-study — PNG 보기용
mkdir -p out/cam2 && scp 'pi:bsp/cam2/*.png' out/cam2/
explorer.exe "$(wslpath -w out/cam2)"     # Windows 탐색기로 폴더 열기
```

확인: **[붙여 주세요]** 세 파일의 채널별 평균(black level 64 차감 후), 포화 비율, 컬러바 PNG가 세로 색 띠로 보이는지
- 밝기 비 = exp12000 평균 / exp2000 평균(차감 후). 기대는 노출 비 634 / 105 ≈ 6.0에 가까움. 포화 비율이 크면 그 파일은 비 판정에서 제외하고 적는다
- libcamera를 거치지 않아 flip이 적용되지 않으므로 상하좌우가 뒤집혀 보일 수 있음(13단계 `rotation` 180). Bayer 순서는 RGGB로 예상

### 6. 원복
```bash
# [Pi]
v4l2-ctl -d "$sd" --get-ctrl test_pattern              # 0
rpicam-still -n --immediate -o ~/bsp/cam2/after.jpg
ls -l ~/bsp/cam2/after.jpg
```

확인: **[붙여 주세요]** `test_pattern` 0, `rpicam-still`이 오류 없이 저장. libcamera는 시작할 때 포맷·컨트롤을 다시 설정한다. 사진이 평소처럼 보이는지 확인

## 완료 기준
- [x] libcamera 없이 직접 작성한 V4L2 앱으로 RAW 캡처
- [x] 노출 12000µs 요청이 634라인(11984.8µs)으로 설정되어 13단계 메타데이터 11984와 같음. 640×480 환산값(11993.6µs)과 구분됨
- [ ] (부분 확인) black level 차감 후 12000µs/2000µs 밝기 비가 노출 비(≈6.0)에 가까움 → 어두운 장면·포화로 전체 평균 비는 판정 불가, 두 장 모두 정상인 픽셀로 48000/8000µs 비 5.3~5.5
- [x] 컬러바 RAW → PNG에서 색 띠 확인
- [x] 프레임 간격과 sequence 연속성 확인, 13단계 `SensorTimestamp` 결과(49.985ms)와 간격 비교
- [x] 원복: `test_pattern` 0, `rpicam-still` 한 장 정상

## 결과
대표 출력 몇 줄과 판단만 적는다. 원문 로그는 쌓지 않는다.

- 0 빌드·배포: Codex 구현·리뷰 통과(재작업 1회: ioctl 이름 출력, 빈 줄), `make deploy`로 `v4l2_capture`·`raw_tool.py` 배포
- 1 장치·포맷: `m=/dev/media1`, `sd=/dev/v4l-subdev0`, `vd=/dev/video0`. unicam 포맷에 `pRAA`(packed)와 `RG10`(unpacked 16bit) 모두 있음 → `RG10` 사용
- 2 타이밍: `RG10` 3280/2464, `Bytes per Line` 6560, `Size Image` 16163840. 1차는 컨트롤 이름을 `hblank`로 써서 `unknown control` → blanking 미설정(기본값) 상태로 3단계 진행
- 3 노출·게인: 12000µs → 634라인(11984.8µs), 2000µs → 105라인(1984.9µs), 다시 읽은 `exposure` 634·105, `analogue_gain` 0 → 13단계 메타데이터 11984·1984와 같은 라인 수. 캡처 20프레임씩, `sequence_gaps=0`, 평균 간격 66.654ms = 모드 기본 프레임 길이 3526라인(`fll_def`) × 18.9035µs = 66653.8µs → 한 라인 시간(3448 / 182.4M)이 실측 간격과 맞음
- 2 타이밍(재설정): `horizontal_blanking` 168, `vertical_blanking` 181, `pixel_rate` 182400000 → 20프레임 평균 간격 50.000ms(마지막 50.004), `sequence_gaps=0`. 계산 2645라인 × 18.9035µs = 49999.8µs와 일치. 13단계 libcamera(640×480, 20fps) 평균 49.985ms와 차이 0.015ms(0.03%). 모드가 달라 라인 시간이 다르고, libcamera는 `FrameDurationLimits`를 라인 단위로 맞춘 값이라 같은 20fps라도 약간 다를 수 있음(차이의 정확한 원인은 확인 필요)
- 4 테스트 패턴: `test_pattern=1` 상태로 5프레임, 평균 49.998ms, `sequence_gaps=0`, 되돌린 뒤 `test_pattern: 0 (Disabled)`
- 5 RAW: 1차(게인 1) exp12000 평균 0.151, exp2000 0.148(black level 64 차감 후, 0~959 중), 포화 0% → 두 장 모두 black level 수준이라 밝기 비 판정 불가(실내 조도에서 12ms·게인 1이 너무 어두움. 13단계 자동 노출은 66.7ms·게인 9.85). colorbar: R·Gr·Gb·B 모두 479.5, 포화 50% → 각 채널이 1023 또는 64 이하로만 나옴. 8색 표준 컬러바에서 R·G·B가 각각 8개 띠 중 4개에서 켜지므로 50%와 맞음. PNG(Claude 확인): 왼쪽부터 흰·노랑·청록·초록·자홍·빨강·파랑·검정 → RGGB 순서와 2×2 디베이어 정상
- 5 RAW 2차(`analogue_gain=232`): exp12000 평균 9.756, exp2000 2.650(포화 0%), 비 3.68 → 기대 6.0과 다름. 신호가 0~959 중 1% 수준이라 화면은 검게 보이고(감마 없는 선형 PNG), 픽셀마다 black level을 빼고 0으로 자르는 계산이 어두운 영역의 노이즈를 한쪽으로만 남겨 낮은 값일수록 평균을 키움(게인 1에서도 0.15가 남은 이유) → 더 밝은 장면으로 다시 측정
- 5 RAW 3차(조명을 직접 향함, 게인 232, 48000/8000µs = 2539/423라인, 비 6.00): 평균 306.1 / 90.3 = 3.39, 포화 11.1% / 3.9% → 두 장 모두 포화가 커서 긴 노출 쪽 밝은 픽셀이 1023에 막혀 비가 눌림. 전체 평균 비는 판정에서 제외하고, 두 장 모두 포화되지 않은 픽셀만으로 비를 다시 계산함
- 5 RAW 픽셀 선택 비(일회성 확인 명령, 저장소 코드 아님): 긴 노출 < 1000·짧은 노출 − 64 > 30인 픽셀 37.1%에서 평균 비 5.307, 중앙값 5.468. 상한을 800·600으로 낮추면 5.408·5.292로 오히려 작아짐 → 포화 직전 구간 때문은 아님. 노출 라인 수를 늘리면 밝기가 늘어남(비례 방향)은 확인했지만 기대 6.00과 약 9~12% 차이. 원인 후보(확인 필요): 조명 깜빡임(120Hz, 8000µs가 한 주기 8.33ms보다 짧음), 센서의 실제 적분 시간 오프셋. 조명 통제가 필요해 이 단계에서 더 좁히지 않음
- 6 원복: `test_pattern: 0 (Disabled)`, `rpicam-still` 저장 성공(`after.jpg` 1,532,764 B). libcamera가 3280x2464 모드를 고르고 센서 포맷을 `SBGGR10_1X10`으로 설정 → flip을 켜서 Bayer 순서가 RGGB에서 BGGR로 바뀜(직접 캡처는 flip 없이 RGGB)
- 미검증 항목: 노출 선형성의 정확한 비(픽셀 선택 비 5.3~5.5, 기대 6.0. 조명 깜빡임·적분 시간 오프셋 확인 필요), analogue_gain 코드와 실제 배율의 관계(데이터시트 식), 장시간 스트림

완료 후 학습 문서 `docs/learning/13-camera.md`에 섹션을 더하고 "VEDA에서 libcamera로 하던 것 중 커널 인터페이스로 직접 할 수 있는 것과 없는 것" 표를 넣는다.
