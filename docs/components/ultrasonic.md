# 초음파: HC-SR04 (업스트림 IIO srf04, config fragment + 직접 작성 overlay)

## 1. 요약
- **역할:** `in_distance_raw`를 읽을 때마다 한 번 측정해 거리(mm)를 IIO sysfs로 준다.
- **상태:** 두 카드에서 `devcheck sonar` PASS(5회 읽기), 거리 변화를 따라가는 것 확인. 줄자 비교 정확도, 범위 밖 오류(`-EIO`·`-ETIMEDOUT`)는 미검증([16 6장](../learning/16-ultrasonic.md#6-트러블슈팅과-한계)).
- **담당 파일:** overlay [srf04-overlay.dts](../../examples/ultrasonic/srf04-overlay.dts), config fragment [configs/bsp-study.config](../../configs/bsp-study.config)·[Yocto bsp-study.cfg](../../yocto/meta-bsp-study/recipes-kernel/linux/files/bsp-study.cfg)(`CONFIG_SRF04=m`). 드라이버는 업스트림 `drivers/iio/proximity/srf04.c`
- **학습 문서:** [16 초음파](../learning/16-ultrasonic.md)

## 2. 구성
흐름: `cat in_distance_raw → srf04_read()가 Trig 10µs → Echo 상승·하강 IRQ에서 ktime 기록 → 시간차 × 106 / 617176 = mm`

| 항목 | 64GB 카드 (6.18.59 Debian) | Yocto 카드 (6.6.63) |
| --- | --- | --- |
| HW 연결 | VCC 5V(물리 2), Trig → GPIO23(물리 16), Echo → **1:2 분압** → GPIO24(물리 18) ([bringup](../bringup.md#40핀-한눈에-보기)) | 같음 |
| DT/overlay | `srf04.dtbo`를 `overlays-bsp-study01/`에 두고 `config.txt`의 `dtoverlay=srf04` | `config.txt`의 `dtoverlay=srf04` |
| 커널 config | 기본 config에 없음 → `configs/bsp-study.config`로 `CONFIG_SRF04=m` | 같은 줄을 `bsp-study.cfg`로 |
| 드라이버 | in-tree `srf04.ko`만 추가 설치(커널 이미지 재빌드 없음) | 커널 레시피가 함께 빌드 |
| 자동 적재 | `depmod -a` 후 `devantech,srf04` modalias로 udev가 올림 | 같음 |
| 사용자 인터페이스 | `/sys/bus/iio/devices/iio:deviceN`(name `srf04`), `in_distance_raw`(mm), `in_distance_scale` 0.001 | 같음 |

- trigger·buffer가 없는 `INDIO_DIRECT_MODE`라 측정 주기는 읽는 쪽이 정한다. 센서 사양상 60ms 이상 간격을 둔다.
- 40.4ms(6.45m)를 넘으면 `-EIO`다. 음속은 343.5m/s 고정이다.

## 3. 빌드·배포
- **64GB 카드:** fragment를 합치고 모듈만 증분 빌드해 설치한다(커널 빌드는 [kernel](kernel.md)).

```bash
# [WSL] ~/bspworkspace/bsp-study
K="make -C external/linux O=$PWD/out/kernel-rpi4 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="
external/linux/scripts/kconfig/merge_config.sh -m -O out/kernel-rpi4 out/kernel-rpi4/.config configs/bsp-study.config
$K olddefconfig && $K prepare && $K -j4 modules
scp out/kernel-rpi4/drivers/iio/proximity/srf04.ko pi:bsp/modules/
make -C examples/ultrasonic && make -C examples/ultrasonic deploy
# [Pi] 64GB
sudo install -D -m 644 ~/bsp/modules/srf04.ko /lib/modules/$(uname -r)/kernel/drivers/iio/proximity/srf04.ko && sudo depmod -a
```

이후 `config.txt`를 백업하고 `dtoverlay=srf04`를 추가해 재부팅한다([16 5장](../learning/16-ultrasonic.md#5-재현)).

- **Yocto 카드:** 커널 bbappend가 `bsp-study.cfg`를 합치고 overlay를 `srf04.dtbo`로 만든다. 별도 레시피는 없다.

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| 설정 | `[WSL] grep CONFIG_SRF04 out/kernel-rpi4/.config` | `CONFIG_SRF04=m` |
| 모듈 | `lsmod \| grep '^srf04 '` | 있음 |
| IIO 장치 | `D=$(dirname "$(grep -l '^srf04$' /sys/bus/iio/devices/iio:device*/name)"); echo $D` | 경로 하나 |
| IRQ | `grep -i proximity /proc/interrupts` 전후로 `cat $D/in_distance_raw` | `pinctrl-bcm2835 24 Edge proximity` 횟수가 읽기 1회에 2 증가 |
| 거리 | `cat $D/in_distance_raw` | mm 정수, 멀어질수록 커짐 |
| 한 번에 | `devcheck sonar` | raw 5개, PASS |

드라이버는 probe 성공 때 로그를 남기지 않는다. dmesg 대신 위 표로 확인한다.

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| Trig·Echo 핀 변경 | overlay의 `trig-gpios`·`echo-gpios` | [bringup](../bringup.md#40핀-한눈에-보기) 핀 표, Echo 분압 유지 | `/proc/interrupts`의 GPIO 번호 |
| 다른 커널 기능 켜기 | `configs/bsp-study.config`와 Yocto `bsp-study.cfg` 둘 다 | `=y`면 커널 이미지 재설치([kernel](kernel.md#5-변경-가이드)) | `.config` diff |
| 측정 주기 | 사용자 공간 읽기 간격 | 60ms 이상 | — |

## 6. 트러블슈팅
먼저 볼 계층: 1 빌드 → 4 모듈 적재 → 7 IIO sysfs.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 1 빌드 | `srf04.ko`만 단일 빌드하면 modpost `undefined!`, `modules.order` 1줄 | — | in-tree 단일 모듈 빌드의 한계 | [kernel 6장](kernel.md#6-트러블슈팅) 참고. `make modules` 증분 빌드 |
| 5 매칭·probe | probe는 됐는데 dmesg에 `srf04` 로그가 없음 | `lsmod`, IIO `name` | 드라이버가 성공 시 로그를 남기지 않음 | 4장 표로 확인 |

**점검 포인트**
- Echo는 5V 출력이다. 분압 없이 GPIO24에 연결하지 않는다.
- 읽을 때 `Input/output error`면 앞에 물체가 없거나 6.45m를 넘은 것이다(`-EIO`, 보드에서 재현하지 않음).
- Pi의 `/proc/config.gz`에는 `CONFIG_SRF04`가 보이지 않는다. `configs.ko`를 다시 설치하지 않았기 때문이다. 기준은 `out/kernel-rpi4/.config`다.

## 7. 알려진 제한
- 측정이 sysfs 읽기에 묶여 있어 일정 주기 수집은 사용자 공간이 맡는다.
- 음속이 20°C 기준 고정이라 온도 보정이 없다.
- 부팅 직후 IRQ 2회의 원인은 확인하지 않았다.
