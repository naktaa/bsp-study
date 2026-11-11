# 하드웨어 모듈 사양·데이터시트

VEDA Final에서 사용한 모듈을 bsp-study에서 BSP 관점으로 다루기 위한 기준 자료다. 배선·핀 할당은 [bringup.md](bringup.md)에서만 관리하고, 이 문서는 **모듈 사양, 인터페이스, 커널 쪽 대응, 데이터시트 링크**를 정리한다.

- 값은 데이터시트 기준이다. 실물 모듈(브레이크아웃 기판)의 레귤레이터·풀업·레벨 변환 회로는 판매사마다 다르므로 실물로 확인한다.
- "확인 필요"는 아직 실물 모델이나 커널 소스로 확인하지 않은 항목이다.
- 커널 쪽 경로는 대상 커널(6.18.59, `external/linux`) 기준이며 roadmap에서 확인한 항목이다.

## 모듈 목록

| roadmap | 모듈 | 인터페이스 | Pi 쪽 커널 대응 | 모델 확정 |
| --- | --- | --- | --- | --- |
| 완료 | GY-521 / MPU-6050 IMU | I2C1 `0x68` + INT(GPIO17) | 직접 작성: `examples/mpu6050`, `examples/mpu6050_iio` (mainline: `drivers/iio/imu/inv_mpu6050`) | 확정 |
| 완료(13) | 카메라 IMX219 | MIPI CSI-2 + I2C(카메라 전용 `i2c-10` `0x10`) | `imx219` + Raspberry Pi unicam(`bcm2835_unicam_legacy`), `imx219` overlay | 센서 확정(실측), 모듈 제품 확인 필요 |
| 14 | INMP441 I2S MEMS 마이크 | I2S(PCM) 수신 | `bcm2835-i2s` + ASoC 코덱 후보(아래) | 확정 |
| 15 | PCA9685 + SG90 ×2 (팬·틸트) | I2C1 `0x40` | `pwm-pca9685`, `i2c-pwm-pca9685a` overlay | 확정(VEDA 설정 `i2c_address=64`) |
| 16 | 초음파 HC-SR04 계열 | GPIO Trig + GPIO Echo(IRQ) | IIO `srf04` (`devantech,srf04`) | 확인 필요 |
| 17 | DC 모터 + 모터 드라이버 | 하드웨어 PWM(EN) + GPIO(IN1/IN2) ×2 | `pwm-bcm2835`, `pwm-2chan` overlay | 드라이버 칩 확인 필요 |

## GY-521 / MPU-6050

