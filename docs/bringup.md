# 환경과 참고 자료

미확인 값은 추측하지 않고 실제 명령 출력·배선 확인 후 채운다.

## 개발 호스트
2026-11-02 사용자 설명과 WSL 명령 출력 기준.
| 항목 | 확인값 |
| --- | --- |
| OS / 배포판 / 아키텍처 | Windows 노트북 + WSL2 / Ubuntu 22.04.5 LTS (jammy) / x86_64. WSL 커널 `6.18.33.2-microsoft-standard-WSL2` (사용자 제공 출력) |
| 컴파일러 / 버전 / 타깃 | `/usr/bin/gcc` 및 `/usr/bin/aarch64-linux-gnu-gcc`: GCC 11.4.0 (`Ubuntu 11.4.0-1ubuntu1~22.04.3`). 크로스 타깃 `aarch64-linux-gnu` (사용자 제공 출력). hello-arm64의 보드 실행 확인. 모듈 빌드 호환성은 미검증 |
| bsp-study 경로 | `/home/nakta/bspworkspace/bsp-study` |
| VEDA 경로 / 참고 커밋 | `../VEDA` / `c2f2ace95af528c6f806c8fc273f02bdb10120c6` |
| VEDA_Final_Project 경로 / 참고 커밋 | `../VEDA_Final_Project` / `8d0ddbe579c41886f6b9b0b50eafb30e64128633` |
| 커널 소스 경로 / 커밋 / 빌드 출력 경로 | 소스: `external/linux` (기존 gitignore 제외). 공식 rpi-6.18.y, 다운로드 HEAD=`b3495ea401d709af0701a85babcba5e3d788a602` (6.18.59), 사용자 출력과 로컬 소스 확인. detached HEAD 고정 및 `out/kernel-rpi4/.config` 기본 설정 생성 확인. 출력: `out/kernel-rpi4`. LOCALVERSION=-v8-bsp-study01, prepare 완료 및 kernelrelease=6.18.59-v8-bsp-study01 확인. Image·modules·dtbs 빌드 종료 상태 0 및 로컬 산출물 확인. 사용자 출력으로 Pi의 새 부팅 파일·버전별 모듈 디렉터리와 새 initramfs 내용 확인. 사용자 제공 uname 출력으로 6.18.59-v8-bsp-study01 ARM64 부팅 확인 |

