# T-001 mainline inv_mpu6050로 같은 센서 읽고 학습용 드라이버와 비교

상태: 완료
관련 roadmap 단계: 9 (학습용 드라이버와 기존 드라이버 번갈아 검증)
유형: B mainline 드라이버+DT
트랙: 없음 (`claude-test` 브랜치)

## 목표 (Claude)
같은 GY-521(MPU6050, I2C1 0x68, INT GPIO17)을 대상 커널의 mainline 드라이버 `inv_mpu6050`에 rpi `i2c-sensor` overlay로 연결하고, 커널 표준 도구로 raw·scale·버퍼를 확인한다. 끝나면 학습용 드라이버([mpu6050_iio.c](../../examples/mpu6050_iio/mpu6050_iio.c))와 채널·scan 배치·IRQ 설정·timestamp·FIFO 처리의 차이를 실측 근거로 설명할 수 있다. 2부 13~17단계에서 반복할 "mainline 드라이버 + overlay + 표준 도구 검증" 흐름을 IMU로 먼저 익힌다.

## 배경 개념 (Claude)
1. **rpi overlay 파라미터:** `i2c-sensor` overlay는 센서별 노드를 `__dormant__` fragment로 갖고, `mpu6050` 파라미터가 해당 fragment(28번)와 INT 핀 fragment를 켠다. `int_pin=17`은 `interrupts` 첫 셀과 `brcm,pins`를 함께 바꾼다. 근거: `external/linux/arch/arm/boot/dts/overlays/i2c-sensor-common.dtsi`(L425~, L925~), overlays `README`의 i2c-sensor 항목.
2. **DT의 IRQ 종류가 칩 INT 설정을 정한다:** mainline은 `irq_get_trigger_type()`으로 DT의 IRQ 종류를 읽어 칩 INT_PIN_CFG를 맞춘다(`IRQ_TYPE_EDGE_FALLING` → active-low). 학습용 드라이버는 level HIGH + LATCH 고정이다. 근거: `drivers/iio/imu/inv_mpu6050/inv_mpu_core.c` L1923~1939.
3. **센서 FIFO와 timestamp 추정:** mainline은 센서 FIFO에서 샘플을 꺼내고 `inv_sensors_timestamp`로 칩 클럭 주기를 추정해 시각을 붙인다. 학습용 드라이버는 FIFO 없이 IRQ 관측 시각을 쓴다. 근거: `inv_mpu_ring.c`, `inv_mpu_core.c` L584~613, `include/linux/iio/common/inv_sensors_timestamp.h`.

## 확인한 사실 (Claude, 2026-11-10 읽기 확인, 보드 미확인)
| 항목 | 값 |
| --- | --- |
| 드라이버 | `drivers/iio/imu/inv_mpu6050/` (core·i2c·ring·trigger) |
| config | `CONFIG_INV_MPU6050_I2C=m`, `CONFIG_INV_MPU6050_IIO=m`, `CONFIG_IIO_TRIGGERED_BUFFER=m`, `CONFIG_IIO_KFIFO_BUF=m` |
| binding | `Documentation/devicetree/bindings/iio/imu/invensense,mpu6050.yaml` |
| overlay | `i2c-sensor` + `mpu6050` 파라미터 (구 `mpu6050` overlay는 deprecated). 노드: `compatible = "invensense,mpu6050"`, `reg = <0x68>`, `interrupts = <4 IRQ_TYPE_EDGE_FALLING>`, pull none. I2C `clock-frequency = <400000>` 포함 |
| dtbo 위치 | `out/pi4-install/boot/firmware/overlays-bsp-study01/i2c-sensor.dtbo` 존재(배포 폴더). Pi `/boot/firmware/overlays-bsp-study01/`의 존재는 확인 필요 |
| scan 배치(소스) | accel X/Y/Z 0~2, temp 3, gyro X/Y/Z 4~6, timestamp 7 (`inv_mpu_iio.h` L437) |
| 표준 도구 | `tools/iio/iio_generic_buffer.c` (옵션 `-a -A -b -c -e -g -l -n -N -t -T -w`) |
| Pi 모듈 설치 | 확인 필요 (`modinfo inv-mpu6050-i2c`) |

