# 13 카메라 IMX219

상태: 완료 · 브랜치: `feat/camera` · Codex 지시서: 없음(코드 변경 없음)

## 목표
VEDA Final에서 쓴 카메라 모듈을 커스텀 커널 6.18.59에서 대상 커널의 `imx219`·unicam 드라이버로 연결하고 사진·영상을 받음. VEDA 앱이 libcamera로 하던 노출·게인 설정과 타임스탬프 읽기가 커널의 어느 드라이버 코드로 이어지는지 실측과 소스로 확인함. 끝나면 센서 드라이버(I2C 제어)와 CSI 수신 드라이버(unicam)가 media controller 그래프로 이어지는 구조와, firmware 자동 감지와 명시 overlay의 차이를 설명할 수 있음.

- 보드에서 바뀌는 것: 카메라 연결, 8단계에서만 `config.txt`의 `camera_auto_detect` 값과 `dtoverlay=imx219` 한 줄(백업 후 원복)
- 하지 않는 것: 커널 config 변경·재빌드, 드라이버 작성, libcamera 앱 개발, libcamera 없이 `v4l2-ctl`로 raw 프레임 받기, EIS, Yocto, 다른 장치와 동시 사용 확인(18단계)
- 마무리 때 학습 문서 `examples/camera/camera.md`를 만든다.

## 알아둘 개념
1. **센서와 수신기는 다른 드라이버:** `imx219`(`drivers/media/i2c/imx219.c`)는 카메라 I2C 버스로 센서 레지스터를 설정하는 V4L2 subdev임. 영상 데이터는 CSI-2 레인으로 SoC의 Unicam에 들어가고 Raspberry Pi 커널의 unicam 드라이버(`drivers/media/platform/bcm2835/bcm2835-unicam.c`, 모듈 `bcm2835_unicam_legacy`)가 메모리 버퍼(`/dev/video0`)에 씀. 기본 DT의 `brcm,bcm2835-unicam`(`bcm283x.dtsi` L458)에 이 드라이버가 매칭되고, 같은 이름의 upstream 드라이버(`drivers/media/platform/broadcom/`)는 `brcm,bcm2835-unicam-upstream`에만 매칭되어 쓰이지 않음. 두 드라이버는 DT의 `port`/`endpoint`로 연결되고 media controller 그래프로 보임. 근거: `arch/arm/boot/dts/overlays/imx219-overlay.dts` fragment@100·@101.
2. **밝기 제어 경로:** 앱의 libcamera 컨트롤 `ExposureTime`(µs)·`AnalogueGain` → libcamera IPA가 센서 라인 수 등으로 변환 → V4L2 subdev 컨트롤 `V4L2_CID_EXPOSURE`·`V4L2_CID_ANALOGUE_GAIN` → `imx219_set_ctrl()`이 I2C(CCI)로 레지스터 쓰기. 근거: `imx219.c` L445(set_ctrl), L111(테스트 패턴 레지스터 예).
3. **타임스탬프 출처:** unicam이 프레임 시작 인터럽트(`UNICAM_FSI`)에서 `ktime_get_ns()`를 찍어 버퍼 timestamp로 넣음. 시계는 MONOTONIC. libcamera의 `SensorTimestamp`가 이 값임. 노출 중간 시각이 아니라 첫 데이터 수신 시각. 근거: `bcm2835-unicam.c` L937~952, L2933.

## 진행 순서