## 타깃 보드
2026-11-02 사용자가 보드에서 실행해 전달한 출력 기준. 모델 출력 끝에 줄바꿈이 없어 모델과 커널 문자열이 붙어 있음.
| 항목 | 확인값 |
| --- | --- |
| 보드 모델 | Raspberry Pi 4 Model B Rev 1.5 (사용자 제공 출력) |
| OS / 아키텍처 / 커널 버전 | Debian GNU/Linux 13 (trixie), DEBIAN_VERSION_FULL=13.7 / aarch64 / 6.18.50+rpt-rpi-v8 (사용자 제공 출력) |
| 부트 파티션 / 설정 파일 위치 | `/dev/mmcblk0p1` FAT, `/boot/firmware`; 실제 설정 `config.txt`, `auto_initramfs=1`, `arm_64bit=1`. 루트는 `/dev/mmcblk0p2` ext4. 사용자 제공 출력 기준 |
| SSH 접속 별칭 | `nakta@192.168.45.224` (사용자 제시 주소, 이어서 보드 hostname=nakta 출력 제공). 고정 IP 여부는 미확인 |
| 보드 빌드 도구 | GCC 14.2.0 (Debian 14.2.0-19), GNU Make 4.4.1 (aarch64-unknown-linux-gnu), 사용자 제공 출력 |
| 보드 libc / 동적 로더 | glibc 2.41 (`getconf GNU_LIBC_VERSION`). `/lib/ld-linux-aarch64.so.1 -> aarch64-linux-gnu/ld-linux-aarch64.so.1` 링크 확인. hello-arm64의 보드 실행과 종료 상태 0 확인 (사용자 제공 출력) |
| 센서 모델 / 보유 여부 | 센서 보유, 보드에는 미연결 (사용자 확인). 기존 프로젝트는 MPU-6050 기준임을 설정·코드·문서에서 확인. 현재 실물의 모듈 표기·전원 조건은 미확인 |
| 데이터시트 출처 / 개정 / 센서 식별값 | 미확인 |
| I2C 버스 / 주소 / 주소 선택 핀 | `/dev/i2c-1`, `/dev/i2c-20`, `/dev/i2c-21` 존재 (사용자 제공 출력). 프로젝트 기본값 bus=1, addr=104 (`0x68`). 센서는 미연결이며 실제 센서 주소·주소 선택 핀은 미확인 |
| SDA / SCL / INT / 전원 / GND 배선 | 미연결. 연결 핀·전원 조건 확인 전 |
| GPIO 번호 체계 | 프로젝트 기본값 `/dev/gpiochip0`, line offset 17. 현재 배선은 미연결이며 BCM·물리 핀·기존 wiringPi 번호 구분 필요 |
| 카메라 모델 / 연결 상태 | 미확인, 초기 단계 필수 아님 |
| 센서 모듈 전원·I/O 전압 조건 / 근거 | 미확인 |
| 실행 커널용 헤더·설정·심볼 정보 확보 방법 | `/lib/modules/6.18.50+rpt-rpi-v8` 존재. `build -> ../../../src/linux-headers-6.18.50+rpt-rpi-v8` 링크 확인. 사용자 제공 `ls -ldL`로 링크 대상 존재 확인, `include/config/kernel.release`는 실행 커널과 동일한 `6.18.50+rpt-rpi-v8`. `build/Makefile`과 `build/Module.symvers` 파일 존재 확인. 전체 빌드 환경의 충분성은 실제 모듈 빌드로 검증 예정 |
| 기존 센서 드라이버 / 바인딩 상태 / 전환 절차 | 미확인 |
| INT 극성·해제 조건 / FIFO 형식 | 미확인, 데이터시트와 실측 대조 필요 |
| 부팅 백업 / 복구 방법 | 부트 파티션 복사본: `out/boot-backup-20261102-222418/firmware/` 사용자 출력·로컬 확인으로 config.txt·cmdline.txt·kernel8.img·initramfs8 존재 확인. 카드 리더기 사용 가능 (사용자 확인). 기존 파일 보존 설치 후 실패 시 FAT 파티션의 config.txt를 백업본으로 복원. 설치·커스텀 커널 부팅 확인. 실제 복구 시험은 미수행 |

## 첫 확인 명령
보드에서 실행한다. 장치 경로가 없다는 결과도 그대로 기록한다.

```bash
cat /proc/device-tree/model
uname -a
uname -m
cat /etc/os-release
ls -l /lib/modules
ls -l /dev/i2c-*
```

호스트에서 실행한다. 컴파일러가 없으면 그 사실부터 기록한다.

```bash
uname -m
cat /etc/os-release
command -v aarch64-linux-gnu-gcc
aarch64-linux-gnu-gcc --version
```

## 참고 자료 대조
2026-11-02 로컬 경로·커밋과 아래 일부 소스를 확인함. PDF는 파일 위치만 확인했으며 본문은 아직 검토하지 않음. 참고 소스 자체의 보드 동작은 미검증.

