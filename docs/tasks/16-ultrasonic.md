# 16 초음파 IIO srf04

상태: 완료 · 브랜치: `feat/ultrasonic` · Codex 지시서: [16-ultrasonic.codex.md](16-ultrasonic.codex.md)

## 목표
VEDA Final에서 쓰던 HC-SR04 초음파 센서를 mainline IIO 드라이버 `srf04`(`devantech,srf04`)와 직접 작성한 overlay로 연결하고, `in_distance_raw`(mm)가 실제 거리에 맞게 변하는지 확인함. 처음으로 커널 config fragment를 만들어 in-tree 모듈 하나만 빌드·설치하는 흐름을 거침. 끝나면 VEDA의 사용자 공간 busy-wait 측정과 커널 IRQ 타임스탬프 측정의 차이를 설명할 수 있음.

- 드라이버: mainline 그대로. `CONFIG_SRF04`가 꺼져 있어 `configs/bsp-study.config`에 `=m`을 추가하고 `srf04.ko`만 빌드한다
- Codex 구현: config fragment, overlay, Makefile
- 하지 않는 것: 드라이버 수정, 사용자 앱(셸로 sysfs 읽기), `power-gpios`, 커널 이미지 재빌드, 다른 장치와 동시 사용
- 마무리 때 학습 문서 `docs/learning/16-ultrasonic.md`를 만든다.

## 알아둘 개념
1. **읽을 때마다 측정하는 IIO:** buffer 없이 `in_distance_raw`를 읽을 때마다 `srf04_read()`(`drivers/iio/proximity/srf04.c` L96)가 Trig에 10µs 펄스를 내고, Echo 상승을 최대 20ms(`HZ/50`), 하강을 최대 50ms(`HZ/20`) 기다린 뒤 시간차를 mm로 바꾼다(343.5m/s 가정, L185). 40.4ms(6.45m)를 넘으면 `-EIO`. MPU6050의 IIO buffer·trigger 방식과 달리 측정이 sysfs 읽기에 묶여 있다.
2. **IRQ 문맥 나누기:** Echo GPIO를 상승·하강 양쪽 IRQ로 등록하고(L304), hard IRQ 핸들러는 `ktime_get()` 시각을 기록하고 `complete()`만 한다(L78). sysfs를 읽은 프로세스는 `wait_for_completion_killable_timeout()`으로 잠들어 기다린다. VEDA `readUltrasonicCm()`은 사용자 공간에서 `digitalRead()`를 돌며 `micros()`로 쟀다(CPU를 계속 쓰고 스케줄링에 따라 오차가 생김).
3. **config fragment → 모듈 증분 빌드:** 저장소의 `configs/bsp-study.config`에 바꿀 설정만 적고 `merge_config.sh`로 `out/kernel-rpi4/.config`에 합친다. 새 `=m` 모듈 하나만 추가하면 커널 이미지와 기존 모듈은 그대로 쓸 수 있으므로 `make modules`로 증분 빌드해 `srf04.o`만 새로 컴파일한다. `make drivers/iio/proximity/srf04.ko` 단일 빌드는 modpost가 `vmlinux.o` 기호만 보고 다른 모듈(`industrialio.ko`)이 내보내는 기호를 몰라 실패한다(2단계 결과). 이미지가 다시 만들어지지 않았는지는 `Image` 시각으로, 호환은 vermagic과 로드 결과로 확인한다.

## 진행 순서
보드 단계 출력은 **[붙여 주세요]**만 채팅에 붙인다.