## 범위 (Claude)
- 변경 파일: 없음
- 새로 만드는 파일: 없음 (저장소 밖 산출물만). `iio_generic_buffer` 크로스 빌드 결과는 `out/tools-iio/` 등 Git 제외 경로에 둔다
- 하지 않는 것: 학습용 드라이버·앱·`imu_plot.py` 수정, config fragment 변경, 커널 재빌드, `config.txt` 영구 적용, 60초 재측정, `libiio-utils` 설치(Yocto 단계에서 필요하면 검토)
- 범위 변경 (2026-11-10 사용자 승인): 실행 중 `dtoverlay` 적용에서 `interrupt-parent`가 해결되지 않아(실측 2차 진단) 버퍼·trigger가 생성되지 않았다. 검증 동안만 `config.txt`에 `dtoverlay=i2c-sensor,mpu6050,int_pin=17` 한 줄을 추가해 부팅 시 적용하고, 끝나면 백업으로 원복한다

## 완료 기준 (Claude)
- [x] Pi에 `inv-mpu6050-i2c`·`inv-mpu6050` 모듈이 대상 커널용으로 설치되어 있다
- [x] 학습용 overlay·모듈이 없는 상태에서 `i2c-sensor mpu6050 int_pin=17` 적용 후 `1-0068/driver`가 `inv-mpu6050-i2c`, IIO 장치 `name`이 `mpu6050`이다
- [ ] 같은 기울어진 정지 자세에서 raw × scale이 60초 기준선(ax ≈ -5.77, ay ≈ -2.17, az ≈ -6.62 m/s²)과 노이즈 범위 안에서 같다 → 부분 충족(마감 참조)
- [x] `iio_generic_buffer`로 6축+timestamp 버퍼 샘플을 받고, GPIO17 IRQ 횟수가 증가한다
- [ ] overlay 제거 후 바인딩·IIO 장치·IRQ 행이 사라진다 → 부분 충족(마감 참조)
- [x] 학습용 드라이버와의 차이표(채널·scan 배치·scale/주파수 선택·IRQ 종류·timestamp 출처·FIFO)를 실측과 읽기 확인으로 구분해 작성한다

