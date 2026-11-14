# 카메라 IMX219: 센서·CSI 수신 드라이버와 노출·타임스탬프 경로

13단계는 libcamera(`rpicam-*`)로 드라이버 연결과 노출·타임스탬프 경로를 확인했고, 13-2단계는 libcamera 없이 subdev 컨트롤과 직접 작성한 V4L2 캡처 앱으로 같은 일을 커널 인터페이스에서 했다.

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
- **결과(13-2, 사용자 보드 실행):** libcamera 없이 3280x2464 `RG10` RAW 캡처. 노출 12000µs → 634라인(11984.8µs), 2000µs → 105라인으로 13단계 메타데이터와 같은 라인 수. `vertical_blanking=181`에서 프레임 간격 50.000ms, `sequence_gaps=0`. 테스트 패턴 컬러바로 RGGB 순서 확인. 노출 6배일 때 밝기 비는 5.3~5.5(기대 6.0, 부분 확인).
- **범위:** 커널 config 변경·재빌드, 센서 드라이버 작성, 자동 노출·화이트밸런스, 컬러 영상 출력, 다른 장치와 동시 사용은 하지 않았다.
- **관련 파일:** 작업 노트 [13-camera](../tasks/13-camera.md), [13-2-camera-ctrl](../tasks/13-2-camera-ctrl.md)·[Codex 지시서](../tasks/13-2-camera-ctrl.codex.md). 13-2 코드: [v4l2_capture.c](../../examples/camera/v4l2_capture.c), [raw_tool.py](../../examples/camera/raw_tool.py), [Makefile](../../examples/camera/Makefile). 커널 소스는 `external/linux`(Git 대상 아님): `drivers/media/i2c/imx219.c`, `drivers/media/platform/bcm2835/bcm2835-unicam.c`, `arch/arm/boot/dts/overlays/imx219-overlay.dts`. VEDA 비교: `../VEDA_Final_Project/tank/src/libcamera_capture.cpp`

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

### overlay의 `data-lanes`·`link-frequencies`·`clock-frequency`는 무엇이고 드라이버 어디서 쓰이는가

세 값 모두 rpi `imx219` overlay가 넣는다(`overlays/imx219.dtsi`, `imx219-overlay.dts`). 센서 쪽 endpoint 값은 `imx219_check_hwcfg()`(`imx219.c` L1104)가 probe 첫 단계에서 검사하고, 맞지 않으면 probe가 `-EINVAL`로 끝난다.

| 속성 | 값(Pi 4) | 뜻 | 쓰이는 곳 |
| --- | --- | --- | --- |
| `data-lanes` | 센서·unicam endpoint 모두 `<1 2>` | CSI-2 데이터 레인 수와 번호(클럭 레인은 `clock-lanes = <0>`) | `imx219_check_hwcfg()`가 2 또는 4만 받음(L1123). 레인 수로 `pixel_rate`(2레인 182.4M, 4레인 281.6M, L542)와 link frequency 메뉴를 고름. unicam은 자기 endpoint 레인 수를 최대값으로 잡고(`bcm2835-unicam.c` L3203), 스트림 때 센서가 알려 준 레인 수를 씀(L2510) |
| `link-frequencies` | 센서 endpoint `456000000`(4레인은 363000000) | CSI-2 레인의 클럭 주파수(DDR이라 레인당 912Mbps) | `v4l2_link_freq_to_bitmap()`으로 드라이버 메뉴 `{456000000}`(L222)과 비교, 없으면 `Link frequency not supported`. 읽기 전용 `link_frequency` 컨트롤로 공개(13단계 456MHz) |
| `clock-frequency` | `cam1_clk`(fixed-clock)에 `24000000` | 센서에 들어가는 외부 클럭 xclk | probe가 `clk_get_rate()`로 읽어 24MHz가 아니면 `xclk frequency not supported`(L1201). 레지스터 표의 `EXCK_FREQ`에도 24로 들어감(L191) |