### 시작 전 확인한 것 (Claude 읽기 확인)
| 항목 | 값 |
| --- | --- |
| config | `# CONFIG_SRF04 is not set`, 의존 `GPIOLIB=y`, `IIO=m`. `configs/` 디렉터리 없음(이번에 처음 만듦) |
| binding | `devantech-srf04.yaml`: 필수 `trig-gpios`(출력), `echo-gpios`(IRQ 가능 입력). 선택 `power-gpios`, `startup-time-ms` |
| rpi overlay | 없음 |
| IIO 이름 | `srf04` (L322), 채널 `in_distance_raw`·`in_distance_scale`(0.001 → m) |
| VEDA 배선 | `origin/test/tank+eis:patrol_track.cpp` L9·L15: HC-SR04, wiringPi `TRIG=4`·`ECHO=5` → BCM GPIO23(물리 16)·GPIO24(물리 18). Echo 풀다운 사용(`origin/jiho:patrol/mqtt_patrol/rc_control_node.cpp` L191). VCC·분압 기록 없음 |
| 현재 커널 이미지 | `out/kernel-rpi4/arch/arm64/boot/Image` 2026-11-02 20:26:38 |

### 0. 배선 (전원 분리 상태)
VEDA와 같은 GPIO를 쓴다. HC-SR04는 5V로 동작하고 Echo가 5V를 내므로 **Echo는 반드시 분압 후 GPIO24에 연결한다**(Pi GPIO는 3.3V, 5V 입력 금지).

| HC-SR04 | Pi 물리 핀 | 비고 |
| --- | --- | --- |
| VCC | 2 (5V) | |
| GND | 14 | |
| Trig | 16 (GPIO23) | 3.3V 출력으로 트리거됨 |
| Echo | 분압 후 18 (GPIO24) | Echo ─ 1kΩ ─┬─ GPIO24, ┴─ 2kΩ ─ GND → 약 3.3V |

분압 비율은 1:2(예: 1kΩ·2kΩ, 10kΩ·20kΩ, 2.2kΩ·4.7kΩ)면 된다. 아래 2kΩ 쪽이 GND로 가는 길이라 VEDA의 풀다운 역할도 한다.

**[붙여 주세요]** 실제 모듈 표기(HC-SR04 / HC-SR04P 등), VCC 핀, 사용한 저항 값

### 1. 구현 (Codex)
Codex에 붙여 넣기:
`docs/tasks/16-ultrasonic.codex.md를 읽고 AGENTS.md 규칙에 따라 구현 계획을 먼저 설명해줘.`

```text
configs/bsp-study.config            # CONFIG_SRF04=m
examples/ultrasonic/
  srf04-overlay.dts                 # devantech,srf04, trig GPIO23, echo GPIO24
  Makefile                          # overlay + deploy
```

구현이 끝나면 Claude에게 리뷰를 요청한다(`review`).

### 2. config 합치기와 모듈 빌드 (WSL, 리뷰 통과 후)

```bash
# [WSL] ~/bspworkspace/bsp-study
K="make -C external/linux O=$PWD/out/kernel-rpi4 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="
cp out/kernel-rpi4/.config out/kernel-rpi4/.config.bak-16
ls -l --time-style=+%F_%T out/kernel-rpi4/arch/arm64/boot/Image      # 이전 시각 기록

external/linux/scripts/kconfig/merge_config.sh -m -O out/kernel-rpi4 \
    out/kernel-rpi4/.config configs/bsp-study.config
$K olddefconfig
$K prepare
diff out/kernel-rpi4/.config.bak-16 out/kernel-rpi4/.config         # CONFIG_SRF04=m 한 줄 예상

# 모듈 전체를 증분 빌드: 이미 빌드된 것은 건너뛰고 srf04.o만 새로 컴파일, modpost는 전체 모듈 기호로 연결
$K -j4 modules
ls -l --time-style=+%F_%T out/kernel-rpi4/arch/arm64/boot/Image      # 시각 그대로 예상
wc -l out/kernel-rpi4/modules.order                                  # 수천 줄(1줄이면 단일 빌드 흔적)
modinfo -F vermagic out/kernel-rpi4/drivers/iio/proximity/srf04.ko  # 6.18.59-v8-bsp-study01 ...
modinfo -F alias out/kernel-rpi4/drivers/iio/proximity/srf04.ko | grep srf04
scp out/kernel-rpi4/drivers/iio/proximity/srf04.ko pi:bsp/modules/
```

