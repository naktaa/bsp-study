# 카메라 IMX219: 센서·CSI 수신 드라이버와 노출·타임스탬프 경로

- [1. 한눈에 보기](#1-한눈에-보기)
- [2. 구조](#2-구조)
- [3. 알아야 할 개념](#3-알아야-할-개념)
- [4. 구현 흐름](#4-구현-흐름)
- [5. 재현](#5-재현)
- [6. 트러블슈팅과 한계](#6-트러블슈팅과-한계)

## 1. 한눈에 보기

- **목적:** VEDA Final에서 쓴 카메라 모듈을 커스텀 커널 6.18.59에서 대상 커널의 드라이버로 연결하고, VEDA 앱이 libcamera로 하던 노출 설정과 타임스탬프 읽기가 커널의 어느 코드로 이어지는지 확인한다.
- **기능:** 코드 작성 없이 `imx219` 센서 드라이버와 Raspberry Pi 커널의 unicam CSI-2 수신 드라이버를 rpi `imx219` overlay로 연결하고, `rpicam-apps`(libcamera)로 사진·영상을 받는다.
- **결과(사용자 보드 실행):** `10-0010`이 `imx219`에 바인딩, media 그래프 `imx219 10-0010` → `unicam-image`(`/dev/video0`), 모델 `imx219 [3280x2464 10-bit RGGB]` 확정, 노출 요청값 적용(12000 → 11984µs), 640x480·20fps 영상의 프레임 간격 평균 49.985ms, firmware 자동 감지와 명시 overlay 모두 같은 결과.
- **범위:** 커널 config 변경·재빌드, 드라이버 작성, libcamera 앱 개발, 다른 장치와 동시 사용은 하지 않았다.
- **관련 파일:** 작업 노트 [13-camera](../../docs/tasks/13-camera.md). 커널 소스는 `external/linux`(Git 대상 아님): `drivers/media/i2c/imx219.c`, `drivers/media/platform/bcm2835/bcm2835-unicam.c`, `arch/arm/boot/dts/overlays/imx219-overlay.dts`. VEDA 비교: `../VEDA_Final_Project/tank/src/libcamera_capture.cpp`

## 2. 구조

**흐름:** IMX219 센서 → (CSI-2 2레인) → unicam → `/dev/video0` RAW → ISP → libcamera → `rpicam-still`/`rpicam-vid`. 설정은 반대 방향으로 libcamera → `/dev/v4l-subdev0` → `imx219` → (카메라 I2C) → 센서 레지스터.

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

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 장치 | IMX219 (Camera Module v2 센서) | 빛을 받아 10비트 Bayer RAW 출력, I2C로 레지스터 설정 |
| DT | rpi `imx219` overlay | 카메라 I2C(`i2c0mux/i2c@1` → `i2c-10`)에 `imx219@10`, Unicam 1(`csi@7e801000`)과 port/endpoint 연결, 2레인·24MHz 클럭 |
| 드라이버 | `imx219` | V4L2 subdev. 포맷·모드 선택, 노출·게인·반전·테스트 패턴 컨트롤을 레지스터 쓰기로 바꿈 |
| 드라이버 | unicam (`bcm2835_unicam_legacy`) | CSI-2 프레임을 메모리 버퍼로 받고 프레임 시작 IRQ에서 timestamp 기록 |
| 프레임워크 | V4L2 + media controller | `/dev/video0`, `/dev/v4l-subdev0`, `/dev/mediaN`과 엔티티 연결 공개 |
| 사용자 공간 | libcamera, rpicam-apps | 센서 모드 선택, ISP 처리, 자동 노출·화이트밸런스, JPEG·H.264 저장 |

## 3. 알아야 할 개념

### 왜 센서 드라이버와 수신 드라이버가 따로인가

센서와 SoC는 두 가지 선으로 연결된다. 제어는 느린 I2C로, 영상 데이터는 빠른 CSI-2 레인으로 간다. 그래서 I2C 쪽은 `imx219`(I2C client 드라이버)가, 데이터 쪽은 SoC의 unicam(platform 드라이버)이 맡는다. 두 드라이버는 DT의 `port`/`endpoint`로 서로를 찾고, media controller가 이 연결을 `imx219 10-0010 → unicam-image` 링크로 보여 준다. 같은 센서라도 SoC가 바뀌면 수신 드라이버만 바뀐다.

### 왜 `bcm2835_unicam_legacy` 모듈이 붙는가

대상 커널에는 unicam 드라이버가 둘 있다. 기본 DT의 Unicam 노드는 `compatible = "brcm,bcm2835-unicam"`(`bcm283x.dtsi` L458)이다. 이 이름에는 Raspberry Pi 커널의 드라이버(`drivers/media/platform/bcm2835/bcm2835-unicam.c` L3425, 모듈 `bcm2835-unicam-legacy`, `CONFIG_VIDEO_BCM2835_UNICAM_LEGACY`)가 매칭된다. upstream 드라이버(`drivers/media/platform/broadcom/`)는 rpi 커널에서 `brcm,bcm2835-unicam-upstream`에만 매칭되어 쓰이지 않는다. 모듈 이름의 "legacy"와 매칭 방식이 반대처럼 보이므로 `lsmod`로 실제 모듈을 확인한다.

### 앱의 노출 요청은 어떻게 센서 레지스터까지 가는가

VEDA 앱은 `controls.set(ExposureTime, 12000)`·`AeEnable=false`로 노출을 고정했다(`libcamera_capture.cpp` L198~202). `rpicam-still --shutter 12000 --gain 1`도 같은 libcamera 컨트롤을 쓴다. libcamera IPA가 µs를 센서 라인 수로 바꿔 subdev 컨트롤 `V4L2_CID_EXPOSURE`로 넘기고, `imx219_set_ctrl()`(`imx219.c` L445)이 I2C로 레지스터에 쓴다. 그래서 subdev `exposure` 컨트롤의 범위는 µs가 아니라 라인 수(4~3522)이고, 메타데이터의 `ExposureTime`이 요청값보다 조금 짧게(11984µs) 나온다.

### 타임스탬프는 어디서 찍히는가

unicam이 프레임 시작 인터럽트(`UNICAM_FSI`)에서 `ktime_get_ns()`를 찍어 버퍼 timestamp로 넣는다(`bcm2835-unicam.c` L937~952). 큐는 `V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC`이다(L2933). libcamera의 `SensorTimestamp`가 이 값이다. 이 시각은 **첫 데이터가 들어온 순간**이지 노출 중간이 아니다. VEDA 앱이 `SensorTimestamp + 노출/2`로 보정한 뒤 IMU 시간축(`CLOCK_MONOTONIC_RAW`)에 맞춘 이유다(L314~335).

### firmware 자동 감지와 명시 overlay는 무엇이 다른가

`camera_auto_detect=1`이면 firmware가 부팅 때 카메라를 감지해 맞는 overlay(여기서는 `imx219`)를 스스로 적용한다. `camera_auto_detect=0` + `dtoverlay=imx219`는 사람이 DT를 고르는 방식이다. 결과 DT는 같아서 바인딩·카메라 목록도 같았다. 자동 감지는 편하지만 어떤 overlay가 쓰였는지 `config.txt`에 남지 않는다. Yocto 이미지에서는 명시 방식으로 고정하는 편이 재현에 낫다.

### 왜 카메라 드라이버는 직접 작성하지 않았는가

`imx219.c`(1,348줄) 자체는 I2C 드라이버 구조라 읽을 수 있지만, 모드별 레지스터 설정 순서가 공개 데이터시트에 충분히 나오지 않는다. unicam(3,445줄)은 SoC DMA·CSI-2를, ISP는 GPU firmware·libcamera 튜닝을 다룬다. BSP에서는 보통 검증된 드라이버를 DT로 붙이고 레인 수·클럭·방향·전원 같은 보드 차이를 맞춘다. 그 연습이 이 단계의 목표다.

## 4. 구현 흐름

1. **DT 적용:** firmware가 `imx219` overlay를 적용한다. 카메라 I2C 버스와 mux(`i2c0if`, `i2c0mux`)를 켜고, `i2c_csi_dsi` 버스에 `imx219@10`을, `csi1`에 2레인 endpoint를 만든다(`imx219-overlay.dts` fragment@0~@101). 부팅 로그의 `Fixed dependency cycle(s)`는 두 노드가 서로를 endpoint로 가리켜 fw_devlink가 순환을 푼 정보 메시지다.
2. **센서 probe:** `imx219_probe()`(`imx219.c` L1173)가 클럭·전원을 준비하고 I2C로 식별값을 읽은 뒤 subdev를 등록한다. 컨트롤(노출·게인·반전·테스트 패턴, 읽기 전용 회전 180·orientation External)이 이때 만들어진다.
3. **수신기 probe와 그래프:** unicam이 endpoint를 따라 센서 subdev를 찾고 `/dev/video0`·`/dev/mediaN`을 등록한다. `media-ctl -p`에 `imx219 10-0010` → `unicam-image` 링크가 생긴다.
4. **스트림 시작:** libcamera가 요청 크기에 맞는 센서 모드를 고른다(예: 640x480 요청 → `SRGGB10_CSI2P 640x480/200.16`). subdev 포맷과 unicam 포맷(`pBAA`)을 맞추고, 노출·게인 컨트롤을 쓰고 스트림을 켠다.
5. **프레임마다:** 센서가 CSI-2로 프레임을 보내면 unicam이 프레임 시작 IRQ에서 timestamp를 찍고 버퍼를 채운다. libcamera가 RAW를 ISP(`bcm2835-isp`)로 보내 YUV로 바꾸고, 메타데이터(`ExposureTime`·`AnalogueGain`·`SensorTimestamp`)와 함께 앱에 넘긴다.

## 5. 재현

전체 단계와 확인할 출력은 [작업 노트](../../docs/tasks/13-camera.md)의 진행 순서에 있다. 핵심 명령만 모은다.

```bash
# [Pi] 연결(전원 분리 상태에서 리본 케이블) 후 부팅. camera_auto_detect=1 기본 설정
lsmod | grep -E '^(imx219|bcm2835_unicam_legacy) '
for d in /sys/bus/i2c/devices/*-0010; do echo "$d -> $(readlink -e $d/driver)"; done
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
media-ctl -d "$m" -p                       # imx219 10-0010 -> unicam-image
rpicam-hello --list-cameras                # 0 : imx219 [3280x2464 10-bit RGGB]
```

```bash
# [Pi] 노출 고정 사진과 메타데이터, 20fps 영상 프레임 간격
mkdir -p ~/bsp/cam
rpicam-still -n --immediate --shutter 12000 --gain 1 --metadata ~/bsp/cam/exp12ms.json -o ~/bsp/cam/exp12ms.jpg
grep -E '"(ExposureTime|AnalogueGain)"' ~/bsp/cam/exp12ms.json
rpicam-vid -n -t 5000 --width 640 --height 480 --framerate 20 --shutter 12000 --gain 1 \
    --save-pts ~/bsp/cam/pts.txt -o ~/bsp/cam/test.h264
awk '!/^#/ { if (n) s += $1 - p; p = $1; n++ } END { printf "frames=%d avg_ms=%.3f\n", n, s / (n - 1) }' ~/bsp/cam/pts.txt
```

**확인한 값**

| 촬영 | ExposureTime (µs) | AnalogueGain | 밝기 |
| --- | --- | --- | --- |
| `--shutter 12000 --gain 1` | 11984 | 1.0 | 매우 어두움 |
| `--shutter 2000 --gain 1` | 1984 | 1.0 | 거의 안 보임 |
| 자동 노출 | 66653 | 9.85 | 평소 밝기 |

실내 조명에서 자동 노출은 12ms·게인 1보다 노출 약 5.6배 × 게인 약 9.8배를 골랐다. 640x480·20fps 영상은 96프레임, 평균 간격 49.985ms였다.

```bash
# [Pi] 명시 overlay로 바꿔 보기(백업 후), 확인 뒤 원복
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-ts01
sudo sed -i 's/^camera_auto_detect=1$/camera_auto_detect=0/' /boot/firmware/config.txt
echo 'dtoverlay=imx219' | sudo tee -a /boot/firmware/config.txt
sudo reboot
# 재부팅 후 rpicam-hello --list-cameras 확인, 이어서 원복
sudo cp /boot/firmware/config.txt.bak-ts01 /boot/firmware/config.txt
sudo reboot
```

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| `lsmod | grep '^bcm2835_unicam '`에 출력 없음 | 실제 모듈 이름이 `bcm2835_unicam_legacy` | 패턴을 모듈 이름에 맞춤 |
| `find /proc/device-tree -name 'imx219@10'` 출력 없음 | `/proc/device-tree`는 `/sys/firmware/devicetree/base`로 가는 심볼릭 링크라 find가 따라가지 않음 | `find /proc/device-tree/ ...`처럼 끝에 `/` |
| 재부팅 뒤 unicam이 `/dev/media2` → `/dev/media0` | media 장치 번호는 probe 순서에 따라 바뀜 | 번호를 고정하지 않고 `media-ctl -p`의 driver 이름으로 찾음 |
| 부팅 로그 `Fixed dependency cycle(s)` (csi ↔ imx219) | port/endpoint 상호 참조를 fw_devlink가 정리한 정보 메시지 | 조치 없음. probe 실패 메시지가 없는지만 확인 |
| `--shutter 12000` 사진이 매우 어두움 | 실내 조명에 비해 노출·게인 부족(자동 노출은 66.7ms·9.85) | 정상 동작. VEDA처럼 노출을 고정하려면 조명이나 게인을 함께 맞춤 |

- **미검증:** 카메라 동작 중 subdev `exposure`·`analogue_gain` 컨트롤 값 변화(동시 실행이 어려워 생략하고 메타데이터로 판단), 사진 방향·초점 상태, 명시 overlay 단계의 `config.txt` diff 출력, `test_pattern` 컨트롤, 장시간 스트림과 프레임 누락.
- **한계:** libcamera 없이 `v4l2-ctl`로 RAW를 직접 받는 경로는 다루지 않았다. Yocto 이미지의 libcamera 패키지 포함 여부는 18단계에서 확인한다.
