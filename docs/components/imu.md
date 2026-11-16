# IMU: MPU6050 I2C 6축 센서 (직접 작성 IIO 드라이버)

## 1. 요약
- **역할:** 100Hz 6축(가속도·자이로) 값과 IRQ 시각을 IIO 버퍼로 사용자 공간에 준다.
- **상태:** 두 카드에서 `devcheck imu` PASS(20샘플, 평균 간격 10.016ms). 회전 방향·6방향 보정·센서 FIFO 일괄 읽기는 미검증([09 6장](../learning/09-iio-direct.md#6-트러블슈팅과-한계)).
- **담당 파일:**
  - IIO 드라이버(현재 기준): [mpu6050_iio.c](../../examples/mpu6050_iio/mpu6050_iio.c), [mpu6050-iio-overlay.dts](../../examples/mpu6050_iio/mpu6050-iio-overlay.dts), [mpu6050_compat.h](../../examples/mpu6050_iio/mpu6050_compat.h), 수집 앱 [mpu6050_iio_app.c](../../examples/mpu6050_iio/mpu6050_iio_app.c)
  - Yocto: [mpu6050-iio.bb](../../yocto/meta-bsp-study/recipes-kernel/mpu6050-iio/mpu6050-iio.bb), [bsp-study-devices.inc](../../yocto/meta-bsp-study/conf/bsp-study-devices.inc)
  - misc FIFO 드라이버(06 학습용, 64GB 카드만): [mpu6050_drv.c](../../examples/mpu6050/mpu6050_drv.c), [mpu6050-overlay.dts](../../examples/mpu6050/mpu6050-overlay.dts)
- **학습 문서:** [06 misc·FIFO·IRQ](../learning/06-mpu6050.md), [09 IIO·mainline 비교](../learning/09-iio-direct.md)

## 2. 구성
흐름: `MPU6050 INT → GPIO17 IRQ(시각) → IRQ thread → 14B I2C 읽기 → IIO kfifo → /dev/iio:deviceN → 앱`

| 항목 | 64GB 카드 (6.18.59 Debian) | Yocto 카드 (6.6.63) |
| --- | --- | --- |
| HW 연결 | I2C1 `0x68`(GPIO2/3), INT → GPIO17(물리 11) ([bringup](../bringup.md#gy-521-배선)) | 같음 |
| DT/overlay | 실행 중 `dtoverlay -d ~/bsp/overlays mpu6050-iio-overlay`(09), 또는 `overlays-bsp-study01/mpu6050-iio.dtbo`로 복사 후 `config.txt` 한 줄(18 비교 때, 이후 원복) | `config.txt`의 `dtoverlay=mpu6050-iio`, 부팅 때 적용 |
| 드라이버 | `mpu6050_iio.ko`를 `insmod` | 같은 소스를 레시피로 빌드(`/lib/modules/6.6.63-v8/updates/`), compatible로 자동 적재 |
| 6.6 차이 | — | [mpu6050_compat.h](../../examples/mpu6050_iio/mpu6050_compat.h)가 6.12·6.15·6.16 API를 대신함 |
| 커널 config | `CONFIG_IIO=m`(기본 config) | 기본 config 그대로 |
| 사용자 인터페이스 | `/sys/bus/iio/devices/iio:deviceN`(name `mpu6050-iio-study`), `/dev/iio:deviceN` | 같음 |

- compatible은 `nakta,mpu6050-iio-study`([L392](../../examples/mpu6050_iio/mpu6050_iio.c#L392))이다. 업스트림 `inv_mpu6050`(`invensense,mpu6050`)은 Yocto 이미지에 있지만 0x68에 붙지 않는다.
- 같은 0x68에는 드라이버 하나만 붙인다. misc(`mpu6050-study`), IIO(`mpu6050-iio-study`), mainline(`inv-mpu6050-i2c`) 중 하나를 내린 뒤 다른 것을 올린다.
- 고정 설정: 100Hz(`SMPLRT_DIV=9`), ±2g, ±250°/s([init L233](../../examples/mpu6050_iio/mpu6050_iio.c#L233)).

## 3. 빌드·배포
- **64GB 카드:** WSL에서 6.18 트리(`out/kernel-rpi4`)로 빌드한 뒤 scp. 의존 모듈을 먼저 올린다.

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mpu6050_iio — out/kernel-rpi4 빌드 완료 상태
make && make deploy                         # .ko·.dtbo·앱을 pi:~/bsp/ 아래로
# [Pi] 64GB 카드
sudo modprobe industrialio_triggered_buffer
sudo insmod ~/bsp/modules/mpu6050_iio.ko
sudo dtoverlay -d ~/bsp/overlays mpu6050-iio-overlay
```

- **Yocto 카드:** `mpu6050-iio.bb`가 `examples/mpu6050_iio`의 `.c`·compat 헤더를 가져가 빌드하고, 커널 bbappend가 overlay `.dts`를 커널 트리에 복사해 `mpu6050-iio.dtbo`를 만든다. 부팅하면 udev가 modalias로 모듈과 의존 모듈을 올린다.

```bash
# [WSL, Yocto] ~/yocto/build — 드라이버만 다시 빌드해 확인할 때
bitbake mpu6050-iio && bitbake bsp-study-image
```

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| 바인딩 | `readlink /sys/bus/i2c/devices/1-0068/driver` | `.../mpu6050-iio-study` |
| 버스(64GB) | `i2cdetect -y 1` | `68`에 `UU`. Yocto 이미지에는 i2c-tools가 없다 |
| IRQ | `grep mpu6050-iio-data-ready /proc/interrupts` | GPIO17 Level 행. 버퍼가 꺼져 있으면 횟수가 늘지 않음 |
| IIO 장치 | `grep -lx mpu6050-iio-study /sys/bus/iio/devices/iio:device*/name` | 경로 하나(번호는 고정하지 않음) |
| raw·scale | `cat $dev/in_accel_x_raw $dev/in_accel_scale` | 정수, `0.000598550` |
| 버퍼 | `devcheck imu` | `samples=20 dt_avg_ms=10.0xx`, PASS |

`dev=$(grep -lx mpu6050-iio-study /sys/bus/iio/devices/iio:device*/name | xargs dirname)`로 경로를 잡는다. 64GB 카드는 일반 사용자라 쓰기에 `sudo`가 필요하고 `sudo ~/bsp/apps/devcheck imu`로 실행한다. Yocto 카드는 root라 `sudo` 없이 실행한다.

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| 드라이버 로직 수정 | `examples/mpu6050_iio/mpu6050_iio.c` 한 곳(Yocto 레시피가 같은 파일을 가져감) | 6.18에만 있는 API를 새로 쓰면 `mpu6050_compat.h`에 6.6 대체를 추가 | WSL `make W=1`과 `bitbake mpu6050-iio` 둘 다 경고 0, 두 카드 `devcheck imu` |
| 샘플링 주기·범위 변경 | [init L233](../../examples/mpu6050_iio/mpu6050_iio.c#L233)의 `SMPLRT_DIV`·`GYRO_CONFIG`·`ACCEL_CONFIG` | scale 상수([L40](../../examples/mpu6050_iio/mpu6050_iio.c#L40)), 앱은 sysfs scale을 읽으므로 수정 불필요 | `in_*_scale` 값, `devcheck imu`의 `dt_avg_ms` |
| INT 핀 변경 | overlay의 `pins = "gpio17"`와 `interrupts = <17 4>` | [bringup](../bringup.md#gy-521-배선) 배선 표, 다른 장치 핀 충돌([40핀](../bringup.md#40핀-한눈에-보기)) | `/proc/interrupts`의 GPIO 번호 |
| compatible 변경 | 드라이버 of_match([L392](../../examples/mpu6050_iio/mpu6050_iio.c#L392))와 overlay `compatible` | Yocto 자동 적재는 modalias로 하므로 둘이 같아야 함 | `readlink .../1-0068/driver` |
| mainline `inv_mpu6050`로 비교(64GB) | `config.txt`에 `dtoverlay=i2c-sensor,mpu6050,int_pin=17` | 학습용 overlay·모듈을 먼저 내림, 실행 중 `dtoverlay` 대신 부팅 때 적용 | driver `inv-mpu6050-i2c`, `buffer/` 생성 ([09 5-7](../learning/09-iio-direct.md#5-7-mainline-inv_mpu6050로-같은-센서-읽기)) |

Yocto 카드에서 mainline으로 바꾸는 것은 해 보지 않았다.

## 6. 트러블슈팅
먼저 볼 계층: 4 모듈 적재 → 5 매칭·probe → 6 IRQ → 7 IIO 버퍼.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 4 모듈 적재 | 64GB 카드 `insmod mpu6050_iio.ko`가 `Unknown symbol in module` | `modinfo -F depends ./mpu6050_iio.ko` | `insmod`는 의존 모듈(`industrialio`, `industrialio-triggered-buffer`)을 올리지 않음 | `sudo modprobe industrialio_triggered_buffer` 후 `insmod`. Yocto는 `depmod`된 자동 적재라 해당 없음 |
| 7 IIO 버퍼 | dmesg `[BUFFER] sample dropped: -16` 반복 | `cat $dev/buffer/enable`, 읽는 앱이 있는지 | 읽는 쪽 없이 버퍼를 켜 kfifo(128)가 가득 참 → `-EBUSY` | [조치 절차: 버퍼 가득 참](#조치-절차-버퍼-가득-참) |
| 3 DT·overlay | mainline 비교 때 바인딩·raw는 되는데 `buffer/`·trigger가 없고 `/proc/interrupts`에 행이 없음 | `od -An -tx1 /sys/bus/i2c/devices/1-0068/of_node/interrupt-parent` | 실행 중 `dtoverlay`로 적용하면 `interrupt-parent`가 `ff ff ff ff`(미해결) → IRQ 없이 probe | [조치 절차: mainline IRQ 미연결](#조치-절차-mainline-irq-미연결) |
| 8 사용자 공간 | 빠르게 돌릴 때 gyro가 `-250.137` 근처에서 멈춤 | raw 값이 `-32768`인지 | ±250°/s 범위 포화(−32768/131). 변환 오류 아님 | 빠른 회전이 대상이면 5장 "범위 변경" |

### 조치 절차: 버퍼 가득 참
1. 버퍼를 끈다. 이미 버려진 샘플은 되돌릴 수 없다.
2. 앱 하나로 다시 시작한다. 앱은 읽기 준비를 마친 뒤 버퍼를 켜고, 종료할 때 끈다.
3. 새 구간에 drop 로그가 없는지 본다.

```bash
# [Pi] 64GB 카드 — dev는 4장 명령으로 설정 (Yocto는 sudo를 빼고, 앱 대신 devcheck imu)
printf '0\n' | sudo tee "$dev/buffer/enable"
sudo ~/bsp/apps/mpu6050_iio_app
sudo dmesg | grep -F 'mpu6050-iio-study' | tail -3
```

### 조치 절차: mainline IRQ 미연결
1. 실행 중 적용한 overlay를 제거한다(`sudo dtoverlay -r i2c-sensor`).
2. `config.txt`를 백업하고 부팅 때 적용하는 줄을 추가해 재부팅한다.
3. `interrupt-parent`가 gpio phandle(이 보드 `00 00 00 07`)인지, `buffer/`가 생겼는지 본다. 끝나면 백업으로 원복한다.

```bash
# [Pi] 64GB 카드
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-t001
echo 'dtoverlay=i2c-sensor,mpu6050,int_pin=17' | sudo tee -a /boot/firmware/config.txt
sudo reboot
od -An -tx1 /sys/bus/i2c/devices/1-0068/of_node/interrupt-parent   # 재부팅 후
```

실행 중 적용에서 fixup이 왜 반영되지 않는지는 확인 필요다. 학습용 overlay는 실행 중 적용에서도 IRQ가 정상 연결됐다.

두 사례의 분석(확인한 계층 → 원인)은 [09 6-1](../learning/09-iio-direct.md#6-1-앱-없이-버퍼를-켜-두어-sample-dropped--16)·[6-2](../learning/09-iio-direct.md#6-2-실행-중-dtoverlay로-적용한-mainline-장치에-buffer가-없음)에 있다.

**점검 포인트**
- `devcheck imu`가 `capture/restore (buffer must be idle)`로 FAIL이면 다른 앱이 버퍼를 쓰고 있거나, 강제 종료된 앱이 `buffer/enable`을 1로 남긴 것이다. 앱을 끝내고 0을 쓴다.
- 수집 중 `in_*_raw`를 읽으면 `Device or resource busy`가 나오도록 만들었다(`iio_device_claim_direct`, [L102](../../examples/mpu6050_iio/mpu6050_iio.c#L102)). 버퍼가 꺼진 뒤 다시 읽는다.
- `1-0068`이 없으면 overlay 적용 여부(64GB `dtoverlay -l`, Yocto는 `/boot/config.txt`)와 `/proc/device-tree/soc/i2c@7e804000/mpu6050@68` 노드부터 본다.

## 7. 알려진 제한
- 센서 FIFO를 쓰지 않아 IRQ 처리가 늦으면 값이 덮어써질 수 있고, 긴 간격을 유실 개수로 환산할 수 없다.
- 범위·주파수는 고정이고 보정하지 않는다(정지 상태 |g| 0.92 g, 자이로 bias −2.31 / −0.60 / +0.57 °/s).
- 64GB 카드는 overlay·모듈을 부팅마다 손으로 올린다.