복구: `cp out/kernel-rpi4/.config.bak-16 out/kernel-rpi4/.config` 후 `$K olddefconfig prepare`. `diff`에 다른 줄이 있으면 진행하지 않고 붙여 준다.

**[붙여 주세요]** `diff`, `Image` 시각 두 줄, vermagic

### 3. 설치와 overlay 적용 (Pi)

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/ultrasonic
make deploy
```

```bash
# [Pi] 모듈 설치 위치에 같은 파일이 없는지 확인 → 설치
ls /lib/modules/$(uname -r)/kernel/drivers/iio/proximity/ 2>&1
sudo install -D -m 644 ~/bsp/modules/srf04.ko /lib/modules/$(uname -r)/kernel/drivers/iio/proximity/srf04.ko
sudo depmod -a
modinfo -n srf04

# config 백업 후 overlay 적용
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-16
sudo cp ~/bsp/overlays/srf04.dtbo /boot/firmware/overlays-bsp-study01/
echo 'dtoverlay=srf04' | sudo tee -a /boot/firmware/config.txt
diff /boot/firmware/config.txt.bak-16 /boot/firmware/config.txt
sudo reboot
```

### 4. probe와 거리 읽기 (Pi)

```bash
# [Pi] 재부팅 후
lsmod | grep '^srf04 '                                   # insmod 없이 자동 적재
sudo dmesg | grep -i -E 'srf04|proximity'
D=$(dirname "$(grep -l '^srf04$' /sys/bus/iio/devices/iio:device*/name)"); echo "$D"
cat "$D/in_distance_scale"                               # 0.001000 (raw 1 = 1mm)
grep -i proximity /proc/interrupts                       # Echo IRQ 줄
```

벽이나 책을 센서 앞 10cm, 30cm, 50cm에 두고 각 위치에서:

```bash
# [Pi]
for i in $(seq 10); do cat "$D/in_distance_raw"; sleep 0.1; done | tr '\n' ' '; echo
```

**[붙여 주세요]** 위 출력과 세 거리의 값

### 5. IRQ와 오류 경로 (Pi)

```bash
# [Pi] 한 번 읽을 때 IRQ가 몇 번 느는지
grep -i proximity /proc/interrupts; cat "$D/in_distance_raw"; grep -i proximity /proc/interrupts
# 앞을 비우거나(먼 천장 방향) 센서를 손으로 막았을 때
for i in $(seq 5); do cat "$D/in_distance_raw" 2>&1; sleep 0.1; done
```

확인: 한 번 읽을 때 IRQ 2 증가(상승·하강), 측정 범위 밖이면 `Input/output error`(`-EIO`) 또는 `Connection timed out`(`-ETIMEDOUT`).

**[붙여 주세요]**

### 6. 원복 (Pi)

```bash
# [Pi]
sudo cp /boot/firmware/config.txt.bak-16 /boot/firmware/config.txt
grep -c 'srf04' /boot/firmware/config.txt               # 0
sudo reboot
# 재부팅 후 lsmod | grep srf04 → 없음, srf04 IIO 장치 없음
```

`srf04.ko`는 이 커널의 in-tree 모듈이라 설치한 채 둔다. DT 노드가 없으면 적재되지 않는다. 지울 때는 `sudo rm /lib/modules/$(uname -r)/kernel/drivers/iio/proximity/srf04.ko && sudo depmod -a`.

### 문제가 생기면
- `srf04` 장치가 없음: `lsmod`에 없으면 `modinfo -n srf04`·`depmod` 확인, 있으면 dmesg의 `failed to get trig-gpios`·`gpiod_to_irq` 오류를 본다.
- 모든 읽기가 `Connection timed out`: Echo 상승이 안 옴 → Trig·Echo 배선, VCC 5V, 분압 연결점을 확인한다.
- 값이 들쭉날쭉: 반사면을 넓고 평평한 것으로 바꾸고 측정 간격(60ms 이상)을 지킨다.
- `insmod: invalid module format`: `srf04.ko` vermagic·`$K prepare` 실행 여부를 확인한다.

## 완료 기준
- [x] 배선(Echo 분압) 확인 (저항 값은 미기록)
- [x] Codex 구현, WSL 빌드 확인, 리뷰 통과
- [x] fragment 합친 뒤 `.config` 차이가 `CONFIG_SRF04=m`뿐, `Image` 시각 그대로, vermagic 일치
- [x] `srf04` 자동 적재와 IIO 장치 등록
- [x] 거리가 멀어지면 값이 커짐 (사용자 판단으로 거리별 수치 비교는 생략)
- [x] 한 번 읽기에 IRQ 2 증가 (범위 밖 오류는 미관측)
- [x] 원복 후 장치 없음

## 결과
대표 출력 몇 줄과 판단만 적는다.

**2단계 config 합치기와 모듈 빌드**
- `merge_config.sh`: `Previous value: # CONFIG_SRF04 is not set` → `New value: CONFIG_SRF04=m`. `olddefconfig`·`prepare` 후 `diff`는 이 한 줄뿐
- 첫 시도 `make drivers/iio/proximity/srf04.ko`: 컴파일은 됐으나 `ERROR: modpost: "devm_iio_device_alloc" ... undefined!` 등 IIO 기호 3개 실패 → 단일 모듈 빌드의 modpost는 `vmlinux.o` 기호만 보고, `industrialio.ko`(`CONFIG_IIO=m`)가 내보내는 기호는 모름(`scripts/Makefile.modpost` L104~111). 부작용으로 `modules.order`가 1줄로 덮어써짐
- `make -j4 modules`(증분): `CC [M] kernel/configs.o`(`.config`를 담는 `CONFIG_IKCONFIG` 모듈), `MODPOST`, `LD [M] drivers/iio/proximity/srf04.ko`만 실행 → 다른 드라이버는 다시 컴파일되지 않음
- `Image` 시각 `2026-11-02_20:26:38` 그대로, `modules.order` 1918줄로 복구, vermagic `6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64`, alias `of:N*T*Cdevantech,srf04`
- `srf04.ko` → `pi:bsp/modules/` 전송