| 출처 / 경로 | 확인 범위와 목적 | 적용 판단 |
| --- | --- | --- |
| VEDA / 리눅스커널/수업코드/kmodule/{Makefile,hello_module.c} | 모듈 init/exit와 빌드 설정 확인 | 교육 실습에 사용. 고정 KDIR·타깃 설정은 실제 환경에 맞게 변경하고 단독 `i` 문법 오류 수정 필요 |
| VEDA / 리눅스커널/수업코드/gpiobtn/gpiobtn.c | 문자 디바이스·GPIO IRQ 흐름 확인 | 교육 실습에 사용. 고정 GPIO 번호·자원 및 오류 처리 방식은 그대로 복사하지 않고 대상 커널과 대조 |
| VEDA / 리눅스커널/강의자료, 임베디드리눅스/강의자료, 리눅스디버깅/강의자료 | 모듈·커널·Yocto·디버깅 PDF 위치 확인 | 본문 미검토, 필요한 단계에서 읽기 |
| VEDA_Final_Project / tank/src/imu_reader.cpp | 사용자 공간 I2C·FIFO·GPIO IRQ, 단위·축·시간 처리 확인 | 기존 방식은 비교 기준으로 유지. 커널 수집 구현은 별도로 검증 |
| VEDA_Final_Project / tank/include/imu_reader.hpp | ImuReader 인터페이스 확인 | EIS 선택 과제 진행 시 재검토 |
| VEDA_Final_Project / tank/config/config_template.ini / [imu] 및 tank/include/app_config.hpp / ImuConfig | MPU-6050, bus=1, addr=0x68, IRQ line offset=17, target_hz=500, FIFO 활성 기본값 확인 | 비교용 참고값. 현재 배선·실제 수집률과 구분 |
| VEDA_Final_Project / tank/docs/HYBRID_EIS_FRAMEWORK.md | MPU-6050 명시와 IMU 수집 구조 부분 확인 | IRQ 설명의 wiringPi 경로는 현재 소스와 차이 있음. 현재 구현은 소스와 대조 |

센서 데이터시트, 대상 커널의 드라이버·DT binding·API는 환경 확정 후 출처·버전과 함께 추가한다. 과거 예제의 API를 현재 환경에 적용할 수 있다고 가정하지 않는다.

비교 기록은 필요할 때 아래 형식으로 한 항목씩 추가한다.
- 주제 / 참고 파일·커밋:
- 기존 방식의 목적:
- 판단: 유지 / 교육 실습에만 사용 / 변경
- 이유 / 대상 커널 근거:

### 커널 빌드 수업 자료 대조
- 확인: `VEDA/리눅스커널/강의자료/사물인터넷12장_커널프로그래밍.pdf` 10·29~31·37쪽 본문.
- 기존 방식: rpi-6.12.y 소스 다운로드, /usr/src 이동, ARM64 기본 설정 후 Image·modules·dtbs 크로스 빌드, SD에 설치, 해당 소스로 모듈 빌드.
- 적용: 같은 흐름 유지. 현재 Pi의 6.18 계열·Pi 4 ARM64 설정으로 대조하고, 소스는 sudo 없이 관리하는 사용자 경로에 둔다. 소스 커밋 고정과 별도 출력 디렉터리는 재현을 위해 추가. 설치는 SSH 전송 후 Pi에서 파일 배치·새 initramfs 생성. 기존 파일을 보존하고 kernel·device_tree·overlay_prefix로 별도 이름의 파일을 선택.

### 확인된 커널 빌드 명령
WSL의 bsp-study 루트에서 실행. 사용자 제공 종료 상태 0과 로컬 산출물 확인.

```bash
make -C external/linux O="$PWD/out/kernel-rpi4" \
    ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= \
    -j4 Image modules dtbs
```

### 커스텀 부팅 선택
사용자가 적용한 Pi 4 선택 파일명: kernel8-bsp-study01.img, bcm2711-rpi-4-b-bsp-study01.dtb, overlays-bsp-study01/. 기존 auto_initramfs=1에 따라 대응하는 initramfs8-bsp-study01을 준비. [pi4] 블록 뒤 [all]로 기존 공통 설정 범위 유지하도록 안내. 부팅은 사용자 uname 결과로 확인했으며 최종 config.txt 전문은 재수신하지 않음.
