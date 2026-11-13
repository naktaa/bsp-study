# 17 내장 PWM 신호 확인

상태: 계획 · 브랜치: `feat/motor-pwm` · Codex 지시서: 없음(코드 작성 없음)

## 목표
업스트림 `pwm-bcm2835` + `pwm-2chan` overlay로 GPIO12/13 하드웨어 PWM을 설정하고, 방향 GPIO(5·6·16·26) 출력 값까지 확인함. 모터 드라이버 보드·모터 전원은 연결하지 않고 직접 작성도 없음. 끝나면 SoC 내장 PWM provider가 sysfs 설정을 PERIOD/DUTY 레지스터로 옮기는 방식과, 핀 기능(ALT0)이 PWM 출력을 고르는 방식을 설명할 수 있음.

## 알아둘 개념
1. **내장 PWM provider:** base DT `pwm@7e20c000`(`bcm283x.dtsi` L413)은 `brcm,bcm2835-pwm`, PWM 클럭 50MHz(`assigned-clock-rates`), 기본 `disabled`. overlay가 켜면 `pwm-bcm2835`가 2채널 pwmchip을 등록함(`devm_pwmchip_alloc(dev, 2, …)`)
   - period·duty 단계 수 = `DIV_ROUND_CLOSEST(ns × 50MHz / 1e9)`. 한 단계 20ns, 2단계 미만 주기는 `-EINVAL`(`PERIOD_MIN`)
   - 드라이버에 `get_state`가 없어 debugfs `actual configuration`은 하드웨어 값이 아님(`pwm_get_state_hw`가 `-EOPNOTSUPP`). 15 PCA9685(waveform 콜백)와 다른 점
2. **핀 기능 선택:** `pwm-2chan`은 기본이 GPIO18/19(ALT5, I2S와 겹침)라서 `pin=12 func=4 pin2=13 func2=4`로 GPIO12/13 ALT0을 고름(overlay `__overrides__`). PWM은 핀이 ALT0일 때만 나감
3. **방향 GPIO:** libgpiod가 줄을 request해 출력으로 설정함. 해제 후에도 BCM GPIO는 마지막 방향·값을 유지하므로 원복은 재부팅으로 함

VEDA 기준(읽기 전용 `../VEDA_Final_Project/tank/src/tank_drive.cpp`): wiringPi `pwmSetMode(MS)`·`pwmSetClock(32)`·`pwmSetRange(1024)`, duty 0~255 → 0~1024(L114), 기본 200. VEDA 실제 주파수는 Pi 4 wiringPi 클럭 기준 확인 필요. 핀은 bringup 배치(L_EN GPIO12, R_EN GPIO13, L_IN1/2 GPIO5/6, R_IN1/2 GPIO16/26)를 씀.

## 진행 순서
부트 파일을 바꾸지 않음(런타임 overlay). 원복은 재부팅. GPIO12/13·5·6·16·26에는 아무것도 연결하지 않음(LED는 선택).

### 1. 사전 확인
```bash
# [Pi]
grep -n 'audio\|dtoverlay' /boot/firmware/config.txt
aplay -l
which pinctrl gpioset; gpioset --version | head -1
gpiodetect
pinctrl get 5,6,12,13,16,26
```

확인: **[붙여 주세요]** `dtparam=audio` 상태, 오디오 장치, 도구 유무·libgpiod 버전, `pinctrl-bcm2711` gpiochip 이름, 핀 6개 현재 기능

### 2. overlay와 드라이버
```bash
# [Pi]
sudo dtoverlay pwm-2chan pin=12 func=4 pin2=13 func2=4
dtoverlay -l
lsmod | grep pwm_bcm2835
for c in /sys/class/pwm/pwmchip*; do echo "$c $(cat $c/npwm) $(readlink -f $c/device)"; done
readlink /sys/bus/platform/devices/fe20c000.pwm/driver
pinctrl get 12,13
```

확인: **[붙여 주세요]** `npwm` 2, 장치 `fe20c000.pwm`, 드라이버 `bcm2835-pwm`, GPIO12/13 `a0`. `fe20c000` 경로가 다르면 `for` 출력의 장치 경로를 씀

