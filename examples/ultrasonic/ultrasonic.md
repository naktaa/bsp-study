# 초음파 HC-SR04: config fragment로 켠 mainline IIO srf04

- [1. 한눈에 보기](#1-한눈에-보기)
- [2. 구조](#2-구조)
- [3. 알아야 할 개념](#3-알아야-할-개념)
- [4. 구현 흐름](#4-구현-흐름)
- [5. 재현](#5-재현)
- [6. 트러블슈팅과 한계](#6-트러블슈팅과-한계)

## 1. 한눈에 보기

- **목적:** VEDA Final에서 사용자 공간 wiringPi로 읽던 HC-SR04 초음파 센서를 커널 IIO 드라이버로 처리한다. 대상 커널에 드라이버는 있지만 꺼져 있어서, 커널 config fragment로 켜고 모듈 하나만 추가하는 흐름을 처음 거친다.
- **기능:** 저장소 첫 config fragment [bsp-study.config](../../configs/bsp-study.config)(`CONFIG_SRF04=m`), overlay [srf04-overlay.dts](srf04-overlay.dts)(`devantech,srf04`, Trig GPIO23·Echo GPIO24). 드라이버 코드는 쓰지 않았다.
- **결과(사용자 보드 실행):** 커널 이미지를 다시 빌드하지 않고 `srf04.ko`만 추가했다. 부팅 때 모듈이 자동 적재되어 IIO 장치 `srf04`가 생겼고, `in_distance_raw`(mm)가 멀어질수록 커지는 것을 확인했다. 한 번 읽을 때 Echo IRQ가 2씩 늘었다. 원복 후 장치가 없어졌다. 거리별 정확도와 범위 밖 오류는 확인하지 않았다.
- **관련 파일:** [bsp-study.config](../../configs/bsp-study.config), [srf04-overlay.dts](srf04-overlay.dts), [Makefile](Makefile), 작업 노트 [16-ultrasonic](../../docs/tasks/16-ultrasonic.md)·[Codex 지시서](../../docs/tasks/16-ultrasonic.codex.md), 사양 [hardware.md](../../docs/hardware.md#초음파-hc-sr04-계열), 배선 [bringup.md](../../docs/bringup.md#40핀-한눈에-보기). 커널 소스(`external/linux`, Git 대상 아님): `drivers/iio/proximity/srf04.c`, `Documentation/devicetree/bindings/iio/proximity/devantech-srf04.yaml`. VEDA 비교: `../VEDA_Final_Project` 브랜치 `origin/test/tank+eis`의 `patrol_track.cpp`, `origin/jiho`의 `patrol/mqtt_patrol/rc_control_node.cpp`

## 2. 구조

**흐름:** `cat in_distance_raw` → `srf04_read()`가 Trig에 10µs 펄스 → 센서가 Echo를 왕복 시간만큼 HIGH → Echo GPIO 상승·하강 IRQ에서 시각 기록 → 시간차를 mm로 변환 → sysfs 값.

```text
 +-----------+  Trig (GPIO23)  <---  +--------------------+          +------------------+
 | HC-SR04   |                       | srf04 driver       |  IIO     | sysfs            |
 | 5V        |  Echo --[1:2]-->      | read_raw: trig,    |--------->| in_distance_raw  |
 +-----------+  divider (GPIO24)     | wait, convert      |          | in_distance_scale|
                       |             +---------^----------+          +--------+---------+
                       | edge IRQ              | complete()                   |
                       v                       |                              v
               +----------------+    +---------+----------+          +------------------+
               | pinctrl-bcm2835|--->| hard IRQ handler   |          | cat (process)    |
               | GPIO IRQ 59    |    | ktime_get()        |          | sleeps on wait   |
               +----------------+    +--------------------+          +------------------+
```

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 장치 | HC-SR04 | Trig 펄스를 받으면 40kHz 송신, Echo를 왕복 시간만큼 HIGH(5V) |
| 배선 | 1:2 분압 | Echo 5V → GPIO24 약 3.3V. 아래 저항이 GND로 가는 길이 되어 풀다운 역할 |
| 빌드 설정 | `configs/bsp-study.config` | `CONFIG_SRF04=m`을 `out/kernel-rpi4/.config`에 합침 |
| DT | `srf04` overlay | 루트에 `proximity` 노드, `trig-gpios`·`echo-gpios` |
| 드라이버 | `srf04` (IIO) | GPIO 취득, Echo 양쪽 엣지 IRQ 등록, 읽을 때마다 측정 |
| 프레임워크 | IIO core | `in_distance_raw`·`in_distance_scale` sysfs 노출 |

## 3. 알아야 할 개념

### 왜 드라이버를 쓰지 않고 config만 바꿨는가

`drivers/iio/proximity/srf04.c`가 대상 커널에 있고 binding도 필수 속성이 `trig-gpios`·`echo-gpios` 두 개뿐이다. HC-SR04는 SRF04와 같은 방식(Trig 펄스 → Echo 폭)이라 `devantech,srf04`로 동작했다. 빠진 것은 빌드 설정(`# CONFIG_SRF04 is not set`)과 이 보드의 핀을 적은 DT였다. "드라이버는 있는데 config가 꺼져 있다"는 BSP에서 흔한 상황이고, 바꿀 설정만 fragment로 저장소에 남긴다.

### 왜 모듈 하나만 빌드하면 실패했는가

`make drivers/iio/proximity/srf04.ko`는 컴파일까지 됐지만 modpost에서 `devm_iio_device_alloc` 등 IIO 함수 3개가 undefined로 실패했다. 이 함수들은 커널 이미지가 아니라 `industrialio.ko`(`CONFIG_IIO=m`)가 내보낸다. in-tree 단일 모듈 빌드의 modpost는 `vmlinux.o` 기호와 이번에 빌드한 모듈만 보고 기존 `Module.symvers`를 읽지 않는다(`scripts/Makefile.modpost` L104~111). `make modules` 증분 빌드는 전체 모듈 목록으로 modpost를 돌려 해결됐고, 새로 컴파일된 것은 `srf04.o`와 `.config`를 담는 `kernel/configs.o`뿐이었다.

### 왜 커널 이미지를 다시 만들지 않아도 되는가

추가한 설정이 `=m` 하나라 vmlinux에 들어가는 코드가 바뀌지 않는다. 확인은 세 가지로 했다: `.config` 차이가 `CONFIG_SRF04=m` 한 줄, 빌드 후 `Image` 시각이 그대로, `srf04.ko` vermagic이 Pi 커널(`6.18.59-v8-bsp-study01`)과 같음. 그래서 기존 커널에 `srf04.ko`만 `kernel/drivers/iio/proximity/`에 넣고 `depmod -a`로 별칭 표를 갱신하면, DT 노드가 생길 때 udev가 MODALIAS(`of:N*T*Cdevantech,srf04`)로 모듈을 자동 적재한다([14-3 코덱 모듈](../mic/mic.md#외부-모듈은-왜-자동-적재되지-않았는가)과 같은 경로).

### 왜 IRQ 핸들러와 읽는 프로세스가 나뉘어 있는가

hard IRQ 핸들러는 Echo 레벨로 상승·하강을 구분해 `ktime_get()` 시각을 기록하고 `complete()`만 한다. 잠들 수 없는 문맥이라 할 일을 최소로 둔다. 기다리는 쪽은 sysfs를 읽은 프로세스로, `wait_for_completion_killable_timeout()`으로 잠든다(상승 20ms, 하강 50ms). VEDA `readUltrasonicCm()`은 사용자 공간에서 `digitalRead()`를 돌며 `micros()`로 쟀다. CPU를 측정 내내 쓰고, 그 사이 스케줄링되면 시간이 늘어난다. 커널 방식은 엣지 시점에 바로 시각을 찍고 그 사이에는 CPU를 놓는다.

### 왜 MPU6050 IIO와 사용법이 다른가

MPU6050 IIO는 trigger·buffer로 장치가 정한 주기의 샘플을 `/dev/iio:deviceN`으로 받았다. srf04는 `INDIO_DIRECT_MODE`만 있어 `in_distance_raw`를 읽을 때마다 한 번 측정한다. 측정 주기는 읽는 쪽이 정하고, 센서 사양상 60ms 이상 간격을 둔다.

## 4. 구현 흐름

1. **config를 합친다:** `merge_config.sh -m`이 fragment를 `.config`에 덮어쓰고, `olddefconfig`가 의존성(GPIOLIB=y, IIO=m)을 정리하고, `prepare`가 `auto.conf`·`autoconf.h`에 반영한다.
2. **모듈을 증분 빌드한다:** `make modules`가 `srf04.o`를 컴파일하고 전체 modpost로 IIO 기호를 연결해 `srf04.ko`를 만든다.
3. **Pi에 설치한다:** `kernel/drivers/iio/proximity/srf04.ko`에 넣고 `depmod -a`. 이 커널에서 처음 켠 proximity 드라이버라 디렉터리도 새로 생겼다.
4. **firmware가 DT를 합친다:** `dtoverlay=srf04`로 [srf04-overlay.dts](srf04-overlay.dts)가 적용되어 루트에 `proximity` 노드가 생긴다.
   - [L11](srf04-overlay.dts#L11) `compatible = "devantech,srf04"`
   - [L13](srf04-overlay.dts#L13) `trig-gpios = <&gpio 23 0>`, `echo-gpios = <&gpio 24 0>`. `&gpio`는 2셀(번호, 플래그), 플래그 0은 ACTIVE_HIGH
5. **probe:** udev가 `srf04`를 적재하고, 드라이버가 Trig를 출력 LOW로, Echo를 입력으로 받고 `gpiod_to_irq()`로 얻은 IRQ를 상승·하강 양쪽으로 등록한 뒤 IIO 장치 `srf04`를 등록한다. `/proc/interrupts`에 `pinctrl-bcm2835 24 Edge proximity`(이름은 DT 노드 이름)로 보인다.
6. **읽기:** `cat in_distance_raw` → Trig 10µs → 상승 IRQ·하강 IRQ 각각 1회 → `시간(ns) × 106 / 617176` = mm(343.5m/s 가정). 40.4ms(6.45m)를 넘으면 `-EIO`. `in_distance_scale`은 0.001(raw 1 = 1mm, 단위 m).

## 5. 재현

전체 단계와 출력은 [작업 노트](../../docs/tasks/16-ultrasonic.md)에 있다. 배선(전원 분리 상태): VCC → 물리 2(5V), GND → 14, Trig → 16(GPIO23), Echo → 1:2 분압 → 18(GPIO24). Echo는 5V라 분압 없이 연결하지 않는다.

```bash
# [WSL] ~/bspworkspace/bsp-study — config 합치기와 모듈 빌드
K="make -C external/linux O=$PWD/out/kernel-rpi4 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="
cp out/kernel-rpi4/.config out/kernel-rpi4/.config.bak-16
external/linux/scripts/kconfig/merge_config.sh -m -O out/kernel-rpi4 \
    out/kernel-rpi4/.config configs/bsp-study.config
$K olddefconfig
$K prepare
diff out/kernel-rpi4/.config.bak-16 out/kernel-rpi4/.config         # CONFIG_SRF04=m 한 줄
$K -j4 modules
modinfo -F vermagic out/kernel-rpi4/drivers/iio/proximity/srf04.ko
scp out/kernel-rpi4/drivers/iio/proximity/srf04.ko pi:bsp/modules/
cd examples/ultrasonic && make && make deploy                       # srf04.dtbo → pi:bsp/overlays/
```

```bash
# [Pi] 모듈 설치, config 백업 후 overlay 적용
sudo install -D -m 644 ~/bsp/modules/srf04.ko /lib/modules/$(uname -r)/kernel/drivers/iio/proximity/srf04.ko
sudo depmod -a
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-16
sudo cp ~/bsp/overlays/srf04.dtbo /boot/firmware/overlays-bsp-study01/
echo 'dtoverlay=srf04' | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

```bash
# [Pi] 재부팅 후 (새 셸마다 D를 다시 정한다)
lsmod | grep '^srf04 '
D=$(dirname "$(grep -l '^srf04$' /sys/bus/iio/devices/iio:device*/name)"); echo "D=$D"
grep -i proximity /proc/interrupts; cat "$D/in_distance_raw"; grep -i proximity /proc/interrupts   # IRQ +2
```

```bash
# [Pi] 원복 (srf04.ko는 in-tree 모듈이라 남겨 둔다)
sudo cp /boot/firmware/config.txt.bak-16 /boot/firmware/config.txt
sudo reboot
```

**확인한 값**

| 항목 | 값 |
| --- | --- |
| `.config` 차이 | `# CONFIG_SRF04 is not set` → `CONFIG_SRF04=m` |
| 빌드 후 `Image` | `2026-11-02_20:26:38` 그대로 |
| 자동 적재 | `srf04 12288 0`, `iio:device0` 이름 `srf04` |
| IRQ | `59: ... pinctrl-bcm2835 24 Edge proximity`, 읽기 1회에 22 → 24 |
| 거리 | 가까이 31~51mm, 멀어질수록 커짐(사용자 확인) |

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| `make .../srf04.ko`에서 `modpost: "devm_iio_device_alloc" ... undefined!` | 단일 모듈 빌드의 modpost가 `industrialio.ko` 기호를 모름 | `make modules` 증분 빌드 |
| 단일 빌드 실패 뒤 `modules.order`가 1줄 | 단일 빌드가 목록을 덮어씀 | `make modules`가 다시 만듦(1918줄). 그 전에 `modules_install` 금지 |
| 재부팅 후 `cat: /in_distance_raw: No such file` | 새 셸이라 `D` 변수가 비어 있음 | `D=...` 줄을 다시 실행 |
| probe 성공인데 dmesg에 `srf04` 로그 없음 | 드라이버가 성공 시 로그를 남기지 않음 | `lsmod`·IIO `name`·`/proc/interrupts`로 확인 |

- **미검증:** 거리별 정확도(줄자 비교), 범위 밖 `-EIO`/`-ETIMEDOUT`(측정 때 앞에 물체가 있었음), 부팅 직후 IRQ 2회의 원인, 분압 저항 값과 실물 모듈 표기(VEDA 기록상 HC-SR04), VEDA busy-wait 방식과의 오차 비교.
- **한계:** 측정이 sysfs 읽기에 묶여 있어 일정 주기 수집은 사용자 공간이 맡는다. 음속은 20°C 고정(343.5m/s)이다. Pi의 `configs.ko`(`/proc/config.gz`)는 갱신하지 않아 이전 설정으로 보인다.
