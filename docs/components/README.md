# 컴포넌트 문서

장치·부트·빌드 구성요소마다 **지금 어떻게 돌아가는지 → 정상인지 어떻게 보는지 → 어디를 고치는지 → 문제가 생기면 어떻게 조치하는지**를 적은 운영 문서다. 왜 이렇게 만들었는지는 [학습 문서](../learning/00-overview.md)에 있다.

## 공통 점검 순서
증상이 생기면 아래 순서로 위에서부터 본다. 각 컴포넌트 문서 6장 표의 "계층" 열은 이 번호를 쓴다.

| 순서 | 계층 | 먼저 볼 것 |
| --- | --- | --- |
| 1 | 빌드 | 빌드 로그, `modinfo <모듈> \| grep vermagic`와 `uname -r`, Yocto `tmp/work/.../temp/log.do_*` |
| 2 | 부팅 (firmware·`config.txt`·U-Boot) | 부트 파티션 파일, `config.txt`의 `dtoverlay` 줄, `/proc/device-tree/chosen` |
| 3 | DT·overlay | `/proc/device-tree/` 아래 노드, `status`, `compatible` |
| 4 | 모듈 적재 | `lsmod`, `modinfo`, `dmesg` |
| 5 | 매칭·probe | `/sys/bus/*/devices/*/driver` 링크, probe 오류 로그 |
| 6 | 버스·IRQ | `i2cdetect -y <bus>`(`UU`면 드라이버가 잡음), `/proc/interrupts` |
| 7 | 서브시스템 | IIO sysfs, `media-ctl -p`, `arecord -l`, `/sys/class/pwm` |
| 8 | 사용자 공간 | 앱 출력, `devcheck` |

- 아래 계층이 정상이어야 위 계층을 볼 의미가 있다. 예를 들어 4에서 모듈이 없으면 7의 sysfs가 비어 있는 것은 원인이 아니라 결과다.
- 실행 위치는 명령 블록 첫 줄에 `# [WSL]`·`# [Pi]`(64GB 카드)·`# [Yocto]`(Yocto 카드)로 적는다.

## 한 번에 확인: devcheck
[devcheck.c](../../examples/devcheck/devcheck.c)는 6개 장치를 sysfs·IIO·ALSA·PWM·rpicam으로 읽어 PASS/FAIL을 낸다. 장치 번호를 고정하지 않고 이름으로 찾으므로 두 카드에서 같은 명령을 쓴다.

```bash
# [Yocto] root — 이미지에 설치됨
devcheck all                 # 6개 PASS, 종료 코드 0
devcheck imu                 # 하나만: imu|pca|cam|mic|sonar|pwm
# [Pi] 64GB 카드 — WSL에서 scp examples/devcheck/devcheck pi:bsp/apps/
sudo ~/bsp/apps/devcheck all
```

FAIL이 난 장치의 컴포넌트 문서 4장으로 가서 계층별 확인을 이어 간다.

## 컴포넌트 목록
| 문서 | 대상 | 직접 작성 | 학습 문서 |
| --- | --- | --- | --- |
| [kernel](kernel.md) | 6.18 커널 빌드·설치·복구, config fragment, 외부 모듈 | 빌드 절차·fragment | [07](../learning/07-kernel-build.md), [02](../learning/02-kernel-module.md) |
| [uboot](uboot.md) | U-Boot·`boot.scr`, 64GB 수동 / Yocto 두 경로 | 부트 스크립트 | [11](../learning/11-uboot-boot.md) |
| [yocto](yocto.md) | `meta-bsp-study` 레이어·이미지·모듈 레시피·Wi-Fi | 레이어 전체 | [12](../learning/12-yocto-minimal.md), [18](../learning/18-integration.md) |
| [imu](imu.md) | MPU6050 | misc·IIO 드라이버 | [06](../learning/06-mpu6050.md), [09](../learning/09-iio-direct.md) |
| [camera](camera.md) | IMX219 + unicam + libcamera | 센서 드라이버 `imx219_study`, V4L2 앱 | [13](../learning/13-camera.md) |
| [mic](mic.md) | INMP441 | 코덱 드라이버, overlay, 캡처 앱 | [14](../learning/14-mic.md) |
| [pwm](pwm.md) | PCA9685, 모터 PWM | 비교용 provider | [15](../learning/15-pca9685.md), [18](../learning/18-integration.md) |
| [ultrasonic](ultrasonic.md) | HC-SR04 | overlay | [16](../learning/16-ultrasonic.md) |

새 문서는 [_template.md](_template.md) 형식으로 쓴다.