### 시작 전 확인한 것 (Claude 읽기 확인)
| 항목 | 값 |
| --- | --- |
| 모델 | VEDA Final `tank/docs/HYBRID_EIS_FRAMEWORK.md` L277에 IMX219 언급, `tank/config/config_template.ini`의 화각 62.2°×48.8°가 Camera Module v2 사양과 같음. **실물 확인 필요**(5단계 `--list-cameras`) |
| config (`out/kernel-rpi4/.config`) | `VIDEO_IMX219=m`, `VIDEO_BCM2835_UNICAM_LEGACY=m`(사용됨), `VIDEO_BCM2835_UNICAM=m`(upstream, 미사용), `MEDIA_CONTROLLER=y`, `V4L2_FWNODE=m`, `VIDEO_ISP_BCM2835=m` |
| binding | `Documentation/devicetree/bindings/media/i2c/sony,imx219.yaml` |
| overlay | `imx219-overlay.dts`: Unicam 1(Pi 4 표준 카메라 포트), CSI 2레인, 24MHz 클럭, 카메라 I2C(`i2c_csi_dsi`). 파라미터 `rotation`(기본 180), `orientation`, `media-controller`(기본 on), `vcm`, `4lane`(CM 전용). overlays README L2990 |
| Pi 배포 (SSH 읽기) | `overlays-bsp-study01/imx219.dtbo` 있음, `config.txt` 24번 줄 `camera_auto_detect=1` |
| Pi 도구 | `rpicam-apps` v1.13.0(`--shutter`·`--gain`·`--metadata`·`--save-pts`·`--framerate` 지원), `media-ctl`, `v4l2-ctl` |
| 카메라 미연결 상태 | `imx219`·`bcm2835_unicam_legacy` 미적재. `/dev/video10~15`·`/dev/media0~3`은 ISP·코덱 장치 |
| VEDA 앱 설정 | `libcamera_capture.cpp` L198~202: `FrameDurationLimits` 50000µs(20fps), `AeEnable=false`, `ExposureTime=12000`(템플릿 값), `AwbEnable=true`. L314~335: `SensorTimestamp` + 노출/2를 `CLOCK_MONOTONIC_RAW`로 변환. 해상도 640×480, `flip=1` |
| IMU와의 관계 | 카메라는 카메라 전용 I2C, IMU는 I2C1(GPIO2/3) → 버스 충돌 없음 |

출력은 **[붙여 주세요]** 단계만 채팅에 붙이면 '결과'에 대표 출력만 옮김. 모든 Pi 명령은 `ssh pi`로 접속한 셸에서 실행.

### 0. 준비
- 카메라 모듈과 15핀 리본 케이블
- Pi 4 CAMERA 포트는 HDMI 포트와 오디오 잭 사이에 있음. 커넥터 걸쇠를 올리고, 케이블의 금속 접점이 HDMI 쪽을 향하게 끝까지 넣고 걸쇠를 누름
- 카메라 모듈 쪽도 같은 방법으로 연결(모듈 보드의 접점 방향 확인)

### 1. 연결 전 기준 확인 (Pi)

```bash
# [Pi]
lsmod | grep -E '^(imx219|bcm2835_unicam_legacy) '                # 출력 없음
ls /dev/video* /dev/media*
grep -n -E 'camera_auto_detect|imx219' /boot/firmware/config.txt
sudo poweroff
```

### 2. 연결과 부팅
- 전원 케이블을 뽑은 상태에서 카메라를 연결함
- 전원을 다시 연결해 부팅(설정 변경 없음 = `camera_auto_detect=1`)

### 3. 드라이버 바인딩 (Pi)

```bash
# [Pi]
uname -r
lsmod | grep -E '^(imx219|bcm2835_unicam_legacy|v4l2_fwnode) '
sudo dmesg | grep -i -E 'imx219|unicam' | head -20
for d in /sys/bus/i2c/devices/*-0010; do echo "$d -> $(readlink -e $d/driver)"; done
find /proc/device-tree/ -name 'imx219@10'    # 끝의 / 필요(/proc/device-tree는 심볼릭 링크)
```

**[붙여 주세요]**

### 4. media 그래프와 장치 노드 (Pi)

```bash
# [Pi]
v4l2-ctl --list-devices
for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "unicam: $m"; done
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1); echo "$m"
media-ctl -d "$m" -p
sd=$(grep -l imx219 /sys/class/video4linux/v4l-subdev*/name | xargs -r dirname | xargs -r basename); echo "/dev/$sd"
v4l2-ctl -d "/dev/$sd" -l
```

**[붙여 주세요]** (`media-ctl -p`는 길면 imx219·unicam 엔티티 부분만)

### 5. 카메라 인식과 사진 (Pi)

```bash
# [Pi]
rpicam-hello --list-cameras
mkdir -p ~/bsp/cam
rpicam-still -n --immediate -o ~/bsp/cam/auto.jpg
ls -l ~/bsp/cam/
```

```bash
# [WSL] ~/bspworkspace/bsp-study — 사진 확인용
mkdir -p out/cam && scp 'pi:bsp/cam/*.jpg' out/cam/
explorer.exe "$(wslpath -w out/cam)"
```

**[붙여 주세요]** `--list-cameras` 출력, 사진이 정상인지(방향·초점 포함)

### 6. 밝기 제어 경로 (Pi)

```bash
# [Pi] VEDA 설정과 같은 노출 12ms, 비교용 2ms. 게인 고정
rpicam-still -n --immediate --shutter 12000 --gain 1 --metadata ~/bsp/cam/exp12ms.json -o ~/bsp/cam/exp12ms.jpg
rpicam-still -n --immediate --shutter 2000  --gain 1 --metadata ~/bsp/cam/exp2ms.json  -o ~/bsp/cam/exp2ms.jpg
grep -E '"(ExposureTime|AnalogueGain|SensorTimestamp)"' ~/bsp/cam/exp12ms.json ~/bsp/cam/exp2ms.json
```

