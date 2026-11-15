# 카메라 IMX219: 센서·CSI 수신 드라이버와 노출·타임스탬프 경로

## 1. 한눈에 보기
- **목적:** VEDA Final에서 쓴 카메라 모듈을 커스텀 커널 6.18.59에서 대상 커널 드라이버로 연결하고, VEDA 앱이 libcamera로 하던 노출 설정과 타임스탬프 읽기가 커널의 어느 코드로 이어지는지 확인한다.
- **기능:** 13단계는 rpi `imx219` overlay로 `imx219` 센서 드라이버와 unicam CSI-2 수신 드라이버를 연결하고 `rpicam-apps`(libcamera)로 사진·영상을 받는다. 13-2단계는 libcamera 없이 subdev 컨트롤과 직접 작성한 V4L2 캡처 앱으로 같은 일을 한다. 13-3단계는 mainline `imx219` 대신 직접 작성한 고정 모드 센서 드라이버 `imx219_study`를 unicam에 붙여 13-2와 같은 측정을 한다.
- **직접 작성 / 가져다 쓴 것:** 13·13-2는 커널 드라이버·overlay(`imx219.c`, `bcm2835-unicam.c`, `imx219-overlay.dts`)를 그대로 쓰고, 13-2의 V4L2 캡처 앱 [v4l2_capture.c](../../examples/camera/v4l2_capture.c)와 RAW 확인 도구 [raw_tool.py](../../examples/camera/raw_tool.py)를 직접 작성했다. 13-3은 센서 드라이버 [imx219_study.c](../../examples/camera_sensor/imx219_study.c)와 overlay [imx219-study-overlay.dts](../../examples/camera_sensor/imx219-study-overlay.dts)를 직접 작성했고, 레지스터 값은 6.18 `imx219.c`에서 가져왔다. unicam·ISP는 그대로 쓴다.
- **결과(실측):** 노출 12000µs 요청 → 11984µs 적용, 20fps 프레임 간격 평균 49.985ms. 13-2에서 libcamera 없이 같은 노출이 634라인, `vertical_blanking=181`에서 50.000ms·`sequence_gaps=0`. 13-3 직접 작성 드라이버도 634라인·50.001ms·컬러바 채널 평균 479.5로 mainline과 같았다. 상세는 [6장](#6-트러블슈팅과-한계).
- **범위 밖:** 커널 config 변경, 여러 모드·flip·embedded data를 갖춘 센서 드라이버, libcamera에서 직접 작성 드라이버 사용, 자동 노출·화이트밸런스, 화질 처리(ISP), 다른 장치와 동시 사용(→ [18](18-integration.md)).
- **관련 파일:** 작업 노트 [13-camera](../tasks/13-camera.md), [13-2-camera-ctrl](../tasks/13-2-camera-ctrl.md)·[Codex 지시서](../tasks/13-2-camera-ctrl.codex.md), [13-3-camera-sensor](../tasks/13-3-camera-sensor.md)·[Codex 지시서](../tasks/13-3-camera-sensor.codex.md), [Makefile](../../examples/camera/Makefile), [13-3 Makefile](../../examples/camera_sensor/Makefile). 커널 소스는 `external/linux`(Git 대상 아님)의 `drivers/media/i2c/imx219.c`, `drivers/media/platform/bcm2835/bcm2835-unicam.c`, `arch/arm/boot/dts/overlays/imx219-overlay.dts`. VEDA 비교는 `../VEDA_Final_Project/tank/src/libcamera_capture.cpp`

## 2. 구조

**흐름:** IMX219 → (CSI-2 2레인) → unicam → `/dev/video0` RAW → ISP → libcamera → `rpicam-still`/`rpicam-vid`. 설정은 반대로 libcamera → `/dev/v4l-subdev0` → `imx219` → (카메라 I2C) → 센서 레지스터.

```text
 +----------------+  CSI-2 2 lanes  +--------------------+   RAW    +--------------+
 | IMX219 sensor  |---------------->| unicam (fe801000)  |--------->| bcm2835-isp  |
 | 0x10 on i2c-10 |                 | /dev/video0        |          | /dev/media0  |
 +-------^--------+                 | FS IRQ -> ts       |          +------+-------+
         | I2C (CCI) regs           +---------^----------+                 |
 +-------+--------+   media link             |                      +------v-------+
 | imx219.c       |  imx219 -> unicam-image  |                      | libcamera    |
 | v4l2 subdev    |--------------------------+                      | IPA: AE/AWB  |
 | /dev/v4l-sub0  |<------------------ exposure / gain ctrls -------| rpicam-apps  |
 +----------------+                                                 +--------------+
```

13-2는 오른쪽 libcamera 대신 `media-ctl`·`v4l2-ctl`과 직접 작성한 앱이 subdev와 `/dev/video0`을 직접 다룬다. 13-3은 여기에 더해 왼쪽 `imx219.c` 자리를 `imx219_study.c`(엔티티 `imx219-study 10-0010`)로 바꾼다.

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 장치 | IMX219 (Camera Module v2) | 10비트 Bayer RAW 출력, I2C로 레지스터 설정 |
| DT | rpi `imx219` overlay | 카메라 I2C(`i2c0mux/i2c@1` → `i2c-10`)에 `imx219@10`, Unicam 1(`csi@7e801000`)과 port/endpoint 연결, 2레인·24MHz |
| 드라이버 | `imx219` / 13-3 `imx219_study` | V4L2 subdev. 모드 선택, 노출·게인·반전·테스트 패턴을 레지스터 쓰기로 / 고정 모드 1개, 노출·게인·vblank·테스트 패턴만 |
| 드라이버 | unicam (`bcm2835_unicam_legacy`) | CSI-2 프레임을 메모리 버퍼로 받고 프레임 시작 IRQ에서 timestamp |
| 프레임워크 | V4L2 + media controller | `/dev/video0`, `/dev/v4l-subdev0`, `/dev/mediaN`과 엔티티 링크 |
| 사용자 공간 | libcamera·rpicam-apps / 13-2 앱 | 모드·ISP·자동 노출 / 직접 포맷·컨트롤·mmap 캡처 |

## 3. 알아야 할 개념

### 왜 센서 드라이버와 수신 드라이버가 따로인가
센서와 SoC는 두 선으로 연결된다. 제어는 느린 I2C로, 영상은 빠른 CSI-2 레인으로 간다. 그래서 I2C 쪽은 `imx219`(I2C client 드라이버)가, 데이터 쪽은 SoC의 unicam(platform 드라이버)이 맡고, 둘은 DT `port`/`endpoint`로 서로를 찾는다. media controller가 이 연결을 `imx219 10-0010 → unicam-image` 링크로 보여 주며, SoC가 바뀌면 수신 드라이버만 바뀐다.

### 앱의 노출 요청은 어떻게 센서 레지스터까지 가는가
VEDA 앱은 `controls.set(ExposureTime, 12000)`·`AeEnable=false`로 노출을 고정했다(`libcamera_capture.cpp` L198~202). libcamera IPA가 µs를 센서 **라인 수**로 바꿔 subdev 컨트롤 `V4L2_CID_EXPOSURE`로 넘기고, `imx219_set_ctrl()`(`imx219.c` L445)이 I2C로 레지스터에 쓴다. 그래서 subdev `exposure` 범위는 라인 수(4~3522)이고, 적용 값이 요청보다 조금 짧게(11984µs) 나온다.

### 타임스탬프는 어디서 찍히는가
unicam이 프레임 시작 인터럽트(`UNICAM_FSI`)에서 `ktime_get_ns()`를 찍어 버퍼 timestamp로 넣는다(`bcm2835-unicam.c` L937~952, 큐는 `V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC`). libcamera의 `SensorTimestamp`가 이 값이다. 이 시각은 **첫 데이터가 들어온 순간**이라, VEDA 앱은 `SensorTimestamp + 노출/2`로 보정해 IMU 시간축에 맞췄다(L314~335).

### overlay의 data-lanes·link-frequencies·clock-frequency는 드라이버 어디서 쓰이는가
probe 첫 단계 `imx219_check_hwcfg()`(`imx219.c` L1104)가 센서 endpoint 값을 검사하고, 맞지 않으면 `-EINVAL`로 끝난다.

| 속성 | 값(Pi 4) | 쓰이는 곳 |
| --- | --- | --- |
| `data-lanes` | `<1 2>` | 2 또는 4만 받음(L1123). 레인 수로 `pixel_rate`(2레인 182.4M, L542)를 고름. unicam은 스트림 때 센서가 알려 준 레인 수를 씀(L2510) |
| `link-frequencies` | `456000000` | 드라이버 메뉴(L222)와 비교, 없으면 `Link frequency not supported` |
| `clock-frequency` | `cam1_clk` 24MHz | probe가 `clk_get_rate()`로 읽어 24MHz가 아니면 `xclk frequency not supported`(L1201) |

세 값이 맞물려 2레인 × 456MHz × 2(DDR) / 10bit = 182.4M 픽셀/s가 되고, 13-2의 노출 µs 환산과 프레임 간격이 이 값으로 실측과 맞았다.

### 센서 전원·클럭은 어떤 순서로 켜지는가
`imx219_power_on()`(L1017)은 전원(regulator) → 클럭(xclk) → XCLR(리셋 해제) → 6.2ms 대기 순이고, 끌 때(L1054)는 XCLR → 전원 → 클럭이다. Pi 4에서 `VANA`(2.8V)는 GPIO 확장기 5번 핀으로 켜는 `cam1_reg`(`bcm2711-rpi-4-b.dts` L27)이고, overlay에는 `reset-gpios`가 없다. 센서는 `power_on` 동안에만 전원이 들어가므로 드라이버가 없을 때 `i2cdetect -y 10`에 `0x10`이 안 보이는 것은 정상일 수 있다. 13-3 드라이버는 reset GPIO 없이 regulator → xclk → 6.2ms만 하고([imx219_study.c#L130](../../examples/camera_sensor/imx219_study.c#L130)), probe에서 chip ID만 읽은 뒤 끄고 스트림 동안만 켠다.

## 4. 구현 흐름

### 4-1. DT 적용과 probe
firmware가 `imx219` overlay를 적용해 카메라 I2C mux를 켜고 `imx219@10`과 `csi1` 2레인 endpoint를 만든다. 이어서 `imx219_probe()`(L1173)가 아래 순서로 진행하고, unicam이 endpoint를 따라 센서 subdev를 찾아 `/dev/video0`·`/dev/mediaN`을 등록한다.

| 순서 | 하는 일 | 실패할 때 dmesg | 확인할 지점 |
| --- | --- | --- | --- |
| 1 | endpoint 검사 | `endpoint node not found`, `only 2 or 4 data lanes` | overlay 적용, endpoint 값 |
| 2 | xclk 얻기·주파수 검사 | `xclk frequency not supported` | overlay가 `cam1_clk`를 켰는지 |
| 3 | regulator·reset GPIO | `failed to get regulators`, deferred | `/sys/kernel/debug/devices_deferred` |
| 4 | power_on | `failed to enable regulators` | `regulator_summary` |
| 5 | 칩 ID(0x0000 = 0x0219, L1084) | `failed to read chip id 219` | 리본 케이블 방향, `i2c-10`, 전원 GPIO |
| 6 | LP-11 대기 전환, 컨트롤·subdev 등록 | — | — |

이 표는 소스를 읽어 정리했고 실패를 재현하지는 않았다.

### 4-2. libcamera 스트림(13단계)
libcamera가 요청 크기에 맞는 센서 모드를 고른다(640x480 요청 → `SRGGB10_CSI2P 640x480/200.16`). subdev 포맷과 unicam 포맷(`pBAA`)을 맞추고 노출·게인을 쓴 뒤 스트림을 켠다. 프레임마다 unicam이 timestamp를 찍고 버퍼를 채우면, libcamera가 RAW를 `bcm2835-isp`로 보내 YUV로 바꾸고 메타데이터와 함께 앱에 넘긴다.

### 4-3. libcamera 없이(13-2)

| 순서 | 하는 일 | 값·코드 |
| --- | --- | --- |
| 1 | `media-ctl -V`로 센서 pad, `v4l2-ctl --set-fmt-video`로 unicam 포맷 | `SRGGB10_1X10/3280x2464`, `RG10`(한 줄 6560바이트). 포맷을 바꾸면 `vblank`가 기본값으로 돌아가므로(`imx219.c` L900) 그다음에 blanking 설정 |
| 2 | 타이밍 | 한 라인 = (3280 + hblank 168) / 182.4M = 18.9035µs, 프레임 = (2464 + vblank) × 한 라인. vblank 181 → 50.000ms |
| 3 | 노출 | `exposure = floor(µs / 18.9035)`: 12000µs → 634라인. 컨트롤은 스트리밍 중에만 레지스터에 써진다(`imx219.c` L474) |
| 4 | 캡처 | `REQBUFS`(4개) → `mmap` → `QBUF` → `STREAMON`([L110](../../examples/camera/v4l2_capture.c#L110)) → `DQBUF`마다 `sequence`·`timestamp`, 마지막 프레임 저장 → `QBUF`([L114](../../examples/camera/v4l2_capture.c#L114)) |
| 5 | RAW 확인 | [raw_tool.py](../../examples/camera/raw_tool.py#L26)가 black level 64를 뺀 채널 평균·포화 비율, 2×2 묶음 PNG |

VEDA가 libcamera로 하던 것 중 노출·게인 고정, 프레임 길이(`vertical_blanking`), `SensorTimestamp`(V4L2 버퍼 timestamp), RAW 캡처, flip은 커널 인터페이스로 직접 할 수 있다. 디베이어·AWB·감마·자동 노출은 ISP·IPA 영역이라 직접 구현해야 하고, 프레임별 실제 적용 노출은 6.18 `imx219`에 embedded data pad가 없어 얻을 수 없다(L1261).

### 4-4. 직접 작성한 센서 드라이버(13-3)
unicam(레거시)이 센서에서 쓰는 것은 source pad 수, `get_fmt`, `video.s_stream`뿐이고 PIXEL_RATE·LINK_FREQ는 읽지 않는다(`bcm2835-unicam.c` L3099·L644·L2570). 그래서 모드가 하나면 이 셋과 사용자 측정용 컨트롤만 있으면 된다.

| 순서 | 하는 일 | 코드 |
| --- | --- | --- |
| 1 | endpoint가 2레인·456MHz인지, xclk 24MHz인지 확인 | [L93](../../examples/camera_sensor/imx219_study.c#L93), [L378](../../examples/camera_sensor/imx219_study.c#L378) |
| 2 | 전원 on → chip ID `0x0219` → 전원 off | [L389](../../examples/camera_sensor/imx219_study.c#L389) |
| 3 | 컨트롤 7개, source pad 1개, `v4l2_async_register_subdev_sensor` → unicam `.complete`가 `/dev/video0` 등록 | [L403](../../examples/camera_sensor/imx219_study.c#L403) |
| 4 | `get_fmt`·`set_fmt`는 state 없이 고정 포맷 반환 | [L316](../../examples/camera_sensor/imx219_study.c#L316) |
| 5 | `s_stream(1)`: 컨트롤 lock 안에서 전원 on → 레지스터 44개 → 저장된 컨트롤 적용 → MODE_SELECT 1 | [L243](../../examples/camera_sensor/imx219_study.c#L243) |
| 6 | `s_ctrl`: 전원이 켜져 있을 때만 I2C 쓰기, 꺼져 있으면 값만 저장 | [L157](../../examples/camera_sensor/imx219_study.c#L157) |

- 레지스터 테이블([L41](../../examples/camera_sensor/imx219_study.c#L41))은 6.18 `imx219.c`의 `imx219_common_regs` 23개(0x3000 이상 접근 해제, 문서화되지 않은 값 포함), 2레인 PLL 8개, `imx219_set_framefmt()`가 3280×2464 RAW10에서 계산하는 값 13개다. mainline은 크기마다 계산하고, 13-3은 그 결과 하나를 고정했다.
- `s_ctrl`이 `streaming`이 아니라 `powered`를 보는 이유: 스트림 시작 중 `__v4l2_ctrl_handler_setup()`이 부를 때는 아직 `streaming`이 false라, 그 플래그를 보면 미리 설정한 노출·테스트 패턴이 센서에 들어가지 않는다.
- mainline 표준과의 차이: mainline은 `init_state` + `v4l2_subdev_init_finalize()`로 포맷·crop을 코어 state에 두고 `enable_streams`를 `v4l2_subdev_s_stream_helper`로 잇는다. 13-3은 모드가 하나라 상태 API 없이 `.s_stream`을 직접 구현했다(새 upstream 드라이버는 상태 API가 표준).

## 5. 재현

<details><summary>연결 확인 → libcamera 노출·간격 → 13-2 직접 캡처 (펼치기)</summary>

선행 조건: 전원 분리 상태에서 리본 케이블 연결, `camera_auto_detect=1` 기본 설정. 전체 단계는 [작업 노트](../tasks/13-camera.md)에 있다.

```bash
# [Pi] 연결·바인딩·그래프
lsmod | grep -E '^(imx219|bcm2835_unicam_legacy) '
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
media-ctl -d "$m" -p | grep -A2 'imx219'   # imx219 10-0010 -> unicam-image
rpicam-hello --list-cameras                # 0 : imx219 [3280x2464 10-bit RGGB]
```

```bash
# [Pi] 노출 고정 사진·메타데이터, 20fps 영상 프레임 간격
mkdir -p ~/bsp/cam && cd ~/bsp/cam
rpicam-still -n --immediate --shutter 12000 --gain 1 --metadata exp12ms.json -o exp12ms.jpg
grep -E '"(ExposureTime|AnalogueGain)"' exp12ms.json          # 11984, 1.0
rpicam-vid -n -t 5000 --width 640 --height 480 --framerate 20 --shutter 12000 --gain 1 --save-pts pts.txt -o test.h264
awk '!/^#/ { if (n) s += $1 - p; p = $1; n++ } END { printf "frames=%d avg_ms=%.3f\n", n, s / (n - 1) }' pts.txt
```

```bash
# [WSL] ~/bspworkspace/bsp-study — 13-2 앱 빌드·배포
make -C examples/camera deploy       # pi:bsp/apps/v4l2_capture, raw_tool.py
```

```bash
# [Pi] 13-2: rpicam-* 미실행 상태, 위에서 m을 정의한 같은 셸
sd=$(media-ctl -d "$m" -e "imx219 10-0010"); vd=$(media-ctl -d "$m" -e unicam-image)
media-ctl -d "$m" -V '"imx219 10-0010":0 [fmt:SRGGB10_1X10/3280x2464]'
v4l2-ctl -d "$vd" --set-fmt-video=width=3280,height=2464,pixelformat=RG10
v4l2-ctl -d "$sd" --set-ctrl horizontal_blanking=168,vertical_blanking=181,exposure=634,analogue_gain=0
mkdir -p ~/bsp/cam2 && cd ~/bsp/cam2
~/bsp/apps/v4l2_capture "$vd" 20 exp12000.raw | tail -1    # average_interval_ms=50.000 sequence_gaps=0
v4l2-ctl -d "$sd" --set-ctrl test_pattern=1 && ~/bsp/apps/v4l2_capture "$vd" 5 colorbar.raw | tail -1
v4l2-ctl -d "$sd" --set-ctrl test_pattern=0
python3 ~/bsp/apps/raw_tool.py colorbar.raw 3280 2464 --stride 6560 --png colorbar.png
```

```bash
# [WSL] ~/bspworkspace/bsp-study — 13-3 드라이버·overlay 빌드·배포
make -C examples/camera_sensor W=1 && make -C examples/camera_sensor deploy
```

```bash
# [Pi] 13-3: config 백업 후 자동 감지를 끄고 직접 작성 overlay 사용, 재부팅
C=/boot/firmware/config.txt; sudo cp $C $C.bak-13-3
sudo cp ~/bsp/overlays/imx219-study.dtbo /boot/firmware/overlays-bsp-study01/
sudo sed -i 's/^camera_auto_detect=1$/camera_auto_detect=0/' $C && echo "dtoverlay=imx219-study" | sudo tee -a $C
sudo reboot
# 재부팅 후
sudo modprobe -a v4l2_cci v4l2_fwnode && sudo insmod ~/bsp/modules/imx219_study.ko   # dmesg: chip ID: 0x0219
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
sd=$(media-ctl -d "$m" -e "imx219-study 10-0010"); vd=$(media-ctl -d "$m" -e unicam-image)
media-ctl -d "$m" -V '"imx219-study 10-0010":0 [fmt:SRGGB10_1X10/3280x2464]'
v4l2-ctl -d "$vd" --set-fmt-video=width=3280,height=2464,pixelformat=RG10
v4l2-ctl -d "$sd" --set-ctrl test_pattern=0,vertical_blanking=181,exposure=634,analogue_gain=0   # hblank는 읽기 전용 168
~/bsp/apps/v4l2_capture "$vd" 20 ~/bsp/cam3/exp12000.raw | tail -1                           # 50.001ms, gaps 0
# 원복
sudo cp $C.bak-13-3 $C && sudo reboot
```

모듈을 다시 올릴 때는 `sudo rmmod bcm2835_unicam_legacy` → `rmmod imx219_study` → `insmod` → `modprobe bcm2835_unicam_legacy` 순서로 한다(6장 더 보기).

PNG는 WSL에서 `scp 'pi:bsp/cam2/*.png' out/cam2/`로 가져와 본다. 원복은 `test_pattern` 0을 확인하고 `rpicam-still` 한 장을 찍는다(libcamera가 포맷·컨트롤을 다시 설정한다). 명시 overlay를 시험하려면 `config.txt`를 백업한 뒤 `camera_auto_detect=0` + `dtoverlay=imx219`로 바꾸고 재부팅, 확인 후 백업으로 되돌린다.

</details>

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| `lsmod \| grep '^bcm2835_unicam '`에 출력 없음 | 실제 모듈 이름이 `bcm2835_unicam_legacy` | 패턴을 모듈 이름에 맞춤(아래 더 보기) |
| `find /proc/device-tree -name 'imx219@10'` 출력 없음 | `/proc/device-tree`는 심볼릭 링크라 find가 따라가지 않음 | 끝에 `/`를 붙임 |
| 재부팅 뒤 unicam이 `/dev/media2` → `/dev/media0` | media 번호는 probe 순서에 따라 바뀜 | `media-ctl -p`의 driver 이름으로 찾음 |
| 부팅 로그 `Fixed dependency cycle(s)` (csi ↔ imx219) | endpoint 상호 참조를 fw_devlink가 정리한 정보 메시지 | 조치 없음 |
| `--shutter 12000` 사진·13-2 RAW가 매우 어두움 | 실내 조명에 비해 노출·게인 부족(자동 노출은 66.7ms·9.85), PNG는 감마 없는 선형 | 정상. 게인·노출을 올리거나 밝은 장면 |
| `v4l2-ctl --set-ctrl hblank=...`에 `unknown control` | 이름은 `horizontal_blanking`·`vertical_blanking` | `v4l2-ctl -l`로 확인 |
| 13-3 `rmmod imx219_study`가 `Module imx219_study is in use` | unicam에 등록될 때 V4L2 코어가 센서 모듈에 `try_module_get`(`v4l2-device.c` L130) | unicam을 먼저 내림 |
| 13-3 v4l2-compliance 뒤 찍은 장면이 컬러바와 md5까지 같음 | compliance가 컨트롤을 시험하며 `test_pattern`을 켠 채 남긴 것으로 추정(확인 안 함) | `--set-ctrl test_pattern=0`을 명시하고 다시 찍음 |
| 13-3 RAW 장면이 어둡고 초록빛이며 상하 반전 | ISP(화이트 밸런스·감마) 없음, flip 미구현(모듈은 `rotation` 180) | 정상. 사진은 mainline + libcamera로 확인 |
| 노출 6배인데 밝기 비가 3.39·3.68로 작음 | 긴 노출 쪽 포화(11%), 어두운 장면은 black level을 빼고 0으로 자르면 낮은 값의 평균이 커짐 | 두 장 모두 포화·잘림이 없는 픽셀만 사용 → 5.31~5.47 |

**현재 버전 검증 (13: 2026-11-11, 13-2: 2026-11-15, 13-3: 2026-11-16)**

| 항목 | 결과 |
| --- | --- |
| 연결 | `10-0010` → `imx219`, `imx219 10-0010` → `unicam-image`(`/dev/video0`), `imx219 [3280x2464 10-bit RGGB]`, 자동 감지·명시 overlay 같은 결과 |
| libcamera 노출 | 12000 → 11984µs, 2000 → 1984µs(게인 1.0), 자동 노출 66653µs·9.85 |
| libcamera 영상 | 640x480·20fps 96프레임, 평균 간격 49.985ms |
| 13-2 포맷·노출 | `RG10` 3280/2464·6560바이트, 12000µs → 634라인, 2000µs → 105라인 |
| 13-2 간격 | 기본 66.654ms, `vertical_blanking=181`에서 50.000ms, `sequence_gaps=0` |
| 13-2 컬러바 | 채널 평균 모두 479.5, PNG 표준 8색 띠 → RGGB 순서 확인 |
| 13-3 probe·연결 | `chip ID: 0x0219`, mainline `imx219` 미적재, `imx219-study 10-0010` → `unicam-image`, Used by 1, 단독 rmmod 거부, unicam → 센서 해제·적재 3회 오류 없음 |
| 13-3 측정 | 첫 스트림부터 수신(LP-11 토글 없음), 기본 66.651ms, `vertical_blanking=181`에서 50.001ms, 12000µs → 634라인, 컬러바 479.5·포화 50%, `sequence_gaps=0` → 13-2 mainline과 같음 |
| 13-3 v4l2-compliance | `-u /dev/v4l-subdev0` 46개 중 45 통과. 실패 1: 컨트롤 이벤트 구독(`V4L2_SUBDEV_FL_HAS_EVENTS`·`subscribe_event` 없음) |

- 미검증: libcamera 동작 중 subdev 컨트롤 값 변화(메타데이터로만 판단), 사진 방향·초점, 장시간 스트림과 프레임 누락, 노출 선형성의 정확한 비(5.3~5.5 vs 6.0), `analogue_gain` 코드와 실제 배율, mainline `imx219`의 v4l2-compliance 결과, 13-3 드라이버로 스트림 중 노출 변경.
- 한계: 13-2·13-3 RAW 확인은 2×2 디베이어·선형 PNG까지다. 13-3 드라이버는 모드 1개·flip 없음이고, 엔티티 이름이 달라 libcamera rpi 파이프라인에서 쓰지 않는다. Yocto 이미지(6.6.63)의 libcamera 0.4.0·rpicam-apps 1.4.2 통합과 HDMI 미리보기는 [18 문서](18-integration.md)에서 확인했다.

**더 보기**
- 왜 `bcm2835_unicam_legacy` 모듈이 붙는가 → 기본 DT의 `brcm,bcm2835-unicam`(`bcm283x.dtsi` L458)에 rpi 커널 드라이버(`bcm2835-unicam.c` L3425)가 매칭된다. upstream 드라이버는 rpi 커널에서 `brcm,bcm2835-unicam-upstream`에만 매칭된다
- firmware 자동 감지와 명시 overlay는? → `camera_auto_detect=1`은 firmware가 감지해 overlay를 고르고, 명시 방식은 `config.txt`에 남는다. 결과 DT는 같았고 Yocto에서는 명시 방식이 재현에 낫다
- 센서 드라이버는 어디까지 직접 썼나 → 13-3에서 probe·전원·subdev ops·컨트롤·async 등록은 직접 쓰고, 레지스터 값(문서화되지 않은 초기화 포함)은 공개 데이터시트로 채울 수 없어 6.18 `imx219.c`에서 가져왔다. unicam·ISP는 SoC DMA·GPU firmware 영역이라 그대로 쓴다
- 왜 센서 모듈만 rmmod할 수 없나 → `v4l2-async.c` L350 → `__v4l2_device_register_subdev()`가 unicam에 등록된 센서 모듈에 `try_module_get`을 한다. unicam을 내려야 참조가 풀린다
- flip·화질은 누가 맞추나 → flip 레지스터를 컨트롤로 여는 것과 DT `rotation`은 BSP, flip을 켤지·자동 노출·화이트 밸런스·감마는 libcamera IPA와 `bcm2835-isp`
- 13·13-2·13-3은 카메라 업무 중 어디까지인가 → BSP(DT·probe·센서 드라이버·subdev 컨트롤·V4L2 캡처)까지. ISP 튜닝·자동 노출 알고리즘·인코딩·IMU 동기는 하지 않았다
- `clock-noncontinuous`는? → 레인 사이 클럭을 멈출 수 있다는 뜻으로 unicam이 `bus_flags`로 받아 D-PHY 설정에 쓴다(L2326)
- 640x480 모드의 노출 환산은? → analog binning이라 `val / 2`, 한 라인 3560 / 364.8M이어서 같은 12000µs가 1229라인 → 11993.6µs
