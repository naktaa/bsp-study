# 현재 개발 환경

2026-11-05까지의 사용자 명령 출력·모듈 설명과 WSL 파일 읽기 확인 기준. 재현 명령은 [커널 문서](kernel-build.md)와 [모듈 문서](../examples/hello_module/kernel-module.md)에 둔다.

## WSL 호스트

| 항목 | 확인값 |
| --- | --- |
| PC / OS | Windows 노트북, WSL2 Ubuntu 22.04.5 LTS (jammy) |
| 아키텍처 / WSL 커널 | x86_64 / 6.18.33.2-microsoft-standard-WSL2 |
| 저장소 | `/home/nakta/bspworkspace/bsp-study` |
| 툴체인 | gcc·aarch64-linux-gnu-gcc 11.4.0, ARM64 binutils 2.38, Make 4.3 |
| 커널 소스 | `external/linux`, raspberrypi/linux rpi-6.18.y 계열 |
| 고정 커밋 | `b3495ea401d709af0701a85babcba5e3d788a602` (6.18.59), detached HEAD |
| 커널 설정·출력 | `out/kernel-rpi4`, bcm2711_defconfig + LOCALVERSION=-v8-bsp-study01, LOCALVERSION_AUTO=n, 각 make에서 LOCALVERSION= |
| 배포 파일 | `out/pi4-install`, `out/pi4-study01.tar.gz` (빌드·배포 산출물은 Git 제외) |
| 기존 부팅 백업 | `out/boot-backup-20261102-222418/firmware/`, 루트 파일시스템 전체 백업은 아님 |

## Pi 타깃