| 항목 | 값 |
| --- | --- |
| 기능 | 3축 가속도 + 3축 자이로, 내장 FIFO 1024B, Data Ready 인터럽트 |
| 인터페이스 | I2C 최대 400kHz, 주소 `0x68`(AD0=LOW) / `0x69`(AD0=HIGH) |
| 식별 | `WHO_AM_I`(0x75) = `0x68` |
| 현재 설정 | 100Hz, ±2g(16384 LSB/g), ±250deg/s(131 LSB/(deg/s)), INT active HIGH·LATCH |
| 주의 | GY-521 기판의 전원 회로·AD0 풀다운은 판매사마다 다름 → [bringup.md](bringup.md#ad0와-i2c-주소) |

- [MPU-6000/6050 Product Specification (TDK)](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf)
- [MPU-6000/6050 Register Map Rev 4.2 (미러)](https://shop.cpu.com.tw/upload/2013/03/RM-MPU-6000A.pdf)

## 카메라 IMX219

| 항목 | 값 |
| --- | --- |
| 센서 | Sony IMX219, 8MP(3280×2464), 1.12µm 픽셀 |
| 인터페이스 | MIPI CSI-2 2-lane 영상 + 센서 제어용 I2C. Pi 4의 CAM 커넥터(15핀 FPC) |
| 커널 | `drivers/media/i2c/imx219.c`(센서 서브디바이스) → Raspberry Pi unicam `drivers/media/platform/bcm2835/bcm2835-unicam.c`(모듈 `bcm2835_unicam_legacy`, CSI-2 수신) → V4L2 / media controller. upstream `drivers/media/platform/broadcom/`는 rpi 커널에서 `-upstream` compatible에만 매칭되어 쓰이지 않음 |
| 부팅 설정 | 기본 `camera_auto_detect=1`이면 펌웨어가 인식한 카메라의 overlay를 자동 적용한다. 수동으로 다룰 때는 `camera_auto_detect=0` + `dtoverlay=imx219`. 두 방식 모두 같은 결과를 실측함 |
| 실측 | `imx219 [3280x2464 10-bit RGGB]`, 모드 640x480@200 / 1640x1232@81 / 1920x1080@47.6 / 3280x2464@21.2 fps, subdev `exposure` 4~3522(라인), `analogue_gain` 0~232 → [카메라 문서](../examples/camera/camera.md) |
| VEDA 사용 | libcamera 640×480@20fps, `hfov_deg=62.2`, `vfov_deg=48.8` |
| 확인 필요 | 실물이 공식 Camera Module 2인지 호환 모듈인지(호환 모듈은 overlay 옵션이 다를 수 있음) |

- Sony는 IMX219 데이터시트를 공개 배포하지 않는다. 레지스터 수준은 커널 드라이버 소스를 기준으로 본다.
- [Raspberry Pi Camera Module 2 제품 페이지](https://www.raspberrypi.com/products/camera-module-v2)
- [config.txt 문서 (camera_auto_detect, dtoverlay)](https://www.raspberrypi.com/documentation/computers/config_txt.html)

## INMP441 I2S MEMS 마이크

| 항목 | 값 |
| --- | --- |
| 기능 | 전방향 MEMS 마이크, 디지털 I2S 출력 |
| 전원 | 1.8~3.3V (절대 범위 1.62~3.63V) → Pi 3.3V 사용 |
| 데이터 형식 | I2S 슬레이브, 24bit 2의 보수, MSB first, WS 한 프레임 64 SCK(채널당 32 SCK), MSB는 1 SCK 지연(표준 I2S) |
| 클럭 | SCK 0.5~3.2MHz, WS(샘플레이트) 7.8~50kHz. 48kHz면 SCK 3.072MHz |
| L/R 핀 | LOW → 왼쪽 채널, HIGH → 오른쪽 채널. LSB 뒤 SD를 tri-state로 두어 두 개를 한 선에 공유 가능 |
| 기동 | 전원 인가 후 2^18 SCK(48kHz 기준 약 85ms) 동안 출력 0 |
| 성능 | 감도 −26dBFS(94dB SPL, 1kHz), SNR 61dBA |

- **Pi가 I2S 마스터**로 BCLK·LRCLK를 공급하고 마이크는 SD로 데이터만 보낸다. 모듈 핀: VDD, GND, SCK→BCLK, WS→LRCLK, SD→DIN, L/R.
- Pi 4 I2S(PCM) 핀은 GPIO18(BCLK), GPIO19(LRCLK), GPIO20(DIN), GPIO21(DOUT)이다. **아래 모터 핀 충돌 참고.**
- 24bit 데이터가 32bit 슬롯에 들어가므로 ALSA 캡처는 보통 `S32_LE`로 받는다.
- 커널 코덱 후보(14단계에서 확정): `sound/soc/codecs/ics43432.c`(같은 형식의 I2S MEMS 마이크용), `dmic.c`, simple-audio-card. 기존 overlay 후보: `googlevoicehat-soundcard`, `adau7002-simple`. INMP441 전용 mainline 드라이버는 확인 필요.
- [INMP441 제품 페이지 (TDK InvenSense)](https://invensense.tdk.com/?p=582)
- [INMP441 데이터시트 (미러)](https://microcontroller-course.web.cern.ch/Project_Material/resources/INMP441.pdf)

## PCA9685 + SG90 (팬·틸트)

| 항목 | PCA9685 |
| --- | --- |
| 기능 | 16채널 12bit PWM 컨트롤러(I2C) |
| 전원 | VDD 2.3~5.5V. 서보 전원(V+)은 별도 공급 |
| 주소 | 상위 고정 1 + A5~A0 → 기판 기본 `0x40`(VEDA 설정 `i2c_address=64`). All Call `0x70`이 기본 활성이므로 같은 버스의 다른 장치 주소와 겹치지 않는지 확인 |
| 클럭 | 내부 오실레이터 25MHz |
| 주파수 | `PRE_SCALE`(0xFE) = round(25MHz / (4096 × f)) − 1, 약 24~1526Hz. **MODE1 SLEEP=1 상태에서만 PRE_SCALE 쓰기 가능** |
| 채널 레지스터 | LEDn_ON_L/H, OFF_L/H = `0x06 + 4n` ~ `0x09 + 4n` |
| 출력 | 기본 토템폴, MODE2 OUTDRV로 오픈드레인 선택 |

| 항목 | SG90 |
| --- | --- |
| 전원 | 약 4.8~6V (5V 기준) |
| 제어 | 주기 20ms(50Hz), 펄스 폭 약 1ms~2ms(판매사 표기 500~2400µs), 중앙 1.5ms |
| 회전 | 약 180° |
| VEDA 사용 | 50Hz, pan=ch0, tilt=ch1, 중앙 90° |

- 커널: `drivers/pwm/pwm-pca9685.c` → pwmchip 16채널. sysfs `/sys/class/pwm/pwmchipN`에서 period 20,000,000ns, duty 1,000,000~2,000,000ns로 제어.
- 서보 2개의 기동 전류를 Pi 5V 핀에서 직접 끌어오지 않는다. 서보 전원은 분리하고 GND만 공통.
- [PCA9685 데이터시트 (NXP)](https://nxp.com/docs/en/data-sheet/PCA9685.pdf)
- [SG90 데이터시트 (미러)](https://bit.nkust.edu.tw/~csshieh/teach/111A/mc/datasheet/SG90.pdf)

## 초음파 HC-SR04 계열

| 항목 | 값 |
| --- | --- |
| 전원 | 5V, 약 15mA |
| 측정 | 2~400cm, 분해능 약 0.3cm |
| 동작 | Trig에 10µs 이상 HIGH → 모듈이 40kHz 송신 → Echo가 왕복 시간만큼 HIGH |
| 거리 | 거리 = Echo 폭 × 음속(약 340m/s) / 2 |
| 주기 | 측정 간격 60ms 이상 |
| **주의** | **Echo 출력이 5V 레벨이다. Pi GPIO는 5V tolerant가 아니므로 분압이나 레벨 변환 후 연결한다.** Trig는 3.3V로 구동 가능한지 실물로 확인 |

- 커널: IIO `drivers/iio/proximity/srf04.c`, DT binding `devantech,srf04`. 필수 속성 `trig-gpios`(출력), `echo-gpios`(IRQ 가능 입력). 드라이버가 Echo의 상승·하강 IRQ 사이 시간을 재서 거리를 계산한다.
- HC-SR04 전용 compatible은 mainline에 없다(2026-07 RFC 패치 제안 단계). 타이밍이 같아 `devantech,srf04`로 동작하는지 16단계에서 확인.
- 확인 필요: 실물 모델(HC-SR04, 3.3V 동작형 HC-SR04P 등). 3.3V 동작형이면 레벨 변환 조건이 달라진다.
- [HC-SR04 사용자 매뉴얼 (ElecFreaks, 미러)](https://www-users.york.ac.uk/~mjf5/roach/Datasheets/HCSR04_Manual.pdf)
- [devantech-srf04 DT binding](https://grok.openbmc.org/xref/openbmc/linux/Documentation/devicetree/bindings/iio/proximity/devantech-srf04.yaml?r=52f171ab)

## DC 모터 + 모터 드라이버

| 항목 | 값 |
| --- | --- |
| 제어 방식 (VEDA) | 채널당 EN(PWM) + IN1/IN2(방향), 좌·우 2채널. wiringPi `pwmWrite`, PWM_MODE_MS, clock 32, range 1024 |
| VEDA 핀 (wiringPi → BCM) | L_EN 1→**GPIO18**, R_EN 24→**GPIO19**, L_IN1 28→**GPIO20**, L_IN2 27→GPIO16, R_IN1 25→GPIO26, R_IN2 23→GPIO13 |
| 커널 | `drivers/pwm/pwm-bcm2835.c`, overlay `pwm`/`pwm-2chan`. Pi 4 하드웨어 PWM0은 GPIO12 또는 18, PWM1은 GPIO13 또는 19 |
| 확인 필요 | 모터 드라이버 칩(EN/IN1/IN2 구조로 보아 L298N 계열로 추정), 모터 전원 전압 |

- 모터 전원은 Pi와 분리하고 GND만 공통으로 둔다.

### 핀 충돌: VEDA 모터 핀 ↔ I2S

VEDA Final의 모터 배선은 I2S 핀 3개를 이미 쓰고 있다.

| GPIO | VEDA 모터 | I2S |
| --- | --- | --- |
| 18 | L_EN (PWM0) | BCLK |
| 19 | R_EN (PWM1) | LRCLK |
| 20 | L_IN1 | DIN (마이크 데이터) |

마이크(14단계)와 모터(17단계)를 함께 쓰려면 **모터 쪽을 옮겨야 한다.** 후보는 하드웨어 PWM을 GPIO12/13으로 옮기고(`pwm-2chan` overlay의 핀 파라미터), IN 핀을 비어 있는 GPIO로 바꾸는 것이다. GPIO13은 VEDA에서 R_IN2였으므로 함께 옮긴다. 최종 핀 배치는 [bringup.md](bringup.md)에 "계획" 상태로 먼저 정리한 뒤 장치별 TASK에서 확정한다.

## 공통 참고

- [BCM2711 ARM Peripherals](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf) — GPIO 대체 기능(ALT) 표, PWM·PCM(I2S)·I2C 블록
- [Raspberry Pi GPIO 핀 배치](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#gpio-pinout-map)
- [config.txt](https://www.raspberrypi.com/documentation/computers/config_txt.html)
- 미러 링크는 제조사 원본을 다른 사이트가 올린 것이다. 원본 문서가 공개되어 있으면 원본으로 교체한다.