세 값은 서로 맞물린다. 2레인 × 456MHz × 2(DDR) / 10bit = 182.4M 픽셀/s가 `pixel_rate`이고, 13-2에서 노출 µs 환산과 프레임 간격(66.654ms = 3526라인 × 3448 / 182.4M)이 이 값으로 실측과 맞았다. `clock-noncontinuous`는 레인 사이 클럭을 멈출 수 있다는 뜻으로 unicam이 `bus_flags`로 받아 D-PHY 설정에 쓴다(L2326).

### 센서 전원·클럭은 어떤 순서로 켜지고, probe가 실패하면 어디를 보는가

`imx219_power_on()`(`imx219.c` L1017) 순서는 전원(regulator) → 클럭(xclk) → XCLR(리셋 해제) → 6.2ms 대기다. 끌 때(`imx219_power_off()`, L1054)는 XCLR → 전원 → 클럭 순이다.
- **전원:** `regulator_bulk_enable()`로 `VANA`(2.8V)·`VDIG`(1.8V)·`VDDL`(1.2V)를 켠다(L252). Pi 4 DT에서 `VANA`는 `cam1_reg`(regulator-fixed, GPIO 확장기 `expgpio` 5번 핀으로 켬, `bcm2711-rpi-4-b.dts` L27)이고 나머지 둘은 dummy다. 카메라 모듈 전원 스위치가 이 GPIO라 보면 된다
- **클럭:** `clk_prepare_enable(xclk)`. 실패하면 전원을 다시 끔(`goto reg_off`)
- **XCLR:** `reset-gpios`가 있으면 올리고 6.2~7.2ms 기다린다(L306 `IMX219_XCLR_MIN_DELAY_US`). Pi 4 overlay에는 `reset-gpios`가 없다. 카메라 모듈 쪽에서는 `VANA` GPIO(카메라 커넥터의 enable 신호)가 전원과 XCLR을 함께 켜는 것으로 보이나, 모듈 회로는 확인 필요

`imx219_probe()`(L1173) 순서와 실패 메시지:

| 순서 | 하는 일 | 실패할 때 dmesg | 확인할 지점 |
| --- | --- | --- | --- |
| 1 | endpoint 검사(`check_hwcfg`) | `endpoint node not found`, `only 2 or 4 data lanes`, `Link frequency not supported` | overlay 적용 여부(`dtoverlay -l`, `camera_auto_detect`), endpoint 값 |
| 2 | CCI regmap, xclk 얻기·주파수 검사 | `failed to get xclk`, `xclk frequency not supported` | overlay fragment@1이 `cam1_clk`를 켰는지(기본 `disabled`) |
| 3 | regulator·reset GPIO 얻기 | `failed to get regulators`, `failed to get reset gpio`. GPIO 확장기가 아직 없으면 deferred | `/sys/kernel/debug/devices_deferred` |
| 4 | `power_on` | `failed to enable regulators`, `failed to enable clock` | regulator 상태(`/sys/kernel/debug/regulator/regulator_summary`) |
| 5 | 칩 ID 읽기(레지스터 0x0000 = 0x0219, L1084) | `failed to read chip id 219`(I2C 응답 없음), `chip id mismatch` | 리본 케이블 방향·체결, 카메라 I2C 버스(`i2c-10`, `i2c0mux`), 다른 센서 모델 |
| 6 | 스트리밍 → 대기 전환(LP-11), 컨트롤·subdev 등록 | (드물게) I2C 쓰기 실패 | — |

센서는 `power_on` 동안에만 전원이 들어가므로, 드라이버가 없을 때 `i2cdetect -y 10`으로 `0x10`이 안 보이는 것은 정상일 수 있다. 칩 ID 실패는 I2C 배선과 전원 GPIO를 함께 의심한다.

### 카메라 업무에서 13·13-2는 어디까지 했는가

