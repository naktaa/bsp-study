# 현재 개발 환경

2026-11-04까지의 사용자 명령 출력과 WSL 파일 읽기 확인 기준. 재현 명령은 [커널 문서](kernel-build.md)와 [모듈 문서](../examples/hello_module/README.md)에 둔다.

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
| SSH | `nakta@192.168.45.224`, hostname=nakta. 고정 IP 여부 미확인 |
| 루트 / 부트 | `/dev/mmcblk0p2` ext4 `/`, `/dev/mmcblk0p1` FAT `/boot/firmware` |
| 부팅 설정 | 실제 파일 `/boot/firmware/config.txt`. `/boot/config.txt`는 이동 안내. 2026-11-04 사용자 제공 전문 확인 ([활성 설정](kernel-build.md#7-부팅-선택검증)) |
| 새 부팅 파일 | kernel8-bsp-study01.img, bcm2711-rpi-4-b-bsp-study01.dtb, overlays-bsp-study01/, initramfs8-bsp-study01 존재 확인 |
| 화면 / overlay 매핑 | 매핑 누락으로 미출력 발생 후 기존 overlays의 overlay_map.dtb 복사·재부팅으로 사용자가 정상 출력 확인. 현재 매핑은 이 복구 파일이며 같은 소스 빌드 결과로 정상 재배포하는 절차는 아직 미실행 ([상세](kernel-build.md#ssh는-되지만-커스텀-커널-화면이-나오지-않음)) |
| libc / 로더 | glibc 2.41, `/lib/ld-linux-aarch64.so.1` 경로 확인 및 hello 실행 성공 |
| 복구 수단 | SD카드 리더기와 최초 부팅 백업 보유. 화면 장애 복구는 확인했으며 기존 커널로 되돌리는 시험은 미수행 |

2026-11-04 화면 복구 후 uname에서도 6.18.59-v8-bsp-study01을 확인했다. 최종 설정에는 커스텀 kernel·device_tree·overlay_prefix, 그 뒤 [all], auto_initramfs=1과 dtoverlay=vc4-kms-v3d가 있으며 os_prefix는 없다.

컴파일은 WSL에서 수행한다. Pi의 GCC 14.2.0·Make 4.4.1과 기존 커널용 헤더는 존재를 확인했지만 실습의 빌드 기준으로 사용하지 않는다. 커스텀 커널용 빌드 기준은 WSL의 out/kernel-rpi4다.

## 센서·배선 미확인 항목

센서는 보유하나 미연결이다. 프로젝트는 MPU-6050, I2C bus=1·addr=0x68, GPIO line offset=17, FIFO·500Hz 목표를 기본값으로 사용한다. 이는 현재 실물·배선·실제 수집률 확인값이 아니다. 기존 커널에서 `/dev/i2c-1`, 20, 21은 관측했으나 새 커널에서의 센서 인식은 미검증이다.
실물 모듈 표기·데이터시트·전원/I/O 전압·주소 선택 핀·SDA/SCL/INT/GND·GPIO 번호 체계는 센서 단계 전에 확인한다.

## 참고 자료

| 출처 | 확인 범위 |
| --- | --- |
| `../VEDA`, c2f2ace95af528c6f806c8fc273f02bdb10120c6 | 커널프로그래밍 PDF 10·29~31·37쪽, kmodule C·Makefile, gpiobtn C 확인. 다른 PDF 본문 전체는 미검토 |
| `../VEDA_Final_Project`, 8d0ddbe579c41886f6b9b0b50eafb30e64128633 | tank/src/imu_reader.cpp, imu_reader.hpp, config_template.ini의 imu, app_config.hpp의 ImuConfig, HYBRID_EIS_FRAMEWORK의 IMU 부분 확인 |

자료별 적용 차이는 해당 실습 문서에서 설명한다. 원본 저장소는 읽기 전용이다.