```bash
# [Pi] 카메라 동작 중 커널 subdev 컨트롤 읽기 (4단계와 같은 셸, sd 사용)
for us in 12000 2000; do
    rpicam-hello -n -t 5000 --shutter $us --gain 1 >/dev/null 2>&1 &
    sleep 3
    echo "shutter=$us"; v4l2-ctl -d "/dev/$sd" --get-ctrl exposure,analogue_gain
    wait
done
```

WSL에서 5단계 `scp`를 다시 실행해 두 사진의 밝기를 비교함.

**[붙여 주세요]** `grep`·`v4l2-ctl` 출력, 두 사진 밝기 차이

### 7. 타임스탬프·프레임 간격 (Pi)

```bash
# [Pi] VEDA 설정: 640x480, 20fps, 노출 12ms
rpicam-vid -n -t 5000 --width 640 --height 480 --framerate 20 --shutter 12000 --gain 1 \
    --save-pts ~/bsp/cam/pts.txt -o ~/bsp/cam/test.h264
head -3 ~/bsp/cam/pts.txt
awk '!/^#/ { if (n) s += $1 - p; p = $1; n++ } END { printf "frames=%d avg_ms=%.3f\n", n, s / (n - 1) }' ~/bsp/cam/pts.txt
```

**[붙여 주세요]**

### 8. 명시 overlay 방식 (Pi)

```bash
# [Pi] 백업 후 자동 감지 끄고 imx219 overlay 명시
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-ts01
sudo sed -i 's/^camera_auto_detect=1$/camera_auto_detect=0/' /boot/firmware/config.txt
echo 'dtoverlay=imx219' | sudo tee -a /boot/firmware/config.txt
diff /boot/firmware/config.txt.bak-ts01 /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후
lsmod | grep -E '^(imx219|bcm2835_unicam_legacy) '
for d in /sys/bus/i2c/devices/*-0010; do echo "$d -> $(readlink -e $d/driver)"; done
rpicam-hello --list-cameras
```

**[붙여 주세요]**

```bash
# [Pi] 원복
sudo cp /boot/firmware/config.txt.bak-ts01 /boot/firmware/config.txt
grep -n 'camera_auto_detect' /boot/firmware/config.txt      # camera_auto_detect=1
grep -c '^dtoverlay=imx219' /boot/firmware/config.txt       # 0
sudo reboot
# 재부팅 후
rpicam-hello --list-cameras
```

**[붙여 주세요]**

### 문제가 생기면
- `--list-cameras`에 카메라가 없음: 전원을 끄고 리본 케이블 방향·삽입 깊이·걸쇠를 확인. 3단계 dmesg의 imx219 오류(식별값 읽기 실패 등)를 붙여 줌
- 8단계 후 부팅이 안 됨: SD 카드를 PC 리더로 열어 `config.txt.bak-ts01`을 `config.txt`로 덮어씀
- 사진이 뒤집힘: overlay 기본 `rotation=180`과 모듈 장착 방향의 영향. 이번 TASK에서는 기록만 함

## 완료 기준
- [x] 전원을 끈 상태에서 카메라 연결, 배선 정보를 bringup.md에 기록
- [x] 자동 감지 부팅에서 `imx219`·`bcm2835_unicam_legacy` 적재, 카메라 I2C 장치 `*-0010`이 `imx219`에 바인딩
- [x] `media-ctl -p`에서 `imx219` 엔티티 → unicam `unicam-image` 노드 연결 확인
- [x] `rpicam-hello --list-cameras`에 `imx219`와 모드 목록 → 모델 확정
- [x] 노출 12ms·2ms 사진 두 장의 밝기 차이를 사용자가 확인, `--metadata`의 `ExposureTime`·`AnalogueGain`이 요청값과 맞음
- [ ] (생략, 미검증) 카메라 동작 중 센서 subdev의 `exposure`·`analogue_gain` 컨트롤 값이 `--shutter`에 따라 바뀜(동시 접근이 안 되면 미검증으로 두고 메타데이터로 판단)
- [x] 640×480·20fps 5초 영상의 `--save-pts` 평균 간격이 약 50ms
- [x] 명시 overlay(`camera_auto_detect=0` + `dtoverlay=imx219`)로도 같은 바인딩·`--list-cameras` 결과, 이후 원복