## 검증 절차 (Claude)
선행: 센서 배선은 [bringup](../bringup.md#gy-521-배선) 그대로. 학습용 드라이버와 mainline을 같은 0x68에 동시에 붙이지 않는다.

```bash
# [WSL] ~/bspworkspace/bsp-study — Codex 산출물 배포 (정확한 경로는 구현 보고 기준)
scp out/tools-iio/iio_generic_buffer pi:bsp/apps/
```

```bash
# [Pi] 1. 모듈·dtbo 확인과 학습용 드라이버 정리
uname -r                                          # 6.18.59-v8-bsp-study01
modinfo -F vermagic inv-mpu6050-i2c               # 6.18.59-v8-bsp-study01로 시작
ls /boot/firmware/overlays-bsp-study01/i2c-sensor.dtbo
sudo dtoverlay -l                                 # mpu6050-iio-overlay가 있으면 아래 두 줄
# sudo dtoverlay -r mpu6050-iio-overlay && sudo rmmod mpu6050_iio
ls -ld /sys/bus/i2c/devices/1-0068                # No such file or directory

# [Pi] 2. mainline 연결
sudo dtoverlay -d /boot/firmware/overlays-bsp-study01 i2c-sensor mpu6050 int_pin=17
sleep 1
readlink -e /sys/bus/i2c/devices/1-0068/driver
lsmod | grep -E '^inv_mpu6050'
dev=$(grep -lx 'mpu6050' /sys/bus/iio/devices/iio:device*/name | xargs -r dirname); echo "$dev"
sudo dmesg | grep -iE 'inv.?mpu|mpu6050' | tail -5

# [Pi] 3. sysfs 구조·raw·scale (센서는 60초 기준선과 같은 자세)
ls "$dev"
for ch in accel_x accel_y accel_z anglvel_x anglvel_y anglvel_z; do printf '%s: ' "$ch"; cat "$dev/in_${ch}_raw"; done
cat "$dev/in_accel_scale" "$dev/in_anglvel_scale" "$dev/sampling_frequency"
cat "$dev"/in_accel_scale_available "$dev"/in_anglvel_scale_available 2>/dev/null
grep -H . "$dev"/scan_elements/*_index "$dev"/scan_elements/*_type
ls /sys/bus/iio/devices/ | grep -i trigger; cat /sys/bus/iio/devices/trigger*/name

# [Pi] 4. 버퍼 수집 (Codex 구현 보고 기준: 온도 제외 6축+timestamp를 sysfs로 선택, -a 없이 실행)
getconf GNU_LIBC_VERSION                          # 도구 요구 GLIBC_2.34 이상
cat "$dev/sampling_frequency_available"
echo 0 | sudo tee "$dev/buffer/enable"
for f in "$dev"/scan_elements/*_en; do echo 0 | sudo tee "$f" >/dev/null; done
for ch in in_accel_x in_accel_y in_accel_z in_anglvel_x in_anglvel_y in_anglvel_z in_timestamp; do
    echo 1 | sudo tee "$dev/scan_elements/${ch}_en" >/dev/null
done
grep -H . "$dev"/scan_elements/*_en
grep -iE 'mpu|inv' /proc/interrupts
sudo ~/bsp/apps/iio_generic_buffer -n mpu6050 -l 16 -c 5
grep -iE 'mpu|inv' /proc/interrupts
cat "$dev/buffer/enable"                          # 0
# 수동으로 켠 채널은 도구가 끄지 않으므로 되돌린다
for f in "$dev"/scan_elements/*_en; do echo 0 | sudo tee "$f" >/dev/null; done

# [Pi] 5. 정리
sudo dtoverlay -r i2c-sensor
ls -ld /sys/bus/i2c/devices/1-0068 "$dev"         # 둘 다 No such file or directory
grep -iE 'mpu|inv' /proc/interrupts               # 출력 없음
```

### 범위 변경 후 절차: 부팅 시 적용 (2026-11-10)
위 2단계(`dtoverlay` 실행 중 적용)와 5단계(`dtoverlay -r`) 대신 아래를 쓴다. 3·4단계는 그대로 사용한다. 부팅 실패 시 SD 카드 리더로 `config.txt` 마지막 줄을 지운다.

```bash
# [Pi] A. 백업·추가·재부팅 (학습용 overlay·모듈이 없는 상태에서)
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.bak-t001
echo 'dtoverlay=i2c-sensor,mpu6050,int_pin=17' | sudo tee -a /boot/firmware/config.txt
tail -4 /boot/firmware/config.txt
sudo reboot

# [Pi] B. 재부팅 후 IRQ 연결 확인 → 정상이면 3·4단계
uname -r
readlink -e /sys/bus/i2c/devices/1-0068/driver
n=$(readlink -f /sys/bus/i2c/devices/1-0068/of_node)
od -An -tx1 "$n/interrupt-parent"                 # 00 00 00 07 기대
grep -iE 'mpu|inv' /proc/interrupts
dev=$(grep -lx 'mpu6050' /sys/bus/iio/devices/iio:device*/name | xargs -r dirname); echo "$dev"
ls "$dev" | grep -E 'buffer|scan_elements|trigger'

# [Pi] C. 원복
sudo cp /boot/firmware/config.txt.bak-t001 /boot/firmware/config.txt
sudo reboot
# 재부팅 후
ls -ld /sys/bus/i2c/devices/1-0068                # No such file or directory
```

## 구현 보고 (Codex)
- 변경 요약 (2026-11-10):
  - `claude-test` 브랜치에서 진행했다. 기존 코드·Makefile·overlay·config는 수정하지 않았고 이 구현 보고만 기록했다.
  - 대상 커널의 표준 `iio_generic_buffer`를 크로스 빌드했다. 실행 파일·오브젝트·헤더 링크·빌드 보조 산출물은 Git 제외 경로 `out/tools-iio/`에 생성했다.
  - `out/kernel-rpi4/include/config/kernel.release`는 `6.18.59-v8-bsp-study01`, 읽은 `external/linux` HEAD는 `b3495ea401d709af0701a85babcba5e3d788a602`이다.
  - `.config`의 `INV_MPU6050_IIO`, `INV_MPU6050_I2C`, `IIO_TRIGGERED_BUFFER`, `IIO_KFIFO_BUF`가 모두 `m`이고 배포 폴더의 `i2c-sensor.dtbo`가 존재함을 확인했다. Pi 설치 여부는 미확인이다.
- 빌드 명령·결과:

```bash
# [WSL] ~/bspworkspace/bsp-study
mkdir -p out/tools-iio
make -C external/linux/tools/iio \
    OUTPUT="$PWD/out/tools-iio/" CROSS_COMPILE=aarch64-linux-gnu- \
    "$PWD/out/tools-iio/iio_generic_buffer"
file out/tools-iio/iio_generic_buffer
aarch64-linux-gnu-readelf -h -l -V out/tools-iio/iio_generic_buffer
git check-ignore out/tools-iio/iio_generic_buffer
```

  - `make` 종료 코드 0, 컴파일·링크 경고 없음. `file`·`readelf`로 ELF64 little-endian AArch64 PIE 실행 파일임을 확인했다. Git 제외 적용도 확인했다.
  - 동적 링크이며 로더는 `/lib/ld-linux-aarch64.so.1`, 요구 GLIBC 심볼 버전의 최댓값은 `GLIBC_2.34`다. Pi 사용자 공간과의 호환성·실행 성공은 보드 확인이 필요하다.
- checkpatch 결과 (`checkpatch.pl --no-tree -f <파일>`): 해당 없음(저장소 C 소스 변경 없음)
- 판단이 필요했던 부분:
  - 기존 `tools/iio/Makefile`의 단일 실행 파일 타깃을 사용해 불필요한 도구는 빌드하지 않았다. 도구의 채널 활성화·trigger 연결·버퍼 읽기·SI 단위 변환·정리 코드는 변경하지 않았다.
  - `-a`는 활성 채널이 없으면 온도까지 포함해 전부 켠다. 완료 기준에 맞춰 6축+timestamp만 sysfs로 선택하고 `-a` 없이 실행하도록 아래 명령을 확정했다. mainline의 `inv_mpu_scan_masks`는 온도 없는 6축 조합을 지원한다.
  - `-c 5`는 읽기 반복 5회이며 샘플 5개를 보장하지 않는다. 짧은 수집을 위해 `-l 16`을 지정한다. `-e`·`-g`는 사용하지 않아 trigger와 poll 기반 수집을 유지한다.
  - 출력 열은 scan index 순으로 정렬되어 accel X/Y/Z, gyro X/Y/Z, timestamp다. 6축 값은 도구가 scale·offset을 적용한 SI 단위이고 timestamp는 정수 ns다. 온도의 scan index 3을 꺼도 gyro index가 재번호화되지는 않는다.
  - 아래 비교는 모두 소스 읽기 확인이다. 보드 실측·기능 정상 판정과 최종 비교표의 문서 반영은 아직 하지 않았다.

| 항목 | 학습용 `mpu6050_iio.c` | 대상 커널 mainline | 보드 확인 |
| --- | --- | --- | --- |
| 채널 | 6축+timestamp | 6축+온도+timestamp | 미확인 |
| scan index | accel 0~2, gyro 3~5, timestamp 6 | accel 0~2, temp 3, gyro 4~6, timestamp 7 | 미확인 |
| scale | accel `0.000598550`, gyro `0.000133231` 고정 | accel 4종·gyro 4종 선택 가능. ±2 g는 `0.000598`, ±250 deg/s는 `0.000133090` | sysfs 값·raw×scale 미확인 |
| 주파수·초기 범위 | 공칭 100 Hz, ±2 g·±250 deg/s 고정 | sampling_frequency 변경 가능. 초기 공칭 50 Hz, ±2 g·±2000 deg/s | 실제 초기값 미확인 |
| IRQ | level HIGH + active-high LATCH 고정 | DT에서 획득. 이번 overlay는 falling edge → active-low, 비래치 | GPIO17 IRQ 증가 미확인 |
| timestamp | hard IRQ에서 관측한 `iio_get_time_ns()`, CLOCK_MONOTONIC 지정 | IRQ 시각을 기준으로 `inv_sensors_timestamp`가 주기를 추정하고 FIFO 샘플별 시각 생성 | 출력·간격 미확인 |
| FIFO | 센서 FIFO 없이 IRQ마다 출력 레지스터 읽기 | 센서 FIFO의 완전한 샘플을 일괄 읽기, overflow 임박 시 FIFO 재설정 | 수집 성공 미확인 |

  - 비교 근거: `examples/mpu6050_iio/mpu6050_iio.c`, `external/linux/drivers/iio/imu/inv_mpu6050/{inv_mpu_core.c,inv_mpu_iio.h,inv_mpu_trigger.c,inv_mpu_ring.c}`, `external/linux/tools/iio/{iio_generic_buffer.c,iio_utils.c}`. overlay·binding도 TASK에 지정된 대상 커널 파일을 읽었다. 참고 저장소 VEDA·VEDA_Final_Project는 이번 작업에서 읽지 않았다.
- 사용자가 보드에서 실행할 명령:
  - 다음 명령은 준비한 절차이며 Codex가 실행하지 않았다. TASK 상단 ‘검증 절차’의 1~3단계와 5단계는 유지하고, 배포 명령 및 4단계는 아래를 사용한다. 확인 단계에서 모듈·dtbo가 없거나 기존 `1-0068`이 남으면 연결을 진행하지 말고 출력을 확인한다.

```bash
# [WSL] ~/bspworkspace/bsp-study
# Pi의 ~/bsp/apps/가 준비되어 있는지 확인한 뒤 배포한다.
scp out/tools-iio/iio_generic_buffer pi:bsp/apps/
```

```bash
# [Pi] 실행 호환성 및 두 모듈 확인. 이어서 상단 검증 절차 1~3단계를 수행한다.
getconf GNU_LIBC_VERSION
test -x /lib/ld-linux-aarch64.so.1
modinfo -F vermagic inv-mpu6050-i2c
modinfo -F vermagic inv-mpu6050

# [Pi] 검증 절차 4단계 대체. 앞 단계와 같은 셸에서 실행한다.
# mainline mpu6050 장치가 하나만 연결된 상태가 선행 조건이다.
dev=$(grep -lx 'mpu6050' /sys/bus/iio/devices/iio:device*/name | xargs -r dirname)
test -n "$dev" && test -d "$dev" || exit 1
cat "$dev/sampling_frequency" "$dev/sampling_frequency_available"

# 버퍼 정지 상태에서 온도를 제외한 6축+timestamp를 명시적으로 선택한다.
echo 0 | sudo tee "$dev/buffer/enable"
for f in "$dev"/scan_elements/*_en; do
    echo 0 | sudo tee "$f"
done
for ch in in_accel_x in_accel_y in_accel_z in_anglvel_x in_anglvel_y in_anglvel_z in_timestamp; do
    echo 1 | sudo tee "$dev/scan_elements/${ch}_en"
done
grep -H . "$dev"/scan_elements/*_en
grep -H . "$dev"/scan_elements/*_index "$dev"/scan_elements/*_type
grep -iE 'mpu|inv' /proc/interrupts
sudo ~/bsp/apps/iio_generic_buffer -n mpu6050 -l 16 -c 5
grep -iE 'mpu|inv' /proc/interrupts
cat "$dev/buffer/enable"

# 수동 선택한 채널은 도구가 해제하지 않으므로 수집 후 되돌린다.
for f in "$dev"/scan_elements/*_en; do
    echo 0 | sudo tee "$f"
done

# [Pi] 검증 절차 5단계: 동적으로 추가한 overlay를 제거한다.
sudo dtoverlay -r i2c-sensor
ls -ld /sys/bus/i2c/devices/1-0068 "$dev"
grep -iE 'mpu|inv' /proc/interrupts
```

  - 마지막 `ls`의 두 경로 부재와 IRQ 검색 출력 없음은 정리 후 예상 결과다. 수집 중 데이터가 오지 않으면 Ctrl+C로 도구를 중단하고 `buffer/enable`이 0인지 확인한 뒤 채널 해제·overlay 제거 순서로 정리한다.
  - raw×scale 비교는 상단 3단계에서 센서를 기존 기준선과 같은 자세로 유지하고 수행한다. mainline scale의 반올림과 초기 gyro 범위가 학습용과 다르므로 실제 sysfs scale을 사용한다. 60초 재측정이나 설정 변경은 하지 않는다.
  - 보드 모듈 설치·바인딩·raw/scale·버퍼·IRQ·overlay 해제는 모두 미검증이며 완료 기준 체크박스는 유지한다.

## 리뷰 (Claude)
2026-11-10. 통과 → 상태 `실측`.

- [x] 범위 준수: 저장소에서 바뀐 것은 이 TASK 파일 한 개(`git status`). `external/linux` 작업 트리도 깨끗하고 산출물은 Git 제외 경로 `out/tools-iio/`에만 있다. config 변경 없음(유형 B 기준 충족)
- [x] 오류·해제 경로: 해당 없음(커널 코드 변경 없음). 도구 정리 동작 확인: `iio_generic_buffer`는 자기가 켠 채널만 끈다(`tools/iio/iio_generic_buffer.c` L303, autochannels가 ACTIVE일 때만). 그래서 수동으로 켠 채널은 다시 꺼야 한다는 보고 내용이 맞다
- [x] IRQ·잠금 문맥: 해당 없음
- [x] 과잉 구현 없음: 단일 타깃 `iio_generic_buffer`만 빌드했다. `-a` 대신 6축+timestamp를 명시적으로 고른 것은 완료 기준(6축+timestamp)에 맞춘 판단이다
- [x] 커널 스타일·checkpatch: 해당 없음(C 소스 변경 없음)
- 보고 내용의 소스 대조 결과:
  - 초기값 50Hz·±2000 deg/s·±2 g: `inv_mpu_core.c` L111~122와 일치
  - trigger 이름 `%s-dev%d`: `inv_mpu_trigger.c` L306과 도구 기본값 L520이 일치하므로 `-t` 없이 연결된다
  - `sampling_frequency_available`(10~500)·`in_*_scale_available`: `inv_mpu_core.c` L1687~1707
  - 산출물: `file` 결과 ELF64 AArch64 PIE
- 보드 명령 수정(Claude, 검증 절차에 반영): 보고의 `test ... || exit 1`은 대화형 SSH 셸에서 실행하면 셸이 닫히므로 뺐다. 경로 확인은 2단계의 `echo "$dev"` 출력으로 대신한다. 4단계는 보고의 채널 선택·수집·해제 순서를 그대로 옮겼다
- 해석 주의: mainline 초기 gyro 범위는 ±2000 deg/s라서 gyro raw가 학습용(±250)보다 약 1/8로 작다. 비교는 raw가 아니라 각 드라이버 sysfs의 raw × scale로 한다. ±2 g accel scale 표기는 학습용 `0.000598550`, mainline `0.000598`(반올림 표기)
- 수정 지시 (Codex용): 없음
- 이해 확인 질문:
  1. **같은 센서인데 mainline 드라이버는 `IRQ_TYPE_EDGE_FALLING`, 학습용은 level HIGH를 쓴다. 둘 다 동작하는 이유는?** → 드라이버가 DT의 IRQ 종류를 읽어 칩 INT 극성·래치(INT_PIN_CFG)를 그에 맞게 설정하기 때문이다(`irq_get_trigger_type`). falling edge이면 칩이 active-low 펄스를 내고, 학습용은 active-high를 INT_STATUS를 읽을 때까지 래치한다. 극성은 DT와 칩 설정이 짝을 이뤄야 한다.
  2. **mainline은 왜 센서 FIFO를 쓰고 timestamp를 "추정"하는가?** → IRQ 처리가 늦어져도 센서가 FIFO에 샘플을 쌓아 두므로 유실을 줄이고, IRQ 한 번에 여러 샘플을 읽는다. 그러면 샘플마다 IRQ 시각이 없으므로, IRQ 시각과 칩 클럭 주기로 각 샘플의 시각을 계산한다(`inv_sensors_timestamp`). 학습용은 FIFO 없이 샘플 1개당 IRQ 1번이라 IRQ 관측 시각을 그대로 쓴다.
  3. **같은 0x68에 학습용 overlay와 `i2c-sensor mpu6050`을 동시에 적용하면 안 되는 이유는?** → 같은 버스·주소의 I2C client가 두 번 만들어질 수 없고(주소 충돌로 두 번째 등록 실패), 설령 다른 경로로 접근해도 두 드라이버가 같은 레지스터·INT를 서로 다르게 설정해 IRQ 극성·데이터가 꼬인다. 그래서 한쪽 overlay·모듈을 내린 뒤 다른 쪽을 적용한다.

## 실측 결과 (사용자 제공, Claude 기록)
사용자가 준 보드 출력·로그의 원문 일부(대표 로그)와 요약을 적는다. 해석은 마감에서 한다.

### 1차 실행 (2026-11-10)
- 요약: 모듈·dtbo 존재, mainline 바인딩·IIO 장치 생성, sysfs raw·scale 조회 성공. `buffer/`·`scan_elements/`·trigger가 없어 4단계 실패. `/proc/interrupts`에 해당 행 없음. overlay 제거 후 정리 확인.

```text
6.18.59-v8-bsp-study01
6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64
/boot/firmware/overlays-bsp-study01/i2c-sensor.dtbo
No overlays loaded
ls: cannot access '/sys/bus/i2c/devices/1-0068': No such file or directory
# dtoverlay -d /boot/firmware/overlays-bsp-study01 i2c-sensor mpu6050 int_pin=17
/sys/bus/i2c/drivers/inv-mpu6050-i2c
inv_mpu6050_i2c        16384  0
inv_mpu6050            36864  1 inv_mpu6050_i2c
/sys/bus/iio/devices/iio:device0
[ 3687.681326] inv-mpu6050-i2c 1-0068: mounting matrix not found: using identity...
[ 3687.681360] inv-mpu6050-i2c 1-0068: supply vdd not found, using dummy regulator
[ 3687.683791] inv-mpu6050-i2c 1-0068: supply vddio not found, using dummy regulator
# ls "$dev" (buffer·scan_elements·trigger 항목 없음)
current_timestamp_clock in_accel_matrix in_accel_mount_matrix in_accel_scale in_accel_scale_available
in_accel_{x,y,z}_calibbias in_accel_{x,y,z}_raw in_anglvel_mount_matrix in_anglvel_scale
in_anglvel_scale_available in_anglvel_{x,y,z}_calibbias in_anglvel_{x,y,z}_raw in_gyro_matrix
in_temp_offset in_temp_raw in_temp_scale name of_node power sampling_frequency
sampling_frequency_available subsystem uevent waiting_for_supplier
accel_x: -9656
accel_y: -3538
accel_z: -11064
anglvel_x: -43
anglvel_y: -10
anglvel_z: 9
0.000598
0.001064724
50
grep: /sys/bus/iio/devices/iio:device0/scan_elements/*_index: No such file or directory
glibc 2.41
tee: '/sys/bus/iio/devices/iio:device0/buffer/enable': No such file or directory
iio device number being used is 0
Failed to find the trigger mpu6050-trigger
# grep -iE 'mpu|inv' /proc/interrupts → 수집 전후 모두 출력 없음
# dtoverlay -r i2c-sensor 후
ls: cannot access '/sys/bus/i2c/devices/1-0068': No such file or directory
ls: cannot access '/sys/bus/iio/devices/iio:device0': No such file or directory
```

### 2차 진단 (2026-11-10, 같은 dtoverlay 명령으로 재적용 후 live DT 조회)
- 요약: 노드에 `interrupts`(GPIO17, 2 = falling)는 있으나 `interrupt-parent`가 `0xffffffff`(미해결 phandle). gpio 노드 phandle은 `0x07`.
- Claude 읽기 확인(WSL): 배포한 `i2c-sensor.dtbo`의 `__fixups__`에 `gpio = ... "/fragment@28/__dormant__/mpu6050@68:interrupt-parent:0"`가 있고, 배포 파일은 빌드 결과와 같다(`cmp`).

```text
/sys/firmware/devicetree/base/soc/i2c@7e804000/mpu6050@68
compatible  interrupt-parent  interrupts  name  phandle  pinctrl-0  pinctrl-names  reg
 00 00 00 11 00 00 00 02
 ff ff ff ff
/soc/gpio@7e200000
 00 00 00 07
[ 3892.809336] OF: overlay: WARNING: memory leak will occur if overlay removed, property: /soc/i2c@7e804000/clock-frequency
[ 3892.809367] OF: overlay: WARNING: memory leak will occur if overlay removed, property: /soc/i2c@7e804000/status
[ 3892.814137] leds-gpio leds: Failed to create device link (0x180) with supplier fe200000.gpio for /leds/led-act
[ 3892.814390] inv-mpu6050-i2c 1-0068: mounting matrix not found: using identity...
[ 3893.195933] i2c i2c-1: Added multiplexed i2c bus 22
```

### 3차 실행: config.txt 부팅 시 적용 (2026-11-10)
- 요약: `interrupt-parent` = gpio phandle `0x07`, GPIO17 Edge IRQ `inv_mpu` 등록, `buffer`·`scan_elements`·`trigger` 생성. scan 배치는 소스와 같고, `iio_generic_buffer`로 6축+timestamp 5샘플을 받았다(IRQ 0 → 5). 원복 후의 `ls`는 `1-0068`이 남아 있다고 나왔는데, 재부팅 완료 전에 실행된 출력인지 확인이 필요하다.

```text
6.18.59-v8-bsp-study01
/sys/bus/i2c/drivers/inv-mpu6050-i2c
 00 00 00 07
 61:          0          0          0          0 pinctrl-bcm2835  17 Edge      inv_mpu
/sys/bus/iio/devices/iio:device0
buffer
buffer0
scan_elements
trigger
accel_x: -9732
accel_y: -3622
accel_z: -11108
anglvel_x: -47
anglvel_y: -5
anglvel_z: 9
in_accel_x_index:0  in_accel_y_index:1  in_accel_z_index:2  in_temp_index:3
in_anglvel_x_index:4  in_anglvel_y_index:5  in_anglvel_z_index:6  in_timestamp_index:7
in_accel_*_type / in_anglvel_*_type / in_temp_type: be:s16/16>>0
in_timestamp_type: le:s64/64>>0
iio device number being used is 0
iio trigger number being used is 0
/sys/bus/iio/devices/iio:device0 mpu6050-dev0
-5.763524 -2.091804 -6.627036 -0.046848 -0.004259 0.011712 1794243198960687246
-5.769504 -2.126488 -6.635408 -0.047913 -0.010647 0.010647 1794243198980687066
-5.785052 -2.104960 -6.622252 -0.042589 -0.013841 0.006388 1794243199000696986
-5.744388 -2.110940 -6.604312 -0.042589 -0.017036 0.006388 1794243199020713546
-5.768308 -2.106156 -6.659328 -0.043654 -0.011712 0.007453 1794243199040732886
 61:          5          0          0          0 pinctrl-bcm2835  17 Edge      inv_mpu
0
# 원복: cp config.txt.bak-t001 → reboot → (재부팅 후로 붙여넣은) ls
lrwxrwxrwx 1 root root 0 Nov 10 01:51 /sys/bus/i2c/devices/1-0068 -> ../../../devices/platform/soc/fe804000.i2c/i2c-1/1-0068
```

### 원복 재확인 (2026-11-10, 재접속 후)
- 요약: 재부팅 1분 후 `config.txt`에 `i2c-sensor` 줄 없음, `1-0068` 없음, `inv_mpu6050` 모듈 미적재. 3차 실행 끝의 `ls` 결과는 재부팅이 끝나기 전에 실행된 것으로 본다.

```text
 01:55:34 up 1 min,  4 users,  load average: 0.51, 0.26, 0.10
# grep -n 'i2c-sensor' /boot/firmware/config.txt → 출력 없음
ls: cannot access '/sys/bus/i2c/devices/1-0068': No such file or directory
# lsmod | grep -E '^inv_mpu6050' → 출력 없음
```

## 마감 (Claude)
2026-11-10. 완료 기준 6개 중 4개 충족, 2개 부분 충족. 사용자 승인으로 마감.

- 완료 기준 판정:
  1. 충족: `inv-mpu6050-i2c` vermagic `6.18.59-v8-bsp-study01`, 두 모듈 `lsmod` 적재. `inv-mpu6050`의 vermagic 출력은 없음
  2. 충족: `driver` → `inv-mpu6050-i2c`. `name`이 `mpu6050`인 장치를 찾는 명령으로 `iio:device0`을 찾음, trigger `mpu6050-dev0`
  3. 부분 충족: 버퍼 5샘플 평균 ax/ay/az = -5.766/-2.108/-6.630 m/s², 기준선과의 차이 +0.005/+0.065/-0.015. \|g\|의 1% 이내지만 ay는 60초 σ(0.017)의 약 4배. 자세 재현과 50/100Hz 필터 차이를 분리하지 못함
  4. 충족: 3차 실행(부팅 시 적용)에서 6축+timestamp 5샘플, IRQ 0 → 5, `buffer/enable` 0으로 복귀
  5. 부분 충족: 1차 `dtoverlay -r`에서 `1-0068`·`iio:device0` 제거 확인(이때는 IRQ 행이 처음부터 없음). 원복 재부팅 뒤 `1-0068` 부재·모듈 미적재만 확인
  6. 충족: [IIO 문서 2-4](../learning/09-iio-direct.md#2-4-mainline-inv_mpu6050와의-차이)에 실측과 읽기 확인을 구분해 작성
- 마감 해석:
  - timestamp 간격은 20.000~20.019 ms로 50Hz와 맞는다. 값 1794243198 s는 2026-11-10 01:53 KST이므로 CLOCK_REALTIME(IIO 기본값)이다. 학습용은 CLOCK_MONOTONIC
  - 실행 중 적용의 buffer 부재 원인: `interrupt-parent`가 미해결(`ff ff ff ff`)이라 IRQ가 없었다. mainline은 `irq > 0`일 때만 triggered buffer를 만든다(`inv_mpu_core.c` L2086, 읽기 확인)
- 갱신한 문서:
  - `examples/mpu6050_iio/iio-direct.md`:
    - 2-1의 mainline 소스 재확인 문구 갱신
    - 2-3에 대응 항목 추가
    - 2-4 차이표·그림 신설
    - 4-7 재현 절차 신설
    - 5장 트러블슈팅 "실행 중 dtoverlay로 적용한 mainline 장치에 buffer가 없음" 추가
    - 6장 mainline 비교 수치·미검증 추가
  - `docs/worklog.md`: 현재 단계, 완료 요약, 막힌 점, 다음 작업(roadmap 11단계 U-Boot)
  - `docs/roadmap.md`: 9단계 "번갈아 검증" 완료 표시, 10단계 "기존 드라이버와 비교"는 부분 비교로 기록
  - bringup.md·README: 배선·장치 변경이 없고 `config.txt`를 원복해 갱신하지 않음
- 미검증 항목:
  - 원복 뒤 `/proc/interrupts`의 `inv_mpu` 행과 IIO 장치 부재를 직접 확인하지 않음
  - 기준선과 같은 자세·주파수에서 mainline 장기 수집을 하지 않아 ay 차이 원인을 분리하지 못함
  - 실행 중 `dtoverlay`에서 dtbo에 fixup이 있는데도 `interrupt-parent`가 해결되지 않은 원인(확인 필요)
  - 센서 FIFO 여러 샘플 일괄 읽기와 overflow 재설정, timestamp 추정 정확도, 범위·주파수 변경 동작
  - 학습용 드라이버에서 IRQ가 없을 때 probe가 실패하는지(읽기 확인만 함)
- 병합 시 반영 (트랙 TASK만, worklog·roadmap·README): 해당 없음(트랙 밖 TASK)
- 추천 커밋 메시지: `Docs: mainline inv_mpu6050 연결과 학습용 IIO 드라이버 비교 기록`
