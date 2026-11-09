# MPU6050: IIO 채널·scale·IRQ 버퍼

## 목차

- [구현 범위](#구현-범위)
- [전체 실행 흐름](#전체-실행-흐름)
- [코드 해설](#코드-해설)
- [헷갈리는 개념](#헷갈리는-개념)
- [기존 코드와의 차이](#기존-코드와의-차이)
- [빌드·배포·실행·정리](#빌드배포실행정리)
- [검증 현황](#검증-현황)
- [실제 트러블슈팅](#실제-트러블슈팅)

## 구현 범위

[mpu6050_iio.c](mpu6050_iio.c)는 직접 작성한 I2C 센서 드라이버를 IIO에 등록한다. **6축 raw·고정 scale 조회와 IRQ 트리거 기반 버퍼 수집**을 구현한다. 기존 [FIFO·문자 인터페이스 예제](../mpu6050/mpu6050.md)는 보존하며 두 드라이버를 같은 센서에 동시에 연결하지 않는다.

sysfs 조회는 축별 현재 레지스터를 읽고, 버퍼 모드는 GPIO17 Data Ready마다 6축을 읽어 IIO 버퍼에 넣는다. [mpu6050_iio_app.c](mpu6050_iio_app.c)는 6축과 IRQ 관측 시각이 담긴 24바이트 형식을 확인하고 20개 샘플을 읽어 scale과 Δt를 출력한다. 센서는 공칭 100Hz·±2g·±250deg/s다. 센서 FIFO·별도 커널 큐·범위 변경·자동 복구는 추가하지 않았다. BSP 학습 마무리 범위는 IRQ 관측 시각과 기본 수집 검증이며, IIO에 센서 FIFO를 다시 연결하거나 EIS를 보완하는 작업은 현재 범위에서 제외한다.

대상은 Pi 4, 커널 `6.18.59-v8-bsp-study01`, 소스 커밋 `b3495ea401d709af0701a85babcba5e3d788a602`다. 기존 WSL 빌드 디렉터리 `out/kernel-rpi4`와 [배선](../../docs/bringup.md#모듈과-핀-연결표)을 사용한다.

## 전체 실행 흐름

1. 모듈·overlay로 I2C client를 생성하면 probe가 센서를 식별·초기화한다. 트리거·IIO 버퍼·GPIO17 IRQ를 준비하고 IIO 장치를 공개한다. 아직 센서 IRQ와 Linux IRQ는 비활성 상태다.
2. 버퍼가 꺼진 동안 `cat in_accel_x_raw`는 IIO 코어 → read_raw → 축 레지스터 읽기로 동작한다. scale 조회는 같은 콜백에서 상수를 반환한다.
3. 사용자가 6개 scan 채널을 선택하고 buffer/enable에 1을 쓰면 IIO가 수집 콜백을 트리거에 연결한다. set_trigger_state가 센서 Data Ready와 Linux IRQ를 켠다.
4. 센서 INT가 HIGH가 되면 짧은 primary IRQ 핸들러가 monotonic 시각을 기록하고 스레드를 깨운다. IRQ 스레드는 INT_STATUS를 읽어 래치를 해제하고 IIO 트리거를 호출한다.
5. IIO 수집 콜백은 측정 레지스터 14바이트를 읽고 온도 2바이트를 제외한 6축과 IRQ 타임스탬프를 IIO 버퍼에 넣는다. 성공·오류 모두 notify_done으로 처리를 마친다.
6. 앱이 /dev/iio:deviceN에서 읽은 24바이트에서 6축 값과 시각을 해석하고 scale을 곱한다. 이전 행과의 시간 차이도 ms 단위로 표시한다. 버퍼 수집 중 개별 raw 조회는 EBUSY지만 scale은 읽을 수 있다.
7. buffer/enable에 0을 쓰면 IRQ 종료를 기다린 뒤 센서 알림을 끄고 IIO가 수집 콜백을 분리한다. 앱 종료만으로 버퍼가 꺼지지는 않는다.
8. overlay 제거 시 IIO 등록 해제 → IRQ 반환 → 트리거 등록 해제 → 버퍼 정리 → 센서 슬립 순서로 정리한다.

```text
센서 Data Ready → primary IRQ에서 시각 기록 → GPIO17 IRQ 스레드
  → iio_trigger_poll_nested()
  → mpu6050_trigger_handler()
  → 14바이트 측정 레지스터 읽기 → 온도 제외 → IIO 버퍼
  → 앱 read(24바이트) → raw × scale, IRQ 시각, Δt 출력
```

## 코드 해설

### 1. 채널: 사용자에게 어떤 값을 공개할지 선언

```c
#define MPU6050_CHANNEL(_type, _axis, _reg, _index) { \
	.type = _type, \
	.modified = 1, \
	.channel2 = _axis, \
	.address = _reg, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	.scan_index = _index, \
	.scan_type = { \
		.sign = 's', \
		.realbits = 16, \
		.storagebits = 16, \
		.endianness = IIO_BE, \
	}, \
}

MPU6050_CHANNEL(IIO_ACCEL, IIO_MOD_X, MPU6050_REG_ACCEL_XOUT_H, 0)
```


`struct iio_chan_spec`은 측정값 자체가 아니라 채널 설명이다. `mpu6050_channels[]`에는 위와 같은 항목을 축별로 여섯 개 작성했다. 이번에는 scan 형식까지 여섯 번 반복하지 않도록 작은 매크로 하나로 공통 선언을 묶었다. 역슬래시는 전처리 매크로가 다음 줄까지 이어진다는 뜻이다. 함수 호출이 아니라 구조체 초기값의 반복을 줄이는 용도다.

| 필드 | 의미 |
| --- | --- |
| `type` | 가속도는 `IIO_ACCEL`, 각속도는 `IIO_ANGL_VEL` |
| `modified = 1` | X/Y/Z 같은 modifier로 채널을 구분함. 측정값을 수정한다는 뜻이 아님 |
| `channel2` | `IIO_MOD_X/Y/Z`로 축 지정 |
| `address` | 콜백에서 사용할 측정 레지스터 주소. DT의 I2C 주소 `reg=<0x68>`과 다름 |
| `info_mask_separate` | 해당 채널에 제공할 raw 속성을 비트 마스크로 선언 |
| `info_mask_shared_by_type` | 같은 종류의 X/Y/Z가 공유하는 scale 속성 선언 |
| `scan_index` | 버퍼 안의 순서. 가속도 X/Y/Z=0/1/2, 자이로 X/Y/Z=3/4/5, timestamp=6 |
| `scan_type` | 부호 있는 16비트·big-endian 저장. sysfs type은 be:s16/16>>0 |

`BIT(IIO_CHAN_INFO_RAW)`는 raw 속성을 공개한다. IIO 코어가 타입·축·속성을 조합해 `in_accel_x_raw`, `in_anglvel_z_raw` 같은 표준 이름을 만든다. 직접 sysfs 파일 생성 함수를 여섯 번 호출하지 않는다. `static const` 배열은 파일 내부의 변경하지 않는 채널 설명이다.

가속도 X/Y/Z의 상위 바이트 주소는 0x3b/0x3d/0x3f, 자이로는 0x43/0x45/0x47이다. 축마다 상·하위 두 바이트라 주소가 2씩 증가한다.

### 2. read_raw: IIO 요청을 센서 읽기로 연결

```c
struct mpu6050_data *data = iio_priv(indio_dev);
u8 raw[2];
int ret;

if (mask != IIO_CHAN_INFO_RAW)
	return -EINVAL;
if (!iio_device_claim_direct(indio_dev))
	return -EBUSY;

ret = i2c_smbus_read_i2c_block_data(data->client, chan->address,
				 sizeof(raw), raw);
if (ret < 0)
	goto out;
if (ret != sizeof(raw)) {
	ret = -EIO;
	goto out;
}

*val = (s16)get_unaligned_be16(raw);
ret = IIO_VAL_INT;

out:
	iio_device_release_direct(indio_dev);
	return ret;
```

`indio_dev`는 요청 대상 IIO 장치, `chan`은 조회할 채널, `mask`는 raw/scale 등 조회 종류다. RAW는 아래 I2C 읽기로, SCALE은 다음 절의 고정 계수 반환으로 처리한다. 이외 요청은 EINVAL이다. 아래 발췌는 SCALE 처리 이후의 RAW 경로다. 채널 선언에는 비트 마스크인 `BIT(...)`를 쓰지만 콜백의 mask는 `IIO_CHAN_INFO_RAW` 값과 비교한다.

RAW 경로의 claim_direct는 버퍼 수집과 직접 읽기가 동시에 센서를 읽지 않게 한다. 버퍼가 켜져 있으면 false를 반환하므로 EBUSY로 끝낸다. 대상 커널의 이 API는 bool이며 예전 claim_direct_mode의 int 반환 규칙과 혼동하지 않는다. 성공한 claim은 오류 경로에서도 release와 짝지어야 한다. SCALE은 상수 조회여서 이 제한 전에 처리한다.

`iio_priv()`는 IIO 장치를 할당할 때 함께 확보한 우리 상태 영역을 돌려준다. 여기에는 I2C client와 트리거 포인터를 보관한다. `chan->address`가 축별로 다르므로 여섯 축을 같은 함수로 처리한다.

I2C helper의 성공 반환은 읽은 바이트 수다. 음수 오류와 2바이트 미만의 응답을 검사한다. 센서의 big-endian 두 바이트를 `get_unaligned_be16()`으로 합치고 `s16`으로 해석해 음수를 보존한다.

**측정값은 `*val`에 쓰고, 함수는 값의 형식을 반환한다.** `IIO_VAL_INT`는 “val에 정수 하나가 있다”는 뜻이다. 측정값이나 바이트 수를 return하면 안 된다. raw 반환에서는 `val2`를 사용하지 않고, scale 반환에서는 소수 부분을 넣는다. IIO 코어가 정수의 문자열 변환을 맡으므로 드라이버에서 `sprintf()`나 `copy_to_user()`를 호출하지 않는다.

```c
static const struct iio_info mpu6050_info = {
	.read_raw = mpu6050_read_raw,
};
```

`iio_info`는 IIO에 제공할 동작 목록이다. 함수 포인터를 등록하면 IIO 코어가 필요할 때 호출한다. 기존 `file_operations.read`와 연결 방식은 비슷하지만 호출 인자와 반환 규칙이 다르다.

#### scale 요청과 소수 표현

채널마다 scale을 따로 만들지 않고 `info_mask_shared_by_type`에 SCALE을 선언했다. 그 결과 가속도 세 축은 `in_accel_scale` 하나, 자이로 세 축은 `in_anglvel_scale` 하나를 공유한다. read_raw라는 이름의 콜백이지만 mask로 raw와 scale 요청을 모두 받는다.

```c
if (mask == IIO_CHAN_INFO_SCALE) {
	*val = 0;
	switch (chan->type) {
	case IIO_ACCEL:
		*val2 = MPU6050_ACCEL_SCALE_NANO;
		break;
	case IIO_ANGL_VEL:
		*val2 = MPU6050_GYRO_SCALE_NANO;
		break;
	default:
		return -EINVAL;
	}
	return IIO_VAL_INT_PLUS_NANO;
}
```

`IIO_VAL_INT_PLUS_NANO`는 `val + val2 / 1,000,000,000` 형식이다. val=0, val2=598550이면 IIO 코어가 `0.000598550`으로 출력한다. 커널에서 부동소수점 계산을 하지 않고 정수 두 개로 소수 계수를 표현한다.

| 속성 | 현재 계수 | 물리값 계산 |
| --- | --- | --- |
| `in_accel_scale` | 0.000598550 | raw × scale → m/s² |
| `in_anglvel_scale` | 0.000133231 | raw × scale → rad/s |

기존 실습의 감도와 일치하도록 가속도는 `9.80665 / 16384`, 자이로는 `(pi / 180) / 131`을 소수점 9자리로 반올림했다. IIO 단위는 기존 앱의 g·deg/s와 다르다. 이전 표시로 비교하려면 m/s²를 9.80665로 나누거나 rad/s에 180/pi를 곱한다. 단위 근거는 대상 커널의 [IIO ABI](../../external/linux/Documentation/ABI/testing/sysfs-bus-iio)다.

scale 조회는 raw를 자동으로 변환하거나 보정하지 않는다. 사용자 쪽에서 raw에 계수를 곱해야 한다. 계수는 현재 ±2g·±250deg/s 설정에 대응하며 읽기 전용이다. 나중에 범위를 바꾸면 센서 설정과 scale을 함께 변경해야 한다. 영점 오프셋·실제 감도 보정은 구현하지 않았다.

### 3. probe: 메모리 확보 → 센서 준비 → 공개

```c
indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
if (!indio_dev)
	return -ENOMEM;

data = iio_priv(indio_dev);
data->client = client;
indio_dev->name = "mpu6050-iio-study";
indio_dev->modes = INDIO_DIRECT_MODE;
indio_dev->info = &mpu6050_info;
indio_dev->channels = mpu6050_channels;
indio_dev->num_channels = ARRAY_SIZE(mpu6050_channels);
indio_dev->available_scan_masks = mpu6050_scan_masks;
i2c_set_clientdata(client, indio_dev);
```

`devm_iio_device_alloc()`은 IIO 장치와 우리 상태 공간을 확보한다. `sizeof(*data)`는 포인터 크기가 아닌 `struct mpu6050_data` 크기다. devm은 장치 해제 시 해당 자원 정리를 관리한다. `i2c_set_clientdata()`는 remove에서 IIO 장치를 찾기 위한 연결이다.

`INDIO_DIRECT_MODE`는 요청 시 값을 직접 읽는 모드다. 채널 수는 배열 크기에서 구하며 현재 6축과 timestamp를 합쳐 일곱 항목이다. 필드를 설정한 뒤 iio_device_set_clock으로 CLOCK_MONOTONIC을 선택하고 센서를 초기화한다. 이후 트리거·버퍼·IRQ를 준비한 다음 `iio_device_register()`로 공개한다. iio_triggered_buffer_setup은 kfifo와 수집 콜백을 준비하면서 INDIO_BUFFER_TRIGGERED 모드도 추가한다. 설정 실패 시 등록하지 않고 슬립 전환을 시도한 뒤 원래 오류를 반환한다. 초기화의 레지스터 개념은 [기존 MPU6050 해설](../mpu6050/mpu6050.md#4-센서-초기화와-설정-재확인)을 참고한다.

이번에는 `iio_device_register()`와 remove의 `iio_device_unregister()`를 명시적으로 짝지었다. IIO 등록 해제로 sysfs와 활성 버퍼를 정리한 뒤 IRQ·트리거·버퍼를 역순으로 반환하고 센서를 슬립으로 전환한다. IIO 장치·트리거의 기본 할당은 devm을 사용하되 등록과 IRQ·버퍼의 해제 순서는 명시했다. immutable 트리거의 추가 참조는 IIO 장치의 최종 반환에서 정리된다. 기존 misc 예제의 파일별 상태·큐·kref를 복사하지 않는다.

### 4. IRQ와 IIO 트리거의 역할

물리 IRQ는 센서가 새 데이터 준비를 알리는 신호이고, IIO 트리거는 그 신호를 IIO 수집 콜백에 전달하는 연결이다. 둘을 같은 함수로 취급하지 않는다.

```c
status = i2c_smbus_read_byte_data(data->client, MPU6050_REG_INT_STATUS);
if (status < 0)
	dev_err_ratelimited(&data->client->dev, "[IRQ] status read failed: %d\n", status);
else if (status & MPU6050_DATA_READY)
	iio_trigger_poll_nested(data->trigger);
```

primary 핸들러는 다음과 같이 시각만 기록한다. I2C·문자열 출력·사용자 복사는 수행하지 않는다.

```c
static irqreturn_t mpu6050_irq(int irq, void *private)
{
	struct iio_dev *indio_dev = private;

	/* I2C 전송 없이 IRQ 관측 시각만 기록하고 스레드를 깨운다. */
	indio_dev->pollfunc->timestamp = iio_get_time_ns(indio_dev);
	return IRQ_WAKE_THREAD;
}
```

IRQ_WAKE_THREAD는 이 IRQ에 등록한 mpu6050_irq_thread를 실행하도록 요청한다. iio_get_time_ns는 장치에 선택된 시간 기준을 사용하며 이번 기본값은 monotonic이다. 날짜·시각을 표현하는 벽시계가 아니다.

기존 실습과 같은 HIGH·래치 방식이다. INT_STATUS 읽기로 래치를 해제하고 Data Ready일 때 트리거를 호출한다. I2C는 잠들 수 있으므로 hard IRQ에서 실행하지 않는다. request_threaded_irq의 primary는 mpu6050_irq이며 IRQF_ONESHOT을 유지한다. 스레드와 중첩 수집이 끝날 때까지 다음 IRQ 처리를 막으므로 같은 샘플의 pollfunc timestamp를 추가 잠금 없이 전달한다. thread 문맥에서 호출하는 poll_nested는 IIO 소비자의 threaded handler를 실행한다. 별도 작업 큐나 자체 대기 큐를 만들지 않았다.

트리거의 drvdata에는 indio_dev를 연결하고, IRQ의 private 인자에도 같은 장치를 넘긴다. IIO 수집 콜백의 private은 iio_poll_func이며 pf->indio_dev로 장치에 접근한다. 함수마다 전달받는 포인터 종류를 구분한다.

이번 장치의 트리거만 사용하도록 validate_own_device와 iio_trigger_set_immutable을 적용했다. current_trigger에서 이름을 확인할 수 있지만 다른 트리거로 교체하는 기능은 제공하지 않는다.

### 5. 버퍼를 켜고 끄는 시점

IIO 코어가 버퍼 수집을 시작·종료할 때 트리거 ops의 set_trigger_state를 호출한다. 수동으로 이 함수를 호출하는 것이 아니다.

- 시작: 이전 INT_STATUS 해제 → INT_ENABLE=1 → enable_irq.
- 종료: disable_irq로 진행 중인 물리 IRQ와 중첩 수집 종료 대기 → INT_ENABLE=0.

물리 IRQ는 IRQF_NO_AUTOEN으로 등록해 시작 전까지 막아 둔다. 종료 시 센서 I2C 쓰기가 실패해도 Linux IRQ는 이미 차단돼 있다. 이때 경고를 남기고 성공을 반환해 IIO의 콜백 분리·정리를 진행시킨다. 이는 센서 쓰기까지 성공했다는 뜻이 아니다. 잠금을 추가하지 않은 이유는 RAW 접근은 IIO direct claim으로, 버퍼 시작·종료는 IIO의 상태 전환으로, IRQ 수집 종료는 disable_irq/free_irq로 직렬화하기 때문이다.

### 6. 6축·타임스탬프와 IIO 버퍼

```c
u8 raw[14];
IIO_DECLARE_BUFFER_WITH_TS(__be16, scan, 6) = { };

/* 실제 함수에서는 반환값과 길이를 검사한다. */
memcpy(scan, raw, 6);
memcpy(&scan[3], &raw[8], 6);
ret = iio_push_to_buffers_with_ts(indio_dev, scan, sizeof(scan), pf->timestamp);
```

수집 콜백은 0x3b부터 14바이트를 한 전송으로 읽는다. 앞 6바이트는 가속도, 중간 2바이트는 온도, 뒤 6바이트는 자이로다. 온도를 건너뛴 6축 12바이트에 정렬과 timestamp를 포함해 한 scan을 보낸다. __be16은 big-endian 데이터를 보관한다는 표기다. 버퍼에서는 센서 바이트 순서를 유지하고 앱이 해석한다. sysfs raw 경로의 get_unaligned_be16 정수 변환과 구분한다.

| 바이트 | scan_index | 값 |
| --- | --- | --- |
| 0~1 / 2~3 / 4~5 | 0 / 1 / 2 | 가속도 X / Y / Z |
| 6~7 / 8~9 / 10~11 | 3 / 4 / 5 | 자이로 X / Y / Z |
| 12~15 | 없음 | timestamp를 8바이트 경계에 배치하는 4바이트 패딩 |
| 16~23 | 6 | IRQ 관측 시각, signed 64비트 ns. Pi에서는 little-endian |

available_scan_masks의 GENMASK(5,0)는 하드웨어 수집을 항상 6축으로 정한다. 이는 모든 사용자에게 6축 선택을 강제한다는 뜻은 아니다. 일부 채널을 선택하면 IIO가 필요한 채널만 추출할 수 있다. timestamp는 IIO_CHAN_SOFT_TIMESTAMP(6)으로 선언한다. IIO가 timestamp 선택을 별도 플래그로 관리하므로 하드웨어 마스크는 기존 6축 비트만 유지한다. 이번 앱은 6축과 timestamp가 모두 선택된 24바이트 형식만 지원한다.

콜백은 정상·I2C 실패·짧은 전송·버퍼 저장 실패 모두 마지막에 iio_trigger_notify_done을 호출한다. 완료를 알리지 않으면 트리거 사용 상태가 남아 후속 수집이 막힐 수 있다. 오류는 제한된 빈도로 커널 로그에 남기며 실패한 샘플을 성공값으로 보내지 않는다. 자동 리셋·재시도·유실 순번은 추가하지 않았다.

IIO_DECLARE_BUFFER_WITH_TS는 6개의 16비트 값 뒤에 8바이트로 정렬된 시각 공간까지 확보한다. `{ }`로 초기화해 패딩도 0으로 채운다. iio_push_to_buffers_with_ts는 공간 크기를 확인하고 timestamp가 선택됐다면 마지막 8바이트에 pf->timestamp를 넣는다. 단순히 12+8=20바이트로 계산하면 안 된다. IIO_CHAN_SOFT_TIMESTAMP의 저장 형식은 CPU native endian이므로 이 예제의 little-endian Pi에서는 type이 le:s64/64>>0이다. 6축의 big-endian과 다르다.

**이 시각은 센서가 실제 측정한 시각이 아니라 CPU가 IRQ 핸들러를 실행한 시각**이다. IRQ 전달 지연·래치 병합·그 뒤의 I2C 읽기 지연을 없애지는 않는다. 한편 앱이 늦게 read해도 이미 저장된 IRQ 시각을 받아볼 수 있다. 센서 FIFO가 없으므로 긴 Δt를 유실 개수로 곧바로 환산하지 않는다.

센서 FIFO가 없으므로 IRQ 처리 지연 중 과거 값은 덮어써질 수 있다. IIO 버퍼는 이미 읽은 데이터를 사용자에게 전달하는 지연을 흡수할 뿐이다. 20개 수신만으로 정확한 100Hz나 무손실을 검증한 것으로 보지 않는다.

### 7. 사용자 앱과 Makefile

앱 인자는 /sys/bus/iio/devices/iio:deviceN 경로다. name이 우리 장치인지, 6축의 en=1·index=0~5·type=be:s16/16>>0, timestamp의 en=1·index=6·type=le:s64/64>>0, clock=monotonic과 버퍼 활성화를 확인한다. 형식이 맞지 않으면 해석하지 않고 종료한다. scale은 sysfs에서 읽어 사용하므로 감도 상수를 다시 넣지 않는다.

read_sample은 poll로 최대 5초 동안 데이터를 기다리고 한 scan인 24바이트를 읽는다. 제거·timeout·짧은 읽기는 오류로 처리한다. O_NONBLOCK으로 열어 준비 상태가 바뀌어도 read에서 무한 대기하지 않는다. 수신한 big-endian 두 바이트를 int16_t로 해석하고 각 센서 scale을 곱해 m/s²·rad/s를 출력한다. timestamp는 오프셋16부터 memcpy로 정렬된 int64_t 변수에 복사한다. timestamp의 ns 값과 `(현재-이전)/1000000.0`인 Δt(ms)를 출력한다. 첫 행은 이전 샘플이 없어 Δt에 -를 표시한다. 0 이하이거나 증가하지 않는 시각은 오류로 처리한다. 20개 출력 후 close하지만 buffer/enable은 별도로 0을 써야 한다.

Makefile의 all은 modules·overlay·app, deploy는 ko·dtbo·앱의 전송이다. 앱은 APP_CC로 ARM64 크로스 빌드한다. IIO 버퍼 형식을 공개 sysfs에서 확인하므로 별도 공유 헤더는 만들지 않았다.

## 헷갈리는 개념

- **IIO는 센서 제어 코드를 대신 작성하지 않는다.** 레지스터 설정·전송·부호 해석은 우리가 구현하고 채널 속성·조회 호출·문자열 출력을 IIO에 맡긴다.
- sysfs raw는 요청 시 축별 조회, /dev/iio:deviceX는 버퍼 읽기다. 버퍼에는 새 mpu6050_iio_app을 사용하며 기존 misc용 mpu6050_app과 호환되지 않는다.
- 여섯 파일을 차례로 읽으면 각각 다른 시점에 전송한다. 여섯 값이 같은 측정 시점의 한 샘플이라는 보장은 없다. sysfs 반복 조회를 같은 시점의 6축 수집으로 취급하지 않는다.
- raw는 부호 있는 정수이며 범위는 -32768~32767이다. 기존 범위에서는 가속도 raw/16384가 g, 자이로 raw/131이 deg/s다. 현재는 SI 단위 환산 계수를 scale로 제공하며 실제 곱셈은 사용자 공간에서 수행한다.
- GPIO17 INT는 기존과 같은 물리11 배선이다. 새 overlay는 핀 입력·풀다운과 HIGH 레벨 IRQ를 선언한다. raw/scale만 조회할 때는 IRQ를 켜지 않고, 버퍼 활성화 때만 사용한다.

## 기존 코드와의 차이

기존 예제는 IRQ → 센서 FIFO → 커널 큐 → misc read였다. 이번 예제는 sysfs 직접 읽기에 더해 IRQ → IIO 트리거 → 측정 레지스터 읽기 → IIO 버퍼를 구현한다. 센서 제어 원리를 유지하면서 IIO의 사용자 인터페이스를 먼저 배우려는 단계 구분이다.

대상 커널의 [inv_mpu_core.c](../../external/linux/drivers/iio/imu/inv_mpu6050/inv_mpu_core.c)에서 `inv_mpu6050_channels`, `inv_mpu6050_read_raw`, `inv_mpu6050_read_channel_data`와 IIO 등록 부분을 확인했다. 기존 코드는 regmap·runtime PM·버퍼와의 직접 접근 조정·scale 등을 지원한다. 우리는 고정 범위의 RAW·SCALE과 센서 FIFO 없는 6축 버퍼 수집만 지원한다. 기존 inv_mpu_trigger.c의 threaded IRQ·poll_nested·notify_done 연결과 산업용 트리거/버퍼 helper를 대상 소스에서 확인했다. 기존 코어는 가속도 scale을 micro 정밀도, 자이로를 범위 기반 nano 계수로 반환한다. 이번 예제는 앞선 raw/16384·raw/131 환산 기준을 유지하고 두 계수 모두 nano 형식으로 표현하므로 기존 코어의 상수와 약간 다르다. VEDA의 IIO 구현과 직접 대조한 기록은 없어 특정 수업 코드와 같다고 단정하지 않는다.

트리거·버퍼 근거: [Triggered buffers](https://docs.kernel.org/driver-api/iio/triggered-buffers.html), [Triggers](https://docs.kernel.org/driver-api/iio/triggers.html).

추가 개념의 공식 근거: [Linux IIO Core elements](https://docs.kernel.org/driver-api/iio/core.html). 실제 API는 대상 커널 소스에 맞춘다.

## 빌드·배포·실행·정리

### 1. WSL · 새 예제 디렉터리 · 빌드

선행 조건: [커널 구축](../../docs/kernel-build.md)의 소스·빌드 결과와 ARM64 크로스 GCC가 준비돼 있어야 한다. 현재 `out/kernel-rpi4/.config`에서 `CONFIG_IIO=m`을 확인했다. 커널 재빌드나 설정 변경은 하지 않는다.

```bash
cd ~/bspworkspace/bsp-study/examples/mpu6050_iio
make
file mpu6050_iio.ko
modinfo -F vermagic ./mpu6050_iio.ko
modinfo -F depends ./mpu6050_iio.ko
file mpu6050_iio_app
```

[Makefile](Makefile)은 기존 Kbuild 분기·KDIR·크로스 컴파일·호스트 dtc 방식이다. all은 modules·overlay·app을 만든다. 기대값은 ARM aarch64 모듈, `6.18.59-v8-bsp-study01`로 시작하는 vermagic, IIO 코어 industrialio와 triggered-buffer 도우미 의존성이다. 새 앱도 ARM aarch64 실행 파일이어야 한다.

### 2. WSL · 같은 디렉터리 · 전송

선행 조건: 빌드 성공, SSH 별칭 pi와 Pi의 `~/bsp/modules`, `~/bsp/overlays`, `~/bsp/apps`가 준비돼 있어야 한다.

```bash
cd ~/bspworkspace/bsp-study/examples/mpu6050_iio
make deploy
```

deploy는 만든 ko·dtbo·앱을 각각 ~/bsp/modules·overlays·apps로 전송만 한다. 로드·overlay 적용은 별도다.

### 3. Pi · 홈 디렉터리 · 버전과 기존 점유 확인

선행 조건: MPU6050 전원·SDA/SCL 배선 유지, 기존 앱 종료. 이전 FIFO 예제는 이미 정리 완료한 사용자 결과가 있다. 다시 로드했다면 기존 overlay를 제거한 뒤 기존 모듈을 해제하고 진행한다.

```bash
cd ~
uname -r
sudo dtoverlay -l
ls -ld /sys/bus/i2c/devices/1-0068
```

기대 커널은 `6.18.59-v8-bsp-study01`이다. 이미 `1-0068`이 있으면 기존 점유를 확인하고 정리한다. 같은 주소에 새 client를 중복 생성하지 않는다. 바인딩 중인 센서에 i2ctransfer로 직접 접근하지 않는다.

### 4. Pi · 모듈과 overlay 디렉터리 · 등록

선행 조건: 이전 센서 overlay 해제, GPIO17 INT 배선 유지, 새 모듈·overlay·앱 전송 완료. IIO 도우미는 앞서 설치한 대상 커널의 `/lib/modules/$(uname -r)`에서 로드한다.

```bash
cd ~/bsp/modules
sudo modprobe industrialio_triggered_buffer
sudo insmod ./mpu6050_iio.ko

cd ~/bsp/overlays
sudo dtoverlay -d "$PWD" mpu6050-iio-overlay
readlink -e /sys/bus/i2c/devices/1-0068/driver
sudo dmesg | grep -F 'mpu6050-iio-study'
```

예상 driver는 `/sys/bus/i2c/drivers/mpu6050-iio-study`이며 성공 시 `[IIO] ready: raw + scale + IRQ timestamp buffer, scan=24 bytes` 로그가 나온다. modprobe 실패 시 의존 모듈 설치 상태부터 확인하며 insmod를 계속하지 않는다.

driver 링크만으로 probe 완료를 판단하지 않는다. 센서 초기화에는 100ms 대기가 두 번 있어 링크가 보이면서 새 ready 로그는 아직 없을 수 있다. 잠시 뒤 로그를 다시 확인하고 다음 단계의 IIO 이름·timestamp 속성까지 확인한다. 이전 실행의 ready·REMOVE 로그를 이번 등록 결과와 혼동하지 않는다.

### 5. Pi · 홈 디렉터리 · IIO 장치 찾기와 raw 조회

선행 조건: 새 드라이버 바인딩·IIO 등록 성공. 번호를 0으로 고정하지 않고 name으로 찾는다. 새 터미널에서도 이 블록을 실행해 변수를 설정한다.

```bash
cd ~
imu_iio_path=
for device in /sys/bus/iio/devices/iio:device*; do
    [ -r "$device/name" ] || continue
    if [ "$(cat "$device/name")" = "mpu6050-iio-study" ]; then
        imu_iio_path="$device"
        break
    fi
done
printf 'IIO path: %s\n' "$imu_iio_path"
if [ -n "$imu_iio_path" ]; then
    cat "$imu_iio_path/name"
    for axis in x y z; do
        printf 'accel %s: ' "$axis"
        cat "$imu_iio_path/in_accel_${axis}_raw"
        printf 'gyro %s: ' "$axis"
        cat "$imu_iio_path/in_anglvel_${axis}_raw"
    done
    printf 'accel scale (m/s^2 per LSB): '
    cat "$imu_iio_path/in_accel_scale"
    printf 'gyro scale (rad/s per LSB): '
    cat "$imu_iio_path/in_anglvel_scale"
fi
```

scale의 예상 출력은 각각 `0.000598550`, `0.000133231`이다. 이 scale 값은 앞선 direct 버전에서 실측 확인했으며 현재 IRQ 버퍼 추가 버전은 별도 검증한다.

경로가 비어 있으면 등록 상태를 확인한다. 여섯 속성이 정수를 반환하는지 확인하고 센서의 자세를 바꿔 다시 읽는다. 고정된 예상 숫자는 정하지 않는다.

### 6. Pi · 같은 홈 셸 · 6축·timestamp·버퍼 설정

선행 조건: 직전 단계에서 imu_iio_path가 올바르게 설정되고 버퍼가 꺼져 있어 raw/scale 조회가 가능해야 한다. 새 터미널이면 5단계의 name 검색부터 다시 실행한다. 아래는 root 권한으로 설정 파일에 쓰기 위해 tee를 사용한다.

```bash
cd ~
if [ -n "$imu_iio_path" ]; then
    printf '0\n' | sudo tee "$imu_iio_path/buffer/enable"
    for channel in in_accel_x in_accel_y in_accel_z in_anglvel_x in_anglvel_y in_anglvel_z in_timestamp; do
        printf '1\n' | sudo tee "$imu_iio_path/scan_elements/${channel}_en"
    done
    printf '128\n' | sudo tee "$imu_iio_path/buffer/length"
    printf '1\n' | sudo tee "$imu_iio_path/buffer/watermark"
    cat "$imu_iio_path/trigger/current_trigger"
    cat "$imu_iio_path/current_timestamp_clock"
    cat "$imu_iio_path/scan_elements/in_timestamp_type"
fi
```

length=128은 커널 IIO 버퍼 용량이고 센서 FIFO 크기가 아니다. watermark=1은 한 scan부터 읽기 준비 상태로 알리는 설정이다. current_trigger는 이 장치에 고정된 트리거 이름을 보여준다. 여기서는 버퍼를 꺼 둔다. 앱 없이 수집을 켜 놓으면 버퍼가 차므로 다음 단계에서 앱 실행 직전에 활성화한다. 수집 중 raw 조회는 EBUSY이고 scale은 조회할 수 있도록 구현했다. 이 접근 제한의 실제 확인 여부는 검증 현황에 구분한다.

### 7. Pi · 앱 디렉터리 · 시각·간격과 함께 20개 읽기

선행 조건: 같은 셸의 imu_iio_path와 직전 단계의 6축·timestamp·length·watermark 설정, 새 모듈과 앱을 함께 갱신한 상태. 새 터미널이면 5단계에서 변수를 다시 설정한다. 버퍼를 재시작하고 바로 앱을 실행하며, 성공·오류 종료 뒤 모두 버퍼를 끈다.

```bash
cd ~/bsp/apps
if [ -n "$imu_iio_path" ]; then
    printf '0\n' | sudo tee "$imu_iio_path/buffer/enable"
    printf '1\n' | sudo tee "$imu_iio_path/buffer/enable"
    sudo ./mpu6050_iio_app "$imu_iio_path"
    echo $?
    printf '0\n' | sudo tee "$imu_iio_path/buffer/enable"
    sudo dmesg | grep -F 'mpu6050-iio-study' | tail -5
fi
```

OPEN → READ 표 20행(IRQ time·dt 포함) → CLOSE·종료0이 정상 경로의 기대 결과다. 시각 증가와 공칭100Hz에 대응하는 약10ms 간격을 관찰하되 엄격한 허용 범위나 무손실 합격 기준으로 쓰지 않는다. I2C나 버퍼 저장 실패가 없는지도 커널 로그에서 함께 확인한다. 출력은 m/s²·rad/s이며 기존 misc 앱의 g·deg/s와 단위가 다르다. timestamp는 IRQ 관측 시각이고 샘플 순번은 없으므로 간격 변화만으로 센서 유실이나 정확한 측정 시각을 판정하지 않는다.

### 8. Pi · 홈 디렉터리 · 중지 상태와 raw 조회 확인

선행 조건: 직전 단계에서 앱 종료 후 버퍼를 중지했고 같은 셸에서 imu_iio_path를 유지한다. 새 터미널이면 5단계 검색을 다시 실행한다.

```bash
cd ~
if [ -n "$imu_iio_path" ]; then
    cat "$imu_iio_path/buffer/enable"
    cat "$imu_iio_path/in_accel_x_raw"
    sudo dmesg | grep -F 'mpu6050-iio-study' | tail -3
fi
```

buffer/enable=0, BUFFER disabled와 raw 조회 재개가 기대 결과다. 앱이 중단돼 버퍼 중지 명령까지 실행하지 못했다면 `printf '0\n' | sudo tee "$imu_iio_path/buffer/enable"`로 중지한다. 센서 알림 비활성화 실패가 기록됐다면 Linux IRQ 차단은 수행됐더라도 센서 쓰기 성공으로 기록하지 않는다.

### 9. Pi · overlay → 모듈 디렉터리 · 정리

선행 조건: 앱 종료·버퍼 중지·같은 셸에 imu_iio_path 유지. 새 터미널이면 장치 제거 전에 5단계 검색으로 변수를 다시 설정한다. 정상 경로는 이 순서를 사용하고 수집 중 강제 제거는 별도 검증한다.

```bash
cd ~/bsp/overlays
sudo dtoverlay -r mpu6050-iio-overlay
ls -ld /sys/bus/i2c/devices/1-0068
if [ -n "$imu_iio_path" ]; then
    ls -ld "$imu_iio_path"
fi
grep -F 'mpu6050-iio-data-ready' /proc/interrupts
sudo dmesg | grep -F 'mpu6050-iio-study' | tail -3

cd ~/bsp/modules
sudo rmmod mpu6050_iio
lsmod | grep '^mpu6050_iio '
```

기대 결과는 client·이번 IIO 장치의 sysfs 항목·물리 IRQ 행 제거와 REMOVE 로그, 모듈 부재다. IIO 공통 도우미는 다른 장치가 사용할 수도 있으므로 따로 내리지 않는다. 트리거 참조가 있으므로 overlay로 장치를 정리한 뒤 rmmod한다.

## 검증 현황

2026-11-08까지 사용자가 제공한 출력 기준이다. 아래 성공 기록은 사용자 실행 결과이며 에이전트가 빌드하거나 보드를 재실행한 결과가 아니다.

| 버전·항목 | 확인한 결과 |
| --- | --- |
| 최초 raw direct 버전 | depends=industrialio, driver 링크·ready(1376.955190), iio:device0 이름과 6축 raw 조회 확인. accel=(-3436,-242,-15082), gyro=(-293,-189,110). 장치 두 경로 부재·REMOVE(7716.219831) 확인 |
| scale direct 버전 | ready(10072.052929), scale=0.000598550/0.000133231 확인. 장치 두 경로 부재·REMOVE(13421.550529) 확인 |
| timestamp 추가 전 IRQ 버퍼 빌드 | depends=industrialio,industrialio-triggered-buffer와 앱의 ELF ARM aarch64·동적 링크 형식을 사용자 출력으로 확인 |
| timestamp 추가 전 등록·활성화 | 새 ready(15476.277240), GPIO17 Level·Linux IRQ61 등록과 활성화 전 횟수0 확인. 6채널·length128·watermark1 설정, 트리거 mpu6050-iio-study-1-0068, BUFFER enabled(15737.565469)와 IRQ 횟수5 확인 |
| timestamp 추가 전 앱 수신·버퍼 중지 | 버퍼 재시작 후 6축 표20행·CLOSE·종료0 확인. 출력 단위는 m/s²·rad/s. enabled(15926.582049) → disabled(15926.808754) 구간의 제공 로그에 새 drop 없음 |
| timestamp 추가 전 장치 제거 | I2C 1-0068·IIO iio:device0 경로 부재, REMOVE(16131.186128) 확인 |
| 현재 timestamp 버전 등록 | 새 ready(17976.379682)의 scan=24 bytes, current_timestamp_clock=monotonic, in_timestamp_en 속성 확인 |
| 현재 timestamp 버전 수신 | 6축+timestamp 설정 후20개 수신·시각 증가·CLOSE·종료0 확인. 19개 간격은 최소10.013ms·최대10.019ms·평균10.015ms이며 이 구간의 관측률은 약99.85Hz |
| 현재 timestamp 버전 중지·제거 | buffer/enable=0, 중지 후 in_accel_x_raw=-3426 조회 확인. I2C/IIO 장치 경로 부재·REMOVE(19097.883415), 안내한 정리 순서의 IRQ·최종 lsmod 조회 출력 없음 확인 |

현재 24바이트 버전 앱의 첫 행과 마지막 행을 발췌했다. 중간 18행도 사용자 출력에서 수신 확인했으며 아래에는 생략했다. 위 12바이트 버전의 과거 결과와 구분한다.

```text
=== READ: 20 IIO scans ===
 No. |        IRQ time (ns) |  dt (ms) |       Acceleration (m/s^2) |   Angular velocity (rad/s)
     |                      |          |        X        Y        Z |        X        Y        Z
   1 |       18258644188569 |        - |   -2.061   -0.168   -9.084 |   -0.042   -0.013   +0.007
  20 |       18258834481392 |   10.017 |   -2.028   -0.208   -9.099 |   -0.046   -0.013   +0.010
=== CLOSE ===
0
```

간격 통계는 제공된20개 시각의19개 차이에서 계산했다. 첫 시각부터 마지막 시각까지 약190.293ms인 짧은 관찰이며 장기 안정성 시험은 아니다. 앱이 출력한 시각은 센서의 정확한 측정 시각이 아닌 CPU의 IRQ 관측 시각이다. 이번 수집 구간의 제공 로그는 enabled(18258.685048) → disabled(18258.910338)이며 새 drop 메시지는 없었다.

미확인 항목은 다음과 같다. 현재 단계의 정상 수신 성공과 구분한다.

- 버퍼 수집 중 raw 조회의 EBUSY는 코드로 구현했지만 사용자 실행 출력은 아직 없다. 중지 후 raw 재개는 확인했다.
- I2C 실패·짧은 읽기·수집 중 제거·앱 timeout 등 오류/경쟁 경로는 미검증이다.
- 정지 상태 60초 기준선과 장기 주기 안정성은 아직 확인하지 않았다. IRQ 시각만으로 전체 센서 샘플 무손실·측정 정확도를 판정하지 않는다. IIO 센서 FIFO 연결·EIS 보완은 현재 범위에서 제외한다.

## 실제 트러블슈팅

### 앱 없이 버퍼를 켜 두어 sample dropped: -16 발생

**증상:** 버퍼를 활성화한 뒤 앱 실행을 기다리던 구간에서 다음 로그가 나왔다.

```text
[15924.224341] mpu6050-iio-study 1-0068: [BUFFER] sample dropped: -16
[15924.234381] mpu6050-iio-study 1-0068: [BUFFER] sample dropped: -16
```

**원인:** IRQ는 앱이 실행되기 전부터 수집하지만 앱이 읽지 않으면 커널 IIO 버퍼가 찬다. 이번 length=128은 공칭100Hz에서 약1.28초 분량이며 실제 수집률 측정값은 아니다. 이 로그는 센서 FIFO 넘침이나 I2C 읽기 실패가 아니라 `iio_push_to_buffers()`의 저장 실패다. 대상 커널 [iio_store_to_kfifo()](../../external/linux/drivers/iio/buffer/kfifo_buf.c)는 kfifo_in으로 한 샘플을 넣지 못하면 -EBUSY를 반환하고, [IIO 버퍼 코어](../../external/linux/drivers/iio/industrialio-buffer.c)가 이를 드라이버에 전달한다. -EBUSY의 값이 -16이다. 가득 찬 동안 새 샘플을 보관하지 못하며 기존 샘플을 자동으로 최신 값으로 교체하지 않는다. 로그가 제한된 빈도로 출력되므로 두 줄을 유실 두 개로 해석하지 않는다.

**이번 처리:** 버퍼를 껐다 켠 직후 앱으로20개를 읽고 바로 중지했다. 드라이버 수정·커널 재빌드·재부팅은 하지 않았다.

```text
[15926.543837] mpu6050-iio-study 1-0068: [BUFFER] disabled
[15926.582049] mpu6050-iio-study 1-0068: [BUFFER] enabled: 100 Hz nominal
[15926.808754] mpu6050-iio-study 1-0068: [BUFFER] disabled
[16131.186128] mpu6050-iio-study 1-0068: [REMOVE] detached
```

**결과와 재현 절차 보완:** 앱20행·종료0·버퍼 중지·장치 제거를 확인했다. drop은 재시작 이전인15924초대이며 제공된 새 수집 구간에는 없었다. 이것만으로 모든 하드웨어 샘플의 무손실을 증명하지 않는다. 앞으로는 6단계에서 설정만 하고 7단계에서 버퍼 시작 → 앱 읽기 → 버퍼 중지를 붙여 실행한다. 앱 close만으로 수집이 중지되지 않는 점을 유지한다.