**3·4단계 설치·probe·거리**
- 설치 전 `kernel/drivers/iio/proximity/` 디렉터리 없음(이 커널에서 처음 켠 proximity 드라이버) → 덮어쓴 파일 없음, `modinfo -n srf04`가 in-tree 위치를 가리킴
- `config.txt` `diff`는 `> dtoverlay=srf04` 한 줄
- 재부팅 후 `insmod` 없이 `srf04 12288 0`, `iio:device0` 이름 `srf04`, `/proc/interrupts`에 `59: ... pinctrl-bcm2835 24 Edge proximity`. 성공 시 dmesg 로그는 없음(드라이버가 남기지 않음)
- 거리: 가까이 두고 움직이며 31~51mm, 멀리 하면 값이 커지는 것을 사용자가 확인. 거리별 수치 비교는 생략

**5단계 IRQ**
- 한 번 읽기 전후 IRQ 22 → 24: 읽기 한 번에 Echo 상승·하강 2회. 4단계 10회 읽기(20회) + 부팅 직후 2회 = 22와도 맞음
- 오류 경로: 이어진 5회가 모두 35mm로 앞에 물체가 있는 상태였고 `-EIO`/`-ETIMEDOUT`은 관측하지 않음

**6단계 원복**
- `config.txt` 원복 후 재부팅, `lsmod`에 `srf04` 없음, `/sys/bus/iio/devices/` 자체가 없음(IIO 장치 0개). `srf04.ko`는 설치한 채 둠

- 미검증: 거리별 정확도(±cm), 범위 밖 오류(`-EIO`/`-ETIMEDOUT`), 부팅 직후 IRQ 2회의 원인, 배선에 쓴 분압 저항 값 기록