### 3. sysfs 설정과 신호 레벨
```bash
# [Pi] 2단계 pwmchip 번호
P=/sys/class/pwm/pwmchip0
echo 0 | sudo tee $P/export; echo 1 | sudo tee $P/export
# ① 1kHz, duty 50%와 VEDA 200/255(784314ns → 39216단계 = 784320ns)
echo 1000000 | sudo tee $P/pwm0/period $P/pwm1/period
echo 500000  | sudo tee $P/pwm0/duty_cycle
echo 784314  | sudo tee $P/pwm1/duty_cycle
echo 1 | sudo tee $P/pwm0/enable $P/pwm1/enable
sudo cat /sys/kernel/debug/pwm
# ② 0%·100%로 핀 레벨 고정
echo 0       | sudo tee $P/pwm0/duty_cycle; pinctrl get 12     # 기대: lo
echo 1000000 | sudo tee $P/pwm0/duty_cycle; pinctrl get 12     # 기대: hi
# ③ 1초 주기, duty 0.5초: 레벨이 바뀌는지 여러 번 읽음
echo 0 | sudo tee $P/pwm0/duty_cycle
echo 1000000000 | sudo tee $P/pwm0/period
echo 500000000  | sudo tee $P/pwm0/duty_cycle
for i in 1 2 3 4 5 6; do pinctrl get 12; sleep 0.25; done
# ④ 너무 짧은 주기: 거부
echo 0 | sudo tee $P/pwm1/duty_cycle
echo 20 | sudo tee $P/pwm1/period                              # 1단계 < PERIOD_MIN 2, 기대: Invalid argument
```

확인: **[붙여 주세요]** debugfs requested 값, ②의 lo/hi, ③의 레벨 변화, ④의 에러
- ③은 duty를 0으로 먼저 내려야 period를 줄이거나 늘릴 때 `duty ≤ period` 검사에 걸리지 않음
- (선택) GPIO12(물리 32) → LED + 저항(330Ω~1kΩ) → GND를 달면 ③에서 1초 주기 깜빡임을 볼 수 있음

### 4. 방향 GPIO
```bash
# [Pi] 1단계 gpiodetect의 bcm2711 칩 이름(예: gpiochip0)
C=gpiochip0
for combo in "5=1 6=0 16=1 26=0" "5=0 6=1 16=0 26=1" "5=0 6=0 16=0 26=0"; do
  sudo gpioset -c $C -z $combo        # 정 / 역 / 정지
  sleep 0.5
  sudo cat /sys/kernel/debug/gpio | grep -E 'GPIO(5|6|16|26) '
  pinctrl get 5,6,16,26
  sudo pkill gpioset; sleep 0.2
done
pinctrl get 5,6,16,26                  # 해제 후 상태
```

확인: **[붙여 주세요]** 조합마다 debugfs의 `out hi/lo`와 `pinctrl` 값, 해제 후 상태
- libgpiod 1.x(`gpioset --version`이 v1)면 `-c`·`-z`가 없으므로 결과를 붙이고 멈춤. 명령을 바꿔 다시 적음

### 5. 정지와 원복
```bash
# [Pi] 3단계 P 그대로
echo 0 | sudo tee $P/pwm0/enable $P/pwm1/enable
pinctrl get 12,13                      # disable 뒤 레벨
echo 0 | sudo tee $P/unexport; echo 1 | sudo tee $P/unexport
sudo dtoverlay -r pwm-2chan
sudo modprobe -r pwm_bcm2835
lsmod | grep pwm_bcm2835 || echo none
sudo reboot
```

```bash
# [Pi] 재부팅 후
dtoverlay -l
pinctrl get 5,6,12,13,16,26
```

확인: **[붙여 주세요]** disable 뒤 레벨, 모듈 없음, 재부팅 후 overlay 없음과 핀 6개가 1단계와 같은지

## 완료 기준
- [ ] overlay 적용 후 pwmchip(`npwm` 2, 드라이버 `bcm2835-pwm`) 등록, GPIO12/13 ALT0
- [ ] sysfs 설정이 debugfs requested에 반영, 0%/100%에서 핀 lo/hi, 1초 주기에서 레벨 변화, 짧은 주기 거부
- [ ] 방향 GPIO 정·역·정지 조합이 출력 값으로 확인됨, 해제 후 상태 기록
- [ ] disable·원복 후 모듈·overlay 없음, 재부팅 후 핀 기능이 1단계와 같음

## 결과
대표 출력 몇 줄과 판단만 적는다. 원문 로그는 쌓지 않는다.

- 1 사전 확인:
- 2 등록:
- 3 PWM:
- 4 방향 GPIO:
- 5 원복:
- 미검증 항목: 모터 실구동 미검증
