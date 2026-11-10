# 학습 순서

BSP 구성과 저수준 드라이버 구현·데이터 수집을 필수 목표로 한다. 커널·모듈·문자 디바이스·Device Tree와 MPU6050 I2C·IRQ·FIFO 실습을 거쳐 IIO IRQ 타임스탬프·기본 수신·정상 해제까지 확인했다. 장기 주기 안정성·측정 기준선과 오류/경쟁 경로는 후속 범위다. 단계별 상태와 검증 근거는 [worklog](worklog.md)와 연결된 학습 문서에서 관리한다. 필수 과정 이후 [2부](#2부-부팅-체계와-장치-확장)에서 U-Boot·Yocto 부팅 체계를 먼저 갖추고 VEDA Final 장치를 하나씩 추가한다.
구현·수정 전 목적·개념·파일과 범위·검증 방법을 설명하고, 결과를 사용자와 함께 해석한다.
각 단계는 동작 근거와 함께 코드·설정의 역할, 실패 시 확인할 계층을 설명할 수 있을 때 완료한다. 실습 코드·설정은 Codex가 Codex 지시서 범위 안에서 작성하고 외부 모듈·DTBO 빌드까지 확인한다. 전체 커널·Yocto 이미지 빌드, SD 카드 작업과 보드 배포·실측은 사용자가 하고, Claude Code는 계획·리뷰·문서를 맡는다. 모든 수정은 사용자 승인 후 진행한다([AGENTS.md 역할](../AGENTS.md#역할)).

## 필수 과정

| 단계 | 실습 | 상태 | 완료 기준 |
| --- | --- | --- | --- |
| 0 | 환경·배선·참고 자료 확인 | 완료 | 호스트·보드·커널·센서 상태, 데이터시트와 참고 경로 기록 |
| 1 | 사용자 프로그램 빌드·배포 | 완료 | 타깃용 C 프로그램 빌드·전송·보드 실행 및 실행 형식 확인 |
| 2 | 커널 모듈 기초 | 완료 | 대상 커널에 맞춰 빌드·로드·해제하고 로그·버전 관계 설명 |
| 3 | 사용자/커널 경계 | 완료 | 작은 문자 디바이스와 앱으로 데이터 전달, 잘못된 입력 처리, 반복 로드·해제 확인 |
| 4 | I2C 레지스터와 센서 기준선 | 부분 (회전 로그 미확보) | 데이터시트에 근거한 식별·설정·값 변환, 정지·회전 로그 확보 |
| 5 | Device Tree와 장치 매칭 | 완료 | 노드·배선·드라이버 관계 설명, 매칭과 probe/remove 관측 |
| 6 | 최소 I2C 센서 드라이버 | 완료 | 커널에서 센서 식별·초기화·읽기 구현, 사용자 전달·오류 처리·자원 정리 확인 |
| 7 | 커널 빌드·부팅·복구 | 완료 (기존 커널 복귀 시험 미수행) | 고정한 소스·설정의 커널 부팅, 모듈·SSH·기존 화면 및 영향 기능 확인, 복구 절차와 미검증 범위 기록 |
| 8 | 인터럽트·FIFO 수집 | 완료 | 폴링과 비교해 IRQ·FIFO 경로 구현, 데이터 수집과 중단·해제 동작 확인 |
| 9 | IIO와 버퍼 수집 | 완료 | 기존 드라이버 분석, 직접 작성한 드라이버의 최소 IIO 채널·버퍼 구현 및 수집 앱 확인 |
| 10 | 측정·디버깅·재현 | 진행 (60초 정지 기준선 확보, 회전 검증 남음) | 단위·축·수집률·시간 검증, 실제 문제의 분석 근거와 재현 절차 확보 |

순서는 환경에 따라 조정한다. 센서가 없으면 4~6단계를 보류하고 7단계를 먼저 할 수 있다.
현재 선택한 경로는 1 → 7 → 2 순서다. 7단계에서 WSL 크로스 빌드로 커널을 만들고 Pi에서 부팅 확인한 뒤, 같은 커널 빌드 디렉터리로 모듈 실습을 진행한다. 번호는 기존 항목 식별용으로 유지한다.
5~6단계는 장치 매칭과 데이터 읽기를 작은 작업으로 나눠 왕복한다. 각 단계의 상세 구현은 대상 커널과 센서를 확인한 뒤 계획한다.

## 0. 환경과 자료
- 개발 호스트의 실제 OS·아키텍처·툴체인을 확인한다. WSL 여부도 추측하지 않는다.
- 보드의 model, uname, os-release, 모듈 디렉터리, I2C 장치 유무를 확인한다.
- 센서 모델·보유 여부·전원 조건·배선·INT 연결 상태와 데이터시트 출처를 확인한다. 주소나 GPIO를 추측하지 않는다.
- VEDA의 관련 자료와 기존 프로젝트의 센서 → IMU 수집 코드 → 소비자 흐름을 짧게 살핀다. 전체 앱 실행은 필수가 아니다.
- 설명: 호스트와 타깃, 커널과 rootfs, SSH와 크로스 컴파일의 역할.

## 1. 작은 C 프로그램부터
- 설치된 툴체인과 타깃 아키텍처 확인 후 작은 프로그램을 빌드하고 `file`로 확인한다.
- 동적 링크라면 타깃 libc·로더 호환성을 확인하고 SCP/SSH로 실행한다.
- [기존 hello 실습](../examples/hello/cross-compile.md) 완료. Make의 타깃·레시피·변수·증분 빌드는 [모듈 실습](../examples/hello_module/kernel-module.md)에서 확인했으며 일반 앱의 헤더 의존성·링크 규칙은 후속 앱 실습에서 보완한다.

## 2. 모듈 기초
- 완료 근거와 재현: [hello 모듈 실습](../examples/hello_module/kernel-module.md).
- VEDA의 init/exit 예제를 읽고 문법·빌드 경로를 검토해 최소 모듈을 작성한다.
- 실행 커널과 헤더·설정·생성 파일·필요 시 심볼 버전 정보의 일치를 확인한다.
- built-in/module, insmod/modprobe, 의존성, vermagic, dmesg를 실제 출력과 연결한다.
- 빌드 실패와 로드 실패를 구분해 원인을 확인한다.
- 모듈 Makefile을 함께 작성한다. all/clean 타깃, 탭으로 시작하는 레시피, KDIR의 기본값과 재정의, $(MAKE) 재귀 호출, .PHONY, KERNELRELEASE 조건 분기와 obj-m의 역할을 설명한다.
- 첫 빌드 후 소스 수정 없이 다시 make했을 때의 출력과 make clean 후 재빌드를 비교해 증분 빌드와 정리를 확인한다.

## 3. 사용자/커널 경계
- 작은 문자 디바이스로 open/read/write/release, 사용자 포인터와 커널 메모리의 차이를 익힌다.
- 사용자 복사, 반환값·길이·오프셋·잘못된 입력 처리와 자원 정리를 작은 범위에서 실습한다.
- 센서와 무관한 단순 데이터부터 전달한다. 이 실습용 인터페이스를 최종 센서 ABI로 고정하지 않는다.

## 4. I2C 레지스터와 기준선
- 데이터시트의 주소·식별값·전원/리셋·샘플링·측정 범위·데이터 형식을 확인한다.
- 기존 `tank/src/imu_reader.cpp`를 비교하며 OpenCV·RTSP·MQTT 없는 작은 사용자 공간 수집기를 작성한다.
- 레지스터 지정과 읽기 전송 방식, 바이트 순서, 부호 확장, raw 값과 물리 단위 변환을 설명한다.
- 정지·각 축 회전 조건에서 값·축·주기·시간을 기록한다. 이는 커널 드라이버 실습 전 비교 기준이다.
- 장치 바인딩을 확인하고 커널 드라이버와 사용자 직접 접근이 겹치지 않도록 한다.

## 5. Device Tree와 장치 매칭
- 대상 커널의 I2C 센서 드라이버와 DT binding을 읽는다.
- compatible, reg와 실제 버스·주소의 관계를 확인하고 필요한 최소 노드를 구성한다.
- 장치 등록 → 드라이버 매칭 → probe/remove를 sysfs·로그와 대조한다.
- 학습용 드라이버와 기존 드라이버의 바인딩 전환을 명시한다. 기존 compatible을 임의로 가로채거나 가짜 식별자를 운영 설정으로 남기지 않는다.

## 6. 최소 I2C 센서 드라이버
- 센서 식별값 읽기 → 초기화 → 한 샘플 읽기를 순서대로 구현한다. 전체 드라이버를 한 번에 작성하지 않는다.
- 대상 커널의 I2C 접근 API와 기존 드라이버를 대조하고 직접 전송·regmap의 역할을 이해한다.
- 3단계에서 익힌 작은 인터페이스로 값을 전달하고 4단계의 조건과 맞춰 비교한다. 인터페이스는 9단계에서 IIO로 정리한다.
- 전송 실패·식별 불일치·부분 초기화 실패에 대한 반환과 해제 경로를 검토한다. 보드에서 안전하게 재현 가능한 실패부터 검증한다.
- 반복 로드·해제와 앱 종료를 확인한다. 센서가 없는 상황을 성공값으로 숨기지 않는다.

## 7. 커널 빌드와 부팅
- 완료 근거와 재현: [커널 빌드·설치 문서](kernel-build.md). 부팅·모듈 동작·화면 장애 복구 확인. 기존 커널 복귀 절차는 준비했으나 복귀 시험은 미수행.
- 기준 수업 자료: VEDA/리눅스커널/강의자료/사물인터넷12장_커널프로그래밍.pdf 10쪽(소스), 29쪽(설정·크로스 빌드), 30~31쪽(설치).
- 소스 다운로드 → 설정 → 크로스 빌드 → 설치 → 부팅 확인을 나눠 진행한다. 단계마다 필요한 개념과 명령만 다루며 모듈 Makefile은 커널 빌드 환경 확정 후 수정한다.
- 커널·모듈 컴파일은 WSL에서 수행한다. 필요하면 새 커널용 initramfs 생성 등 설치 작업은 Pi에서 수행한다. os_prefix는 필수가 아닌 설치 선택지다.
- 공식 Raspberry Pi 커널 소스에서 대상에 맞는 커밋·설정을 고정한다. 무조건 최신 브랜치를 고르지 않는다.
- Image, modules, DTB/DTBO와 overlay 매핑 등 보조 파일의 역할·설치 위치를 실제 소스·빌드 목록과 OS 기준으로 확인한다.
- 기존 부팅 파일·설정 백업과 복구 경로를 확보한 뒤 변경한다.
- 빌드·설치·부팅·기능 성공을 구분한다. 구분 가능한 버전으로 부팅하고 uname·SSH·모듈·기존 화면과 영향 기능을 확인한다. 장애는 로그로 좁혀 단계적으로 검증하고, 해결 후 정상 배포 명령도 보완한다. 미검증 항목은 남긴다.
- 전체 소스 대신 출처·커밋·설정·패치·명령을 저장한다.

## 8. 인터럽트와 FIFO
- 데이터시트의 INT 동작·극성·해제 조건·FIFO 형식과 실제 배선을 확인한다.
- DT IRQ 설정과 핸들러를 연결하고 인터럽트 문맥과 sleep 가능한 작업의 차이를 설명한다.
- 폴링부터 비교한 뒤 IRQ 기반 읽기와 FIFO 처리로 확장한다. 부분 데이터·overflow·수집 중단을 다룬다.
- IRQ 카운트·수집 로그·오류를 함께 관측한다. 해제 시 IRQ·작업·버퍼 수명과 경쟁 가능성을 확인한다.

## 9. IIO와 버퍼
- 대상 커널의 기존 센서 드라이버에서 채널·scale·트리거·버퍼·타임스탬프 경로를 추적한다.
- 직접 작성한 드라이버에 필요한 최소 IIO 채널과 단일 값 읽기를 추가한 뒤 버퍼 수집으로 확장한다. 기존 드라이버 전체 기능 복제는 목표가 아니다.
- [IIO 예제](../examples/mpu6050_iio/iio-direct.md)는 6축 raw → scale → GPIO17 트리거 버퍼 → IRQ 타임스탬프 순서로 확장했다. 현재 24바이트 scan 20개 수신·시각 증가·약10ms 간격·앱 종료0·버퍼 중지 후 raw 재개·정상 해제를 확인했다. 기존 FIFO·misc 예제는 비교 기준으로 보존하며 IIO 센서 FIFO 재연결은 현재 범위에서 제외한다. 장기 기준선 등 남은 검증은 10단계에서 다룬다.
- scan_elements의 index/type과 활성 채널을 기준으로 크기·엔디언·정렬·시간을 해석하는 작은 수집 앱을 만든다. 앱이 여러 파일로 커지면 CMake의 add_executable, 타깃별 옵션·의존성과 별도 빌드 디렉터리를 함께 실습한다. 커널 모듈 빌드는 Kbuild를 사용한다.
- 학습용 드라이버와 기존 드라이버를 번갈아 검증하고 설정·출력 차이를 설명한다. 2026-11-10 같은 센서를 mainline `inv_mpu6050`(rpi `i2c-sensor` overlay, 부팅 시 적용)에 연결해 바인딩·sysfs·scan 배치·GPIO17 Edge IRQ·`iio_generic_buffer` 6축+timestamp 수신을 확인하고 차이를 정리했다([IIO 문서 2-2](../examples/mpu6050_iio/iio-direct.md#2-2-mainline-inv_mpu6050와의-차이)). 완료.

## 10. 측정과 문제 분석
- 정지 상태 60초 수집부터 시작하고 목표 주기는 장치 지원값 확인 후 정한다. 2026-11-10 학습용 IIO 드라이버(100Hz)로 60초 정지 기준선을 확보했다([IIO 문서 6장](../examples/mpu6050_iio/iio-direct.md#6-트러블슈팅과-한계)). 축별 회전 검증과 문제 분석 정리는 남아 있다.
- 단위·축·bias·timestamp clock과 수집 조건을 맞춰 기준선 및 기존 드라이버와 비교한다. 2026-11-10 mainline 5샘플은 기준선과 가속도 1% 이내로 일치했지만 ay 차이가 60초 표준편차의 약 4배이고, 주파수(50/100Hz)·timestamp clock(REALTIME/MONOTONIC)·자세 재현을 맞추지 않아 부분 비교로 둔다.
- 실제 수집률·간격·오류·누락 의심 구간을 기록한다. 타임스탬프 간격만으로 하드웨어 누락을 단정하지 않는다.
- 각 단계에서 발생한 실제 문제를 빌드·부팅·매칭·버스·IRQ·형식 등 계층별 근거로 정리한다. 검증을 위해 위험한 고장을 만들지 않는다.
- 새 세션에서 빌드·배포·바인딩·수집을 재현할 수 있도록 명령과 설정을 남긴다.

## 2부: 부팅 체계와 장치 확장

필수 과정에서 수동으로 검증한 커널·overlay·IIO 수집을 U-Boot·Yocto 부팅 체계로 묶고, VEDA Final 장치를 하나씩 추가한 뒤 마지막에 한 이미지로 통합한다. 작업 노트·브랜치·config·Yocto 경로 규칙은 [AGENTS.md](../AGENTS.md)를 따른다.

| 단계 | 실습 | SD 카드 | 상태 | 완료 기준 |
| --- | --- | --- | --- | --- |
| 11 | U-Boot 부팅 | 기존 카드 | 완료 | 백업·복구 절차 확보 후 U-Boot 경유 부팅, firmware가 적용한 DTB·overlay가 커널까지 전달됨을 확인 |
| 12 | Yocto 최소 이미지 (센서 없음) | 새 카드 | 예정 | meta-raspberrypi + `yocto/meta-bsp-study` 최소 이미지 빌드·부팅 |
| 13 | 카메라 IMX219 | 기존 카드 | 진행 | 드라이버 probe·미디어 장치 등록, 프레임 캡처 확인 |
| 14 | I2S 마이크 | 기존 카드 | 예정 | ALSA 캡처 장치 등록, 녹음 파일 확인 |
| 15 | PCA9685 팬·틸트 | 기존 카드 | 예정 | PWM 채널로 서보 위치 제어 |
| 16 | 초음파 IIO srf04 | 기존 카드 | 예정 | IIO 거리 값 읽기, 거리 변화에 따른 값 변화 확인 |
| 17 | 모터 PWM | 기존 카드 | 예정 | 하드웨어 PWM·방향 GPIO로 정·역·정지 제어, 별도 전원 확인 |
| 18 | 통합 | 새 카드 | 예정 | 모든 장치와 IMU를 `meta-bsp-study`에 추가, 핀·주소 충돌 확인, Yocto 이미지에서 장치별 재검증 |

- 13~17단계는 장치마다 작업 노트 1개로 WSL 크로스 빌드 경로와 기존 6.18.59 커널 카드에서 실측한다.
- 아래 "확인함"은 2026-11-09 에이전트 읽기 확인(`external/linux` b3495ea, `out/kernel-rpi4/.config`)이며 보드 동작 확인이 아니다.
- 새 장치의 배선·전원은 실제 연결 후 [bringup](bringup.md)에 기록한다. Pi GPIO는 3.3V이며 5V를 견디지 못한다.

## 11. U-Boot 부팅
- 부팅용 SD 카드를 쓴다(T-B01은 카드가 한 장이라 기존 카드에서 진행). 변경 전에 부트 파티션 백업과 복구 절차(SD 리더기로 `config.txt`·부팅 파일 되돌리기)를 먼저 확인한다.
- Pi firmware가 `config.txt`에 따라 U-Boot를 읽고, 기본 DTB에 `dtoverlay`를 적용한 DT를 U-Boot에 넘긴다. U-Boot가 이 DT를 커널에 그대로 넘기는지 커널의 `/proc/device-tree`에서 overlay 노드로 대조한다.
- 2026-11-10 완료(T-B01): U-Boot `v2026.10`(`rpi_arm64_defconfig`)과 `boot.scr`로 커스텀 커널을 부팅하고, firmware DT(`fdt_addr`)를 그대로 넘겨 overlay 노드·`interrupt-parent`가 커널에 남는 것과 원복을 확인했다([U-Boot 문서](../boot/uboot/uboot-boot.md)). 시리얼 콘솔과 PC SD 리더 복구는 미검증이다.
- 확인할 계층: firmware → U-Boot → 커널 → rootfs. 실패 시 어느 단계에서 멈췄는지 시리얼·화면 출력으로 구분한다(시리얼 콘솔 준비 여부 확인 필요).

## 12. Yocto 최소 이미지
- meta-raspberrypi와 저장소 내 `yocto/meta-bsp-study` 레이어로 최소 이미지를 만든다. Yocto 릴리스와 meta-raspberrypi 브랜치는 이 단계 계획에서 고정한다(확인 필요).
- 커널을 현재 6.18.59 커밋(b3495ea)에 맞출지(linux-raspberrypi bbappend) meta-raspberrypi 기본 커널을 쓸지는 이 단계 계획에서 결정한다. 기본 커널을 쓰면 외부 모듈을 그 커널 버전에 맞춰 다시 빌드한다.
- 11단계 U-Boot를 이미지 부팅 경로에 넣는 설정은 선택한 meta-raspberrypi 브랜치 문서로 확인한다.
- 계획 초안: [T-B02](tasks/T-B02-yocto-minimal.md)(참고 자료). 새 카드 도착 후 데스크톱에서 진행한다.
- 장시간 빌드와 SD 카드 기록은 사용자가 한다. Codex는 레시피·conf 작성과 정적 확인까지 한다.

## 13. 카메라 IMX219
- 확인함: `drivers/media/i2c/imx219.c`(`CONFIG_VIDEO_IMX219=m`), `CONFIG_VIDEO_BCM2835_UNICAM=m`, `overlays/imx219-overlay.dts`. VEDA Final 문서에 IMX219 언급이 있다. 실물 모듈 모델은 확인 필요.
- 센서 드라이버(I2C 제어)와 CSI 수신 드라이버(unicam)의 역할, media controller 그래프를 해설한다.
- 작업 노트: [13-camera](tasks/13-camera.md).

## 14. I2S 마이크
- 마이크 모델은 확인 필요(VEDA_Final_Project에서 모델명을 찾지 못함).
- 후보 확인함: `sound/soc/codecs/ics43432.c`·`adau7002.c`·`dmic.c`, simple-card, bcm2835-i2s(모두 `=m`), `overlays/adau7002-simple-overlay.dts`, `overlays/googlevoicehat-soundcard-overlay.dts`. 모델 확인 후 맞는 codec·overlay를 고른다.
- ASoC의 CPU DAI·codec·machine(card) 연결을 해설하고 `arecord`로 캡처를 확인한다.
- I2S는 GPIO18~21을 쓴다. 17단계 PWM 기본 핀과의 충돌을 함께 확인한다.

## 15. PCA9685 팬·틸트
- 확인함: `drivers/pwm/pwm-pca9685.c`(`CONFIG_PWM_PCA9685=m`), `overlays/i2c-pwm-pca9685a-overlay.dts`.
- I2C1 버스를 MPU6050(0x68)과 공유한다. PCA9685 실제 주소와 서보 전원 조건은 실물로 확인하고, IMU와의 공존은 18단계에서 확인한다.
- PWM 서브시스템(pwmchip·채널·주기·duty)을 해설하고 sysfs로 서보 위치를 제어한다.

## 16. 초음파 IIO srf04
- 확인함: `drivers/iio/proximity/srf04.c`, binding `devantech-srf04.yaml`. 현재 설정은 `# CONFIG_SRF04 is not set`이고 rpi overlay는 없다. 따라서 config fragment 추가와 사용자 커널(모듈) 재빌드, Codex의 overlay 작성이 필요하다.
- 실물 모듈 모델은 확인 필요. HC-SR04 계열이면 Echo가 5V이므로 분압·레벨 변환을 배선 전에 확인한다.
- GPIO 펄스 폭 측정을 IIO 채널로 노출하는 구조를 1~10단계의 IIO 구현과 비교한다.

## 17. 모터 PWM
- 전류·전원 위험과 다른 장치 영향이 커서 마지막에 둔다. 모터 전원은 Pi와 분리하고 GND만 공통으로 둔다.
- VEDA Final `tank/src/tank_drive.cpp`는 EN/IN1/IN2와 wiringPi `pwmWrite`로 제어한다. 모터 드라이버 칩 모델은 확인 필요.
- 확인함: `drivers/pwm/pwm-bcm2835.c`(`=m`), `pwm-gpio`(`=m`), `overlays/pwm-overlay.dts`·`pwm-2chan-overlay.dts`. pwm-2chan 기본 핀은 GPIO18/19로 I2S와 겹치므로 핀 선택은 14단계 배선과 함께 정한다.
- 속도는 PWM sysfs, 방향은 libgpiod로 제어하고 정지·종료 시 출력이 꺼지는지 확인한다.

## 18. 통합
- 12단계 최소 이미지에 IMU(IIO 모듈·overlay·수집 앱)와 13~17단계 장치의 모듈·overlay·설정·검증 도구를 `meta-bsp-study` 레시피로 한 번에 추가한다.
- 장치를 모두 켠 상태에서 핀(I2S GPIO18~21과 PWM, I2C1 공유)·I2C 주소·전원 충돌을 확인한다.
- Yocto 이미지에서 장치별 확인 명령을 다시 실행해 기존 카드 결과와 비교한다. libcamera 등 사용자 공간 패키지 필요 여부는 이 단계에서 확인한다.

## 제외 범위
- EIS 보완은 현재 범위에서 제외한다. 추후 사용자가 별도로 요청할 때 대상·범위부터 다시 계획한다.

## 공식 자료 진입점
실제 명령과 API는 대상 커널 버전에 맞춰 다시 확인한다.
- https://www.raspberrypi.com/documentation/computers/linux_kernel.html
- https://www.raspberrypi.com/documentation/computers/configuration.html
- https://docs.kernel.org/kbuild/modules.html
- https://docs.kernel.org/i2c/writing-clients.html
- https://docs.kernel.org/driver-api/iio/index.html
- https://docs.yoctoproject.org/
- https://docs.u-boot.org/
- https://git.yoctoproject.org/meta-raspberrypi/
