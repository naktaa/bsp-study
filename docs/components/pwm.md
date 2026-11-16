# PWM: PCA9685(팬·틸트 서보)와 SoC 내장 모터 PWM

## 1. 요약
- **역할:**
  - PCA9685: I2C1 `0x40`의 16채널 PWM 칩. 서보용 20ms 주기 PWM을 PWM sysfs로 낸다.
  - 모터 PWM: SoC 내장 PWM 두 채널(GPIO12/13)로 모터 드라이버 EN 핀에 PWM을 낸다.
- **상태:** 두 카드에서 `devcheck pca`·`devcheck pwm` PASS. 레지스터·sysfs 값까지만 확인했고 서보·모터 실구동, 핀 파형, 방향 GPIO, 온보드 오디오 충돌은 미검증([15 6장](../learning/15-pca9685.md#6-트러블슈팅과-한계), [18 6장](../learning/18-integration.md#6-트러블슈팅과-한계)). 17단계(모터)는 제외했다.
- **담당 파일:** 업스트림 드라이버(`pwm-pca9685`, `pwm-bcm2835`)와 커널 트리 overlay(`i2c-pwm-pca9685a`, `pwm-2chan`)를 쓰므로 직접 작성한 것은 `config.txt` 줄([bsp-study-devices.inc](../../yocto/meta-bsp-study/conf/bsp-study-devices.inc))뿐이다. 비교용 직접 작성 provider: [pca9685_simple.c](../../examples/pca9685/pca9685_simple.c)
- **학습 문서:** [15 PCA9685](../learning/15-pca9685.md), [18 통합](../learning/18-integration.md)

## 2. 구성
흐름: `sysfs pwmchipN/pwmM(period·duty_cycle·enable) → PWM core(pwm_apply) → provider → (PCA9685: I2C 레지스터 / 내장: PWM 블록) → 핀`

| 항목 | 64GB 카드 (6.18.59 Debian) | Yocto 카드 (6.6.63) |
| --- | --- | --- |
| PCA9685 연결 | I2C1 `0x40`(IMU와 SDA·SCL 공유), VCC 3.3V 레일 ([bringup](../bringup.md#40핀-한눈에-보기)) | 같음 |
| PCA9685 overlay | `dtoverlay=i2c-pwm-pca9685a,addr=0x40`(실행 중 `dtoverlay` 또는 `config.txt`) | `config.txt` 같은 줄 |
| PCA9685 드라이버 | 업스트림 `pwm-pca9685`(driver `pca9685-pwm`, npwm 17). 15-B 비교 때만 `pca9685_simple` | 업스트림 `pwm-pca9685` |
| 모터 PWM 연결 | GPIO12(물리 32) L_EN, GPIO13(물리 33) R_EN. 방향 GPIO5·6·16·26은 계획 | 같음 |
| 모터 PWM overlay | `dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4`(18 비교 때 `config.txt`) | `config.txt` 같은 줄 |
| 모터 PWM 드라이버 | 업스트림 `pwm-bcm2835`(platform `*.pwm`) | 같음 |
| 사용자 인터페이스 | `/sys/class/pwm/pwmchipN`(번호는 probe 순서로 정해짐) | 같음 |

- I2S 마이크가 GPIO18~21을 쓰므로 모터 PWM을 GPIO12/13(ALT0, `func=4`)으로 옮겼다.
- 두 카드의 PCA9685 레지스터는 1카운트 다르다. 6.6 드라이버와 6.18 드라이버의 반올림 방식이 달라서다(6장).

## 3. 빌드·배포
업스트림 드라이버와 커널 트리 overlay라 빌드할 것이 없다. overlay 한 줄과 sysfs 설정만 한다.

```bash
# [Pi] 64GB — 실행 중 적용(시험용). udev가 모듈을 비동기로 올리므로 settle 후 확인
sudo dtoverlay i2c-pwm-pca9685a && sudo udevadm settle
P=$(ls -d /sys/bus/i2c/devices/1-0040/pwm/pwmchip*); echo "$P"
echo 0 | sudo tee $P/export
echo 20000000 | sudo tee $P/pwm0/period; echo 1500000 | sudo tee $P/pwm0/duty_cycle; echo 1 | sudo tee $P/pwm0/enable
```

비교용 `pca9685_simple`은 [15 5장](../learning/15-pca9685.md#5-재현)의 blacklist 절차로 올린다. Yocto 카드는 `RPI_EXTRA_CONFIG`의 두 줄로 부팅 때 적용된다.

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| PCA9685 바인딩 | `readlink /sys/bus/i2c/devices/1-0040/driver` | `.../pca9685-pwm` |
| PCA9685 칩 | `ls /sys/bus/i2c/devices/1-0040/pwm/` | `pwmchipN` 하나, `npwm` 17 |
| PCA9685 레지스터(64GB) | `i2cget -f -y 1 0x40 0xfe` (20ms 설정 후) | `0x79` |
| 모터 PWM 칩 | `ls /sys/bus/platform/drivers/bcm2835-pwm/*.pwm/pwm/` | `pwmchipN` 하나(ch0·ch1 사용) |
| 한 번에 | `devcheck pca`, `devcheck pwm` | PCA: 20ms/1.5ms enable, PRE_SCALE `0x79`. 모터: ch0·ch1 1ms/0.5ms enable. PASS |

`devcheck`는 PWM 칩을 드라이버 경로로 찾고, 설정을 바꾼 뒤 원래대로 돌려놓는다. 드라이버가 붙어 있을 때(`i2cdetect`에 `UU`) 레지스터는 `i2cget -f` 읽기만 한다.

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| PCA9685 주소 변경 | `config.txt`의 `addr=`(Yocto는 `bsp-study-devices.inc`) | 모듈 A0~A5 점퍼, `devcheck.c`의 `1-0040` 경로와 `read_pca()` 주소 | `i2cdetect -y 1` |
| 모터 PWM 핀 변경 | `pwm-2chan`의 `pin`·`func`·`pin2`·`func2` | GPIO별 ALT 기능 번호, [bringup](../bringup.md#40핀-한눈에-보기) 핀 표, I2S·다른 장치 충돌 | `/sys/kernel/debug/pinctrl`(미확인) |
| 서보 각도 범위 | 사용자 공간 duty 값(1.0~2.0ms) | 서보 사양 | 실구동(미검증) |
| 직접 작성 provider로 교체(64GB) | `/etc/modprobe.d/`에 `blacklist pwm_pca9685` | 같은 DT 노드에 드라이버만 바꿔 붙임 | driver `pca9685-simple`, npwm 16 |

## 6. 트러블슈팅
먼저 볼 계층: 5 매칭·probe(`1-0040/driver`) → 7 PWM sysfs.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 4 모듈 적재 | overlay 적용 직후 `lsmod`·`readlink`가 비어 있음 | 잠시 뒤 다시 확인 | udev가 모듈을 비동기로 올리기 전에 명령이 실행됨 | `sudo udevadm settle` 뒤 확인 |
| 6 버스 | 한 번 바인딩한 뒤 `i2cdetect`에 `0x70`이 안 보임 | MODE1의 ALLCALL 비트 | probe가 ALLCALL을 끄고, 모듈을 내려도 돌아오지 않음 | 정상. 필요하면 드라이버가 없을 때 [조치 절차: 칩 기본값 복원](#조치-절차-칩-기본값-복원) |
| 7 서브시스템 | (업스트림) request하지 않은 채널이 `enabled, 0/주기` | — | full off를 주기 있는 duty 0으로 보고하는 waveform 변환 | 실제 출력은 꺼짐. 조치 없음 |
| 7 서브시스템 | 두 채널을 쓰는 중 주기 변경이 `Invalid argument` | 다른 채널 enable 상태 | PRE_SCALE은 16채널 공통. 업스트림은 짧은 주기만 거부하고 긴 주기는 내림, `pca9685_simple`은 모두 거부 | 모든 채널을 끄고 주기를 바꿈 |
| 8 사용자 공간 | 같은 1.5ms인데 LED0이 64GB `00 00 33 01`, Yocto `00 00 34 01`, MODE1도 `0x20`/`0x00` | 커널 버전 | 307.2카운트를 6.18은 버림(307), 6.6은 `DIV_ROUND_UP_ULL`로 올림(308). 6.18만 probe에서 `MODE1_AI`를 켬 | 드라이버 버전 차이. 조치 없음 |

### 조치 절차: 칩 기본값 복원
1. 드라이버를 내려 `i2cdetect -y 1`에서 `40`이 `UU`가 아닌지 확인한다(바인딩 중에는 쓰지 않는다).
2. 데이터시트 기본값을 쓴다. MODE1을 먼저 SLEEP으로 둬야 PRE_SCALE이 반영된다.
3. 값을 읽어 확인한다. 쓴 레지스터는 작업 노트에 남긴다.

```bash
# [Pi] 64GB — 드라이버 미바인딩 상태에서만
i2cset -y 1 0x40 0x00 0x11                   # MODE1 = SLEEP|ALLCALL
i2cset -y 1 0x40 0xfe 0x1e                   # PRE_SCALE = 30
i2cget -y 1 0x40 0x00; i2cget -y 1 0x40 0xfe # 0x11, 0x1e
```

**점검 포인트**
- PCA9685 VCC가 5V에 꽂히면 모듈 풀업 때문에 SDA·SCL이 5V가 된다. 전원을 넣기 전에 3.3V 레일인지 본다.
- `unexport`는 출력을 끄지 않는다. 정리할 때는 `enable`에 0을 먼저 쓴다.
- 서보·모터 전원은 Pi 5V가 아니라 외부 전원을 쓰고 GND만 공통으로 둔다.

## 7. 알려진 제한
- 서보·모터 실구동과 핀 파형은 확인하지 않았다(레지스터·sysfs까지).
- 온보드 아날로그 오디오와 내장 PWM 블록 충돌, 방향 GPIO는 확인하지 않았다(17단계 제외).
- `pca9685_simple`은 학습용이라 ALL_LED·`invert`·`open-drain`·GPIO·`pm_runtime`이 없다.