| 항목 | 확인값 |
| --- | --- |
| 보드 / OS | Raspberry Pi 4 Model B Rev 1.5 / Debian GNU/Linux 13 (trixie), 상세 버전 13.7 |
| 아키텍처 / 현재 커널 | aarch64 / **6.18.59-v8-bsp-study01**, 부팅·모듈 로드/해제 실측 |
| 기존 배포판 커널 | **6.18.50+rpt-rpi-v8**, 초기 환경. 기존 모듈 디렉터리와 kernel8.img·initramfs8 보존 |
| SSH | 현재 사용자 지정 별칭 `pi`로 접속 (`ssh pi`). 기존 확인 주소는 `nakta@192.168.45.224`, hostname=nakta. 별칭의 실제 설정·고정 IP 여부는 별도 확인하지 않음 |
| 루트 / 부트 | `/dev/mmcblk0p2` ext4 `/`, `/dev/mmcblk0p1` FAT `/boot/firmware` |
| 부팅 설정 | 실제 파일 `/boot/firmware/config.txt`. `/boot/config.txt`는 이동 안내. 2026-11-04 사용자 제공 전문 확인 ([활성 설정](kernel-build.md#7-부팅-선택검증)) |
| 새 부팅 파일 | kernel8-bsp-study01.img, bcm2711-rpi-4-b-bsp-study01.dtb, overlays-bsp-study01/, initramfs8-bsp-study01 존재 확인 |
| 화면 / overlay 매핑 | 매핑 누락으로 미출력 발생 후 기존 overlays의 overlay_map.dtb 복사·재부팅으로 사용자가 정상 출력 확인. 현재 매핑은 이 복구 파일이며 같은 소스 빌드 결과로 정상 재배포하는 절차는 아직 미실행 ([상세](kernel-build.md#ssh는-되지만-커스텀-커널-화면이-나오지-않음)) |
| libc / 로더 | glibc 2.41, `/lib/ld-linux-aarch64.so.1` 경로 확인 및 hello 실행 성공 |
| 복구 수단 | SD카드 리더기와 최초 부팅 백업 보유. 화면 장애 복구는 확인했으며 기존 커널로 되돌리는 시험은 미수행 |

2026-11-04 화면 복구 후 uname에서도 6.18.59-v8-bsp-study01을 확인했다. 최종 설정에는 커스텀 kernel·device_tree·overlay_prefix, 그 뒤 [all], auto_initramfs=1과 dtoverlay=vc4-kms-v3d가 있으며 os_prefix는 없다.

컴파일은 WSL에서 수행한다. Pi의 GCC 14.2.0·Make 4.4.1과 기존 커널용 헤더는 존재를 확인했지만 실습의 빌드 기준으로 사용하지 않는다. 커스텀 커널용 빌드 기준은 WSL의 out/kernel-rpi4다.

### SD 카드

병렬 트랙([AGENTS.md](../AGENTS.md#병렬-트랙))에서 트랙별로 SD 카드를 나눠 쓴다. 카드를 바꿔 끼울 때는 전원을 분리한다.

| 카드 | 트랙 | 내용 | 상태 |
| --- | --- | --- | --- |
| 기존 카드 | 센서 | Debian 13 + 커스텀 커널 6.18.59-v8-bsp-study01, 위 표의 부팅 파일 | 사용 중. 최초 부팅 백업은 위 WSL 표 |
| 부팅 트랙 카드 | 부팅 | U-Boot·Yocto 시험용 | 용량·준비 여부·초기 내용 확인 필요 |

## 모듈과 핀 연결표

배선은 이 절에서 한 번만 관리한다. **물리 핀 번호는 Pi 40핀 헤더 위치, BCM GPIO는 신호 번호**다. 두 번호를 섞지 않는다. 상태는 사용자 확인 완료·제안·확인 대기로 구분하며, 제안된 연결을 실제 연결로 기록하지 않는다. 배선 변경은 전원 분리 상태에서 수행한다.

### 보유 모듈

| 모듈 | 사용자 확인 | 현재 상태·남은 확인 |
| --- | --- | --- |
| GY-521 / MPU-6050 | 예전에 디바이스마트에서 구매한 저가형이며 VCC/GND/SCL/SDA/XDA/XCL/AD0/INT 표기가 앞선 예시와 같다고 확인 | 정확한 판매 상품·회로·레귤레이터·AD0 풀다운은 미확인. VCC/GND/SDA/SCL 배선 완료와 주소 0x68의 WHO_AM_I=0x68 응답 확인. AD0 외부 선 연결 상태는 별도 확인 필요 |

### GY-521 배선

사용자가 아래 전원·SDA/SCL 배선 완료를 보고했고, 주소 0x68에서 식별값을 읽었다. AD0 외부 선 상태는 응답값만으로 판단하지 않는다. 핀 순서가 아니라 기판의 신호 표기를 기준으로 연결한다.

| 모듈 핀 | Pi 물리 핀 | BCM GPIO / 기능 | 용도 | 상태 |
| --- | --- | --- | --- | --- |
| VCC | 1 | 3.3V 전원 | 우선 3.3V 공급 기준 | 사용자 연결 완료 확인 |
| GND | 6 | GND | 공통 접지 | 사용자 연결 완료 확인 |
| SDA | 3 | GPIO2 / SDA1 | I2C 데이터 | 사용자 연결 완료 확인 |
| SCL | 5 | GPIO3 / SCL1 | I2C 클럭 | 사용자 연결 완료 확인 |
| AD0 | 선택: 9 | GND | 외부 LOW 고정 시 주소 0x68 | 추가 연결 여부 미정. 보드 기본 상태 확인 필요 |
| INT | 11 | GPIO17 / IRQ 입력 | Data Ready, active HIGH·래치 | GPIO17 IRQ 실측 확인. 물리11 연결 설계이며 입력·풀다운 적용 |
| XDA / XCL | 연결하지 않음 | 미할당 | 보조 I2C | 현재 단계 미사용 계획 |

Pi GPIO는 3.3V 신호이고 5V tolerant가 아니다. 3.3V·GND 핀은 GPIO 데이터 핀이 아니다. 정확한 GY-521 변형의 전원 회로는 확인 전이므로 센서 응답이 없다고 임의로 5V로 올리지 않는다.

### Pi 핀 사용 현황

GY-521의 물리 핀 1·3·5·6 연결을 사용자 보고로 확인했다. 물리 핀 9의 AD0 연결 여부는 아직 확정하지 않았다.

| 물리 핀 | 기능 | 연결 대상 | 다른 모듈 추가 시 기준 |
| --- | --- | --- | --- |
| 1 | 3.3V | GY-521 VCC | 모듈 전압·소비 전류를 함께 확인 |
| 3 | GPIO2 / SDA1 | GY-521 SDA | 같은 I2C 버스 공유 가능, 주소·신호 전압 확인 |
| 5 | GPIO3 / SCL1 | GY-521 SCL | 같은 I2C 버스 공유 가능, 주소·신호 전압 확인 |
| 6 | GND | GY-521 GND | 공통 접지 |
| 9 | GND | AD0 LOW 고정 시 선택 | 현재 사용 확정 아님 |
| 11 | GPIO17 | MPU6050 INT 연결 후보 | Data Ready IRQ 동작 확인. 다른 모듈에 중복 할당하지 않음 |

새 모듈은 위 형식으로 모듈별 연결표와 Pi 핀 현황에 반영한다. 임의 GPIO를 중복 할당하지 않고 전원·GND 공유와 I2C 버스 공유를 구분한다. 사용하지 않은 핀도 자동으로 여유 핀이라 가정하지 말고 overlay·다른 기능의 점유를 확인한다.

### AD0와 I2C 주소

MPU-6050의 AD0가 LOW면 7비트 주소 0x68, HIGH면 0x69다. **외부 선을 연결하지 않음과 칩의 입력이 떠 있음은 다르다.** 모듈에 풀다운이 있으면 외부 AD0 선 없이도 LOW를 유지할 수 있다. 현재 저가형 기판의 풀다운 유무는 확인하지 않았으므로 미연결 상태를 무조건 0x68이라고 단정하지 않는다.

사용자는 과거에 AD0 외부 선 없이 사용한 경험이 있다. 당시 기본 전압이 어떻게 결정됐는지는 회로·측정 기록이 없어 확인할 수 없다. 앞선 GND 연결 제안은 주소를 LOW로 명시하려는 선택이지 모든 GY-521에서 외부 선이 필수라는 뜻은 아니다. 미연결로 사용할 경우 실제 주소 응답을 확인한 뒤 표에 반영한다.

### 현재 버스·후속 계획

커스텀 커널에서 i2c-1과 컨트롤러 fe804000.i2c의 연결은 sysfs 출력으로 확인했다. 사용자가 `/dev/i2c-1`과 i2ctransfer 설치를 확인했다. `1-0068/driver`·`1-0069/driver` 링크가 없는 상태에서 `sudo i2ctransfer -y 1 w1@0x68 0x75 r1`을 실행해 `0x68`을 받았다. 주소 0x68에서 WHO_AM_I 응답을 확인했다. 이후 새 학습 드라이버에서도 식별값 0x68·바인딩·overlay 제거와 모듈 해제를 사용자 출력으로 확인했다. 이후 raw 한 샘플과 misc 문자 인터페이스를 통한 앱 20회 폴링·재실행·g/deg/s 단위 출력·정상 종료도 사용자 로그로 확인했다. INT는 GPIO17(물리11) 경로의 Data Ready IRQ 증가·해제를 사용자 출력으로 확인했으며 측정 정확도·실제 수집률 검증은 남아 있다. [실습·재현](../examples/mpu6050/mpu6050.md).

기존 프로젝트의 I2C bus 1·주소 0x68·GPIO line offset 17·FIFO·500Hz 목표는 비교 기준이다. GPIO17을 현재 INT 배선으로 확정하거나 500Hz가 실제 측정됐다고 기록하지 않는다. IRQ 핀·전원/I/O 전압·AD0 상태·센서 식별값은 해당 단계에서 실물과 함께 확인한다.

근거: [Pi 공식 GPIO 핀 배치·신호 전압](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#gpio-pinout-map), [MPU-6000/6050 제조사 사양서 9.2절](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf). 다른 판매사의 모듈 회로를 현재 기판의 회로로 간주하지 않는다.

## 참고 자료

| 출처 | 확인 범위 |
| --- | --- |
| `../VEDA`, c2f2ace95af528c6f806c8fc273f02bdb10120c6 | 커널프로그래밍 PDF 10·29~31·37쪽, kmodule C·Makefile, gpiobtn C 확인. 다른 PDF 본문 전체는 미검토 |
| `../VEDA_Final_Project`, 8d0ddbe579c41886f6b9b0b50eafb30e64128633 | tank/src/imu_reader.cpp, imu_reader.hpp, config_template.ini의 imu, app_config.hpp의 ImuConfig, HYBRID_EIS_FRAMEWORK의 IMU 부분 확인 |

자료별 적용 차이는 해당 실습 문서에서 설명한다. 원본 저장소는 읽기 전용이다.

GPIO 조회 실측: libgpiod 2.2.1, gpiochip0=pinctrl-bcm2711(58 lines), gpiochip1=raspberrypi-exp-gpio(8 lines). GPIO17은 input, pinctrl은 pd/lo이며 gpioinfo에 consumer가 표시되지 않았다. 이는 IRQ 적용 전의 상태다. 이후 GPIO17 Level IRQ 횟수 증가·앱 공존·IRQ 해제를 별도로 확인했다.