| 영역 | 하는 일 | 13 | 13-2 |
| --- | --- | --- | --- |
| BSP | DT·overlay(레인, link frequency, 클럭, 전원), 드라이버 config·적재, probe 확인 | 기존 overlay로 연결, 자동 감지·명시 overlay 비교 | DT 속성과 probe 경로를 소스로 정리 |
| BSP | 센서 subdev 컨트롤(노출·게인·blanking·테스트 패턴), media 그래프·포맷 | libcamera 경유로만 | **직접 설정**(`v4l2-ctl`, `media-ctl`) |
| BSP | V4L2 캡처(버퍼·timestamp·sequence) | libcamera 경유 | **직접 작성한 앱** |
| ISP·화질 | 디베이어·화이트밸런스·감마·색 보정·노이즈, 튜닝 파일 | libcamera·`bcm2835-isp` 기본 튜닝 사용 | 하지 않음(2×2 확인용 디베이어만) |
| ISP·화질 | 자동 노출·게인 제어 알고리즘 | libcamera 자동 노출 결과만 관찰 | 하지 않음 |
| 앱 | 프레임 처리·저장·인코딩, IMU 시간 동기 | `rpicam-still`·`rpicam-vid` 사용 | 하지 않음 |

### VEDA에서 libcamera로 하던 것 중 커널 인터페이스로 직접 할 수 있는 것과 없는 것

| VEDA(libcamera) | 커널 인터페이스로 직접 | 13-2 확인 |
| --- | --- | --- |
| `ExposureTime`·`AnalogueGain` 고정 | 가능: subdev `exposure`(라인)·`analogue_gain`(코드). µs·배율 변환은 직접 | 라인 수가 13단계와 같음, 밝기 비례 방향 확인 |
| `FrameDurationLimits`(20fps) | 가능: `vertical_blanking`(프레임 길이) | 50.000ms |
| `SensorTimestamp` | 가능: V4L2 버퍼 `timestamp`(unicam FS 인터럽트) | 간격이 13단계와 0.03% 차이 |
| RAW 프레임 | 가능: `media-ctl` 포맷 + V4L2 mmap 스트리밍 | 확인 |
| 센서 방향 보정(flip) | 가능: `horizontal_flip`·`vertical_flip`. 켜면 Bayer 순서가 바뀜(RGGB → BGGR) | libcamera가 `SBGGR10`을 고른 것으로 확인(직접 켜지는 않음) |
| 디베이어·AWB·감마·자동 노출 | 불가(직접 구현해야 함): ISP(`bcm2835-isp`)와 IPA 튜닝 영역 | 하지 않음 |
| 프레임별 실제 적용 노출 보고 | 불가: 6.18 `imx219`는 pad가 1개라 embedded data가 없음(L1261, unicam은 source pad 2개 이상일 때만 메타데이터 노드 등록 `bcm2835-unicam.c` L3122) | — |
| 컨트롤이 몇 번째 프레임부터 적용되는지 | 직접 관리해야 함: 컨트롤은 스트리밍 중에만 레지스터에 써지고(`imx219.c` L474) 센서 지연은 앱이 알아야 함 | 캡처 앞쪽 프레임을 버리고 마지막 프레임을 저장 |

## 4. 구현 흐름

1. **DT 적용:** firmware가 `imx219` overlay를 적용한다. 카메라 I2C 버스와 mux(`i2c0if`, `i2c0mux`)를 켜고, `i2c_csi_dsi` 버스에 `imx219@10`을, `csi1`에 2레인 endpoint를 만든다(`imx219-overlay.dts` fragment@0~@101). 부팅 로그의 `Fixed dependency cycle(s)`는 두 노드가 서로를 endpoint로 가리켜 fw_devlink가 순환을 푼 정보 메시지다.
2. **센서 probe:** `imx219_probe()`(`imx219.c` L1173)가 클럭·전원을 준비하고 I2C로 식별값을 읽은 뒤 subdev를 등록한다. 컨트롤(노출·게인·반전·테스트 패턴, 읽기 전용 회전 180·orientation External)이 이때 만들어진다.
3. **수신기 probe와 그래프:** unicam이 endpoint를 따라 센서 subdev를 찾고 `/dev/video0`·`/dev/mediaN`을 등록한다. `media-ctl -p`에 `imx219 10-0010` → `unicam-image` 링크가 생긴다.
4. **스트림 시작:** libcamera가 요청 크기에 맞는 센서 모드를 고른다(예: 640x480 요청 → `SRGGB10_CSI2P 640x480/200.16`). subdev 포맷과 unicam 포맷(`pBAA`)을 맞추고, 노출·게인 컨트롤을 쓰고 스트림을 켠다.
5. **프레임마다:** 센서가 CSI-2로 프레임을 보내면 unicam이 프레임 시작 IRQ에서 timestamp를 찍고 버퍼를 채운다. libcamera가 RAW를 ISP(`bcm2835-isp`)로 보내 YUV로 바꾸고, 메타데이터(`ExposureTime`·`AnalogueGain`·`SensorTimestamp`)와 함께 앱에 넘긴다.

