# MPU6050: 학습용 IIO 드라이버와 mainline `inv_mpu6050` 비교

## 한 줄 요약
같은 MPU6050을 직접 작성한 IIO 드라이버와 mainline `inv_mpu6050`에 번갈아 연결해 비교함. DT의 IRQ 설정 하나가 칩 INT 설정과 buffer 생성 여부를 정하는 것을 실측으로 확인함. 목적은 남은 장치에 반복할 "mainline 드라이버 + overlay + 표준 도구 검증" 흐름을 IMU로 먼저 익히는 것.

**관련 파일**
- [T-001 TASK](../tasks/T-001-inv-mpu6050-compare.md): 계획·실측 로그·마감
- [IIO 학습 문서 2-4](../../examples/mpu6050_iio/iio-direct.md#2-4-mainline-inv_mpu6050와의-차이): 차이표, [4-7](../../examples/mpu6050_iio/iio-direct.md#4-7-mainline-inv_mpu6050로-같은-센서-읽기): 재현 절차
- [mpu6050_iio.c](../../examples/mpu6050_iio/mpu6050_iio.c): 학습용 드라이버
- mainline: `external/linux/drivers/iio/imu/inv_mpu6050/inv_mpu_core.c`, overlay `external/linux/arch/arm/boot/dts/overlays/i2c-sensor-common.dtsi` (Git 대상 아님)

## 구조

```text
 +------------------+        +-------------------------------+
 |  GY-521 (0x68)   |  I2C1  |  i2c-sensor overlay (DT)      |
 |  MPU6050         |<------>|  reg = 0x68                   |
 |              INT |---+    |  interrupts = <17 FALLING>    |
 +------------------+   |    |  interrupt-parent = &gpio     |
                        |    +---------------+---------------+
                        v                    | probe
 +------------------+        +---------------v---------------+
 | GPIO17 (bcm2835) |  IRQ   |  inv-mpu6050-i2c              |
 | Edge, falling    |------->|  irq_get_trigger_type()       |
 +------------------+        |   -> INT_PIN_CFG active-low   |
                             |  irq > 0 -> trigger + buffer  |
                             +---------------+---------------+
                                             | FIFO read + ts
                             +---------------v---------------+
                             |  IIO core                     |
                             |  trigger mpu6050-dev0 / kfifo |
                             +-------+---------------+-------+
                                     |               |
                             +-------v-----+   +-----v-------------+
                             | sysfs       |   | /dev/iio:device0  |
                             | raw / scale |   | iio_generic_buffer|
                             +-------------+   +-------------------+
```

**데이터 흐름:** GY-521 → GPIO17 IRQ → `inv-mpu6050-i2c` → 센서 FIFO 읽기 + 시각 추정 → IIO kfifo → `/dev/iio:device0` → `iio_generic_buffer`

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 장치 | GY-521 (I2C1 0x68), INT → GPIO17 | 샘플 준비 시 INT 펄스 |
| DT | `i2c-sensor` overlay (`mpu6050`, `int_pin=17`) | `reg`, `interrupts` = GPIO17 falling, `interrupt-parent` = gpio |
| 드라이버 | `inv-mpu6050-i2c` | DT IRQ 종류로 칩 INT_PIN_CFG 설정, IRQ 있으면 trigger·buffer 생성 |
| IIO 코어 | trigger `mpu6050-dev0`, kfifo | 버퍼 enable 시 수집 시작, 샘플 저장 |
| 사용자 공간 | sysfs raw·scale, `iio_generic_buffer` | 단일 값 조회, 버퍼 수집·SI 단위 출력 |

학습용 드라이버는 같은 경로에서 드라이버 단계만 다름. IRQ는 level HIGH + LATCH 고정, 센서 FIFO 없이 IRQ마다 측정 레지스터 읽기, IRQ 시각을 그대로 사용.

## 핵심 API와 선택 이유

| 지점 | 학습용 `mpu6050_iio.c` | mainline `inv_mpu6050` | 의미 |
| --- | --- | --- | --- |
| IRQ 종류 | `IRQF_TRIGGER_HIGH` 고정, 칩 LATCH | `irq_get_trigger_type()`으로 DT 값을 읽어 칩 극성·래치 설정 | 보드가 바뀌어도 DT만 수정 |
| buffer 생성 | 항상 생성(IRQ 요청 실패 시 probe 실패) | `irq > 0`일 때만 `devm_iio_triggered_buffer_setup()` | IRQ 없으면 raw만 동작 |
| 샘플 읽기 | IRQ 1번에 1샘플 | 센서 FIFO 일괄 읽기 | IRQ 지연 시 유실 감소 |
| timestamp | IRQ 시각, CLOCK_MONOTONIC | `inv_sensors_timestamp`로 샘플별 추정, IIO 기본 CLOCK_REALTIME | FIFO 사용 시 샘플별 IRQ 시각 없음 |
| 범위·주파수 | ±2 g·±250 deg/s·100 Hz 고정 | `*_scale_available`, 10~500 Hz 선택 | 학습용은 IIO 뼈대만 구현 |

대안 비교
- **VEDA Final(사용자 공간):** `/dev/i2c-*` 직접 접근 + wiringPi `waitForInterrupt()` 후 센서 FIFO 비움. 빠르게 구현 가능하지만 다른 프로그램이 표준 방식으로 같은 장치를 쓸 수 없음
- **학습용 IIO 드라이버:** 채널·trigger·buffer 구조 이해용 최소 구현
- **mainline 드라이버:** 검증된 드라이버를 DT로 연결하는 것이 기본. 남은 장치(카메라, I2S, PCA9685 등)도 같은 방식으로 진행 예정(계획)

## 실측값
- 바인딩 `inv-mpu6050-i2c`, IIO `name` = `mpu6050`, `/proc/interrupts`에 `pinctrl-bcm2835 17 Edge inv_mpu`
- 초기값: 50 Hz, accel scale `0.000598`(±2 g), gyro scale `0.001064724`(±2000 deg/s)
- scan index: accel 0~2, temp 3, gyro 4~6, timestamp 7 (형식 `be:s16`, `le:s64`)
- `iio_generic_buffer`: 6축+timestamp 5샘플, IRQ 0 → 5, 간격 20.00~20.02 ms, 수집 후 `buffer/enable` = 0
- 학습용 60초 기준선(같은 기울어진 정지 자세)과 비교: 가속도 |g|의 1% 이내 일치. ay 차이(+0.065 m/s²)는 기준선 표준편차의 약 4배라 "노이즈 범위 안 일치"로는 판정하지 않음. 샘플 5개, 주파수(50/100 Hz)도 다름

## 트러블슈팅: 실행 중 overlay 적용 시 buffer 없음
- **증상:** `dtoverlay` 실행 중 적용 → 바인딩·raw·scale 정상, `buffer/`·trigger 없음, `/proc/interrupts` 행 없음
- **확인한 계층:**
  - I2C 바인딩 정상
  - live DT: `interrupts`(GPIO17 falling)는 맞음, `interrupt-parent` = `ff ff ff ff`(미해결 phandle)
  - 배포한 dtbo에 해당 fixup 포함
  - 드라이버 소스: IRQ 없으면 trigger·buffer도 만들지 않는 분기
- **원인:** IRQ 미연결 → 드라이버가 의도대로 buffer 없이 probe
- **조치:** 같은 overlay를 `config.txt`로 부팅 시 적용 → `interrupt-parent` = gpio phandle(`0x07`), buffer·IRQ 정상 생성. 검증 후 `config.txt` 원복
- **남은 것:** dtbo에 fixup이 있는데도 실행 중 적용에서 해결되지 않은 원인은 미확인

## 예상 질문과 답

**Q1. 같은 센서인데 IRQ를 falling edge와 level high로 다르게 써도 둘 다 동작하는 이유?**
INT 극성·래치는 칩 레지스터 설정, GPIO가 어떤 변화를 IRQ로 볼지는 DT가 정함. 둘의 짝만 맞으면 됨. mainline은 DT 값을 읽어 칩을 active-low 펄스로 맞추고, 학습용은 active-high로 래치한 뒤 상태 레지스터를 읽어 해제.

**Q2. mainline은 왜 센서 FIFO를 쓰고 timestamp를 추정하나?**
IRQ 처리가 늦어도 센서가 FIFO에 샘플을 쌓아 두므로 유실이 줄고 한 번에 여러 샘플을 읽음. 대신 샘플별 IRQ 시각이 없어 IRQ 시각과 칩 클럭 주기로 각 샘플 시각을 계산. 이번 실측은 IRQ 5번에 5샘플이라 일괄 읽기 장면은 관찰 못 함.

**Q3. buffer가 안 생겼을 때 원인을 어떻게 좁혔나?**
아래 계층부터 확인. 바인딩·raw 정상으로 I2C 제외 → live DT에서 `interrupt-parent` 미해결 발견 → 드라이버 소스에서 IRQ 없으면 buffer를 안 만드는 분기 확인 → 적용 방식만 부팅 시 적용으로 바꿔 차이 확인.

**Q4. 드라이버를 직접 썼는데 왜 mainline과 비교했나?**
직접 작성은 IIO 구조 이해용. BSP 작업에서는 기존 드라이버를 DT로 연결·검증하는 경우가 많음. 같은 센서로 비교하면 생략한 기능(FIFO, 범위 선택, DT 기반 IRQ 설정)이 왜 필요한지 구체적으로 설명 가능.

**Q5. 두 드라이버 값을 어떻게 비교했나?**
raw가 아니라 각 드라이버 sysfs의 raw × scale(SI 단위)로 비교. mainline 초기 gyro 범위가 ±2000 deg/s라 raw 크기가 약 1/8로 다름. timestamp도 REALTIME과 MONOTONIC으로 시계가 달라 직접 빼지 않음.

**Q6. 같은 주소에 두 드라이버를 동시에 붙이면 안 되는 이유?**
같은 버스·주소의 I2C client는 하나만 등록됨. 다른 경로로 접근해도 두 드라이버가 같은 레지스터·INT를 다르게 설정해 IRQ 극성·데이터가 꼬임. 그래서 한쪽 overlay·모듈을 내린 것을 확인한 뒤 전환.

## 하지 않은 것·한계
- **VEDA Final에서 한 일:** 사용자 공간 I2C + wiringPi 인터럽트 대기로 센서 FIFO를 읽어 하이브리드 EIS에 사용. 커널 드라이버·DT는 다루지 않음
- **bsp-study에서 한 일:** 학습용 IIO 드라이버 작성·실측, mainline 드라이버 overlay 연결·표준 도구 검증, 두 드라이버 차이 정리
- **미검증:**
  - mainline 장기 수집, 같은 주파수에서의 정밀 비교
  - FIFO 여러 샘플 일괄 읽기와 overflow 재설정
  - 원복 뒤 IRQ 행 부재 직접 확인
  - 실행 중 적용에서 fixup이 해결되지 않은 원인
- **범위 제외:** 학습용 드라이버에 IIO 센서 FIFO 재연결, EIS 보완