## 결과
대표 출력 몇 줄과 판단만 적는다.

**1~5단계 (자동 감지 `camera_auto_detect=1`, 사용자 실행)**
- 바인딩: `10-0010 -> /sys/bus/i2c/drivers/imx219`, 모듈 `imx219`·`v4l2_fwnode`·`bcm2835_unicam_legacy` 적재. 노드 경로 `/soc/i2c0mux/i2c@1/imx219@10`(dmesg·libcamera 출력)
- dmesg: `Fixed dependency cycle(s)` between `/soc/csi@7e801000` ↔ `imx219@10` — 두 노드가 port/endpoint로 서로를 참조해 fw_devlink가 순환을 풀었다는 정보 메시지로 봄. probe 실패 메시지 없음
- media 그래프(`/dev/media2`, driver `unicam`, `platform:fe801000.csi`): `imx219 10-0010`(`/dev/v4l-subdev0`, `SRGGB10_1X10/3280x2464`) → `unicam-image`(`/dev/video0`) [ENABLED,IMMUTABLE]
- subdev 컨트롤: `exposure` 4~3522(기본 1600, 라인 단위), `analogue_gain` 0~232, `digital_gain` 256~4095, `test_pattern` 메뉴 0~4, `camera_sensor_rotation` 180(읽기 전용), `link_frequency` 456MHz, `pixel_rate` 182.4M
- 모델 확정: `0 : imx219 [3280x2464 10-bit RGGB]`, 모드 640x480@200.16 / 1640x1232@81.07 / 1920x1080@47.57 / 3280x2464@21.19 fps. libcamera v0.7.2, 튜닝 `imx219.json`, Unicam `/dev/media2` + ISP `/dev/media0`
- 사진: `rpicam-still`로 3280x2464 `auto.jpg` 2,561,134 B 저장, 사용자가 이미지 확인
- 진행 중 정정: unicam 모듈 이름이 `bcm2835_unicam_legacy`라 1·3·8단계 `lsmod` 패턴을 고침. `find /proc/device-tree`는 심볼릭 링크라 출력이 없었고 끝에 `/`를 붙이도록 고침

**6단계 밝기 (사용자 화면 확인)**
- 사진 밝기: `auto.jpg`(자동 노출)는 평소 밝기, `--shutter 12000 --gain 1`은 매우 어두움, `--shutter 2000 --gain 1`은 거의 보이지 않음 → 노출 시간을 줄이면 밝기가 그만큼 떨어짐. 실내 조명에서는 자동 노출이 12ms·게인 1보다 긴 노출이나 높은 게인을 고른 것으로 봄(메타데이터로 확인 필요)
- 메타데이터: 요청 12000µs → `ExposureTime` 11984, 요청 2000µs → 1984, 둘 다 `AnalogueGain` 1.0. 요청값이 센서 라인 단위로 맞춰져 약간 짧아짐
- 자동 노출(`auto.json`): `ExposureTime` 66653(약 66.7ms), `AnalogueGain` 9.85 → 12ms·게인 1보다 노출 약 5.6배 × 게인 약 9.8배라 밝기 차이가 설명됨
- subdev 컨트롤 동시 읽기: 카메라 동작이 짧아 함께 실행하기 어려워 생략(미검증)
- 재부팅 후 media 번호가 바뀜: unicam `/dev/media2` → `/dev/media0`, ISP `/dev/media0` → `/dev/media1`. 장치 번호를 고정값으로 쓰지 않음

**7단계 프레임 간격 (640x480, 20fps, 노출 12ms)**
- 센서 모드 `SRGGB10_CSI2P 640x480/200.16` 선택, unicam `640x480-pBAA`
- `frames=96 avg_ms=49.985` → 20fps 주기(50ms)와 일치. 첫 두 줄 `0.000`, `49.985`

**8단계 명시 overlay와 원복**
- 명시 overlay 재부팅 후: `imx219`·`bcm2835_unicam_legacy` 적재, `10-0010 -> imx219`, `--list-cameras` 결과가 자동 감지 때와 같음
- 원복 재부팅 후: `--list-cameras` 동일
- `config.txt` `diff`·원복 후 `grep` 출력은 미전달

- 미검증: subdev 컨트롤 값이 `--shutter`에 따라 바뀌는지(메타데이터로 대신 판단), 사진 방향·초점 상태(사용자 확인 내용 미기록), 8단계 `config.txt` diff 출력
- 마무리: 학습 문서 [camera.md](../../examples/camera/camera.md), bringup 카메라 연결, worklog·roadmap 갱신