### 13-2: libcamera 없이

1. **포맷:** `media-ctl -V`로 센서 pad를 `SRGGB10_1X10/3280x2464`로, `v4l2-ctl --set-fmt-video`로 unicam을 `RG10`(unpacked 16bit, 한 줄 6560바이트)으로 맞춘다. 포맷을 바꾸면 드라이버가 `vblank`를 모드 기본값으로 되돌리므로(`imx219.c` L900) 그다음에 blanking을 설정한다.
2. **타이밍:** 한 라인 = `(3280 + horizontal_blanking 168) / pixel_rate 182.4M` = 18.9035µs. 프레임 = `(2464 + vertical_blanking) × 한 라인`. 기본 1062 → 66.654ms, 181 → 50.000ms.
3. **노출:** `exposure = floor(µs / 18.9035)`. 12000µs → 634, 2000µs → 105. 3280x2464는 binning이 없어 레지스터에 그대로 쓰인다(640x480은 analog binning이라 `val / 2`, 한 라인 3560 / 364.8M이라 같은 12000µs가 1229라인 → 11993.6µs로 달라짐). 컨트롤은 저장됐다가 스트림 시작 때 적용된다.
4. **캡처:** [v4l2_capture.c](../../examples/camera/v4l2_capture.c#L70) `REQBUFS`(4개) → `QUERYBUF`·`mmap` → `QBUF` → `STREAMON`([L110](../../examples/camera/v4l2_capture.c#L110)) → `DQBUF`마다 `sequence`·`timestamp`·간격 출력, 마지막 프레임 저장 → `QBUF`([L114](../../examples/camera/v4l2_capture.c#L114)). 끝에 평균 간격과 sequence 끊김 수.
5. **확인:** [raw_tool.py](../../examples/camera/raw_tool.py#L26)가 black level 64를 뺀 채널 평균과 포화 비율을 내고, 2×2 묶음으로 절반 해상도 PNG를 만든다.

## 5. 재현

전체 단계와 확인할 출력은 [작업 노트](../tasks/13-camera.md)의 진행 순서에 있다. 핵심 명령만 모은다.

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

```bash
# [WSL] ~/bspworkspace/bsp-study — 13-2 앱 빌드·배포
make -C examples/camera deploy       # pi:bsp/apps/v4l2_capture, raw_tool.py
```

```bash
# [Pi] 13-2: rpicam-* 미실행 상태, 같은 셸에서
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
sd=$(media-ctl -d "$m" -e "imx219 10-0010"); vd=$(media-ctl -d "$m" -e unicam-image)
media-ctl -d "$m" -V '"imx219 10-0010":0 [fmt:SRGGB10_1X10/3280x2464]'
v4l2-ctl -d "$vd" --set-fmt-video=width=3280,height=2464,pixelformat=RG10
v4l2-ctl -d "$sd" --set-ctrl horizontal_blanking=168,vertical_blanking=181
v4l2-ctl -d "$sd" --set-ctrl exposure=634,analogue_gain=0          # 12000µs, 게인 1
mkdir -p ~/bsp/cam2 && cd ~/bsp/cam2
~/bsp/apps/v4l2_capture "$vd" 20 exp12000.raw | tail -1             # average_interval_ms=50.000 sequence_gaps=0
v4l2-ctl -d "$sd" --set-ctrl test_pattern=1
~/bsp/apps/v4l2_capture "$vd" 5 colorbar.raw | tail -1
v4l2-ctl -d "$sd" --set-ctrl test_pattern=0
python3 ~/bsp/apps/raw_tool.py colorbar.raw 3280 2464 --stride 6560 --png colorbar.png
```

```bash
# [WSL] ~/bspworkspace/bsp-study — PNG 보기
mkdir -p out/cam2 && scp 'pi:bsp/cam2/*.png' out/cam2/
explorer.exe "$(wslpath -w out/cam2)"
```

원복은 `test_pattern` 0을 확인하고 `rpicam-still` 한 장을 찍는다. libcamera가 시작할 때 포맷·컨트롤을 다시 설정한다.

**확인한 값 (13-2)**

| 항목 | 값 |
| --- | --- |
| 포맷 | `RG10` 3280/2464, 한 줄 6560바이트, 16,163,840바이트 |
| 노출 | 12000µs → 634라인, 2000µs → 105라인(다시 읽은 값 같음) |
| 프레임 간격 | 기본 66.654ms, `vertical_blanking=181`에서 50.000ms, `sequence_gaps=0` |
| 컬러바 | 채널 평균 모두 479.5, 포화 50%, PNG 표준 8색 띠 |
| 밝기 비(48000/8000µs, 게인 232) | 전체 평균 3.39(포화 11%·4%로 무효), 두 장 모두 정상인 픽셀 37%에서 5.31~5.47 |

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| `lsmod | grep '^bcm2835_unicam '`에 출력 없음 | 실제 모듈 이름이 `bcm2835_unicam_legacy` | 패턴을 모듈 이름에 맞춤 |
| `find /proc/device-tree -name 'imx219@10'` 출력 없음 | `/proc/device-tree`는 `/sys/firmware/devicetree/base`로 가는 심볼릭 링크라 find가 따라가지 않음 | `find /proc/device-tree/ ...`처럼 끝에 `/` |
| 재부팅 뒤 unicam이 `/dev/media2` → `/dev/media0` | media 장치 번호는 probe 순서에 따라 바뀜 | 번호를 고정하지 않고 `media-ctl -p`의 driver 이름으로 찾음 |
| 부팅 로그 `Fixed dependency cycle(s)` (csi ↔ imx219) | port/endpoint 상호 참조를 fw_devlink가 정리한 정보 메시지 | 조치 없음. probe 실패 메시지가 없는지만 확인 |
| `--shutter 12000` 사진이 매우 어두움 | 실내 조명에 비해 노출·게인 부족(자동 노출은 66.7ms·9.85) | 정상 동작. VEDA처럼 노출을 고정하려면 조명이나 게인을 함께 맞춤 |
| `v4l2-ctl --set-ctrl hblank=...`에 `unknown control` | v4l2-ctl 이름은 `horizontal_blanking`·`vertical_blanking` | `v4l2-ctl -l`로 이름 확인 |
| 13-2 RAW 평균이 black level 수준(0.15), PNG가 검정 | 12ms·게인 1은 실내에서 너무 어둡고, PNG는 감마 없는 선형 저장 | 게인·노출을 올리거나 밝은 장면 |
| 어두운 장면에서 밝기 비가 작게 나옴(3.68) | 픽셀마다 black level을 빼고 0으로 자르면 노이즈의 음수 쪽만 사라져 낮은 값의 평균이 커짐 | 충분히 밝은 픽셀만 골라 비 계산 |
| 조명을 직접 비추면 비가 작게 나옴(3.39) | 긴 노출 쪽 포화(11%)로 밝은 픽셀이 1023에 막힘 | 두 장 모두 포화·잘림이 없는 픽셀만 사용 |

- **미검증:** libcamera 동작 중 subdev 컨트롤 값 변화(13단계, 메타데이터로 판단), 사진 방향·초점 상태, 명시 overlay 단계의 `config.txt` diff 출력, 장시간 스트림과 프레임 누락, 노출 선형성의 정확한 비(5.3~5.5 vs 6.0. 조명 깜빡임·적분 시간 오프셋 확인 필요), `analogue_gain` 코드와 실제 배율(데이터시트 식 확인 필요). probe 실패 표는 소스 읽기로 정리했고 실패를 재현하지는 않았다.
- **한계:** 13-2 RAW 확인은 2×2 디베이어·선형 PNG까지이고 화질 처리는 하지 않는다. Yocto 이미지의 libcamera 패키지 포함 여부는 18단계에서 확인한다.
