# 현재 진행 상태

## 현재 단계
Pi firmware → U-Boot `v2026.10` → 커스텀 커널 6.18.59(initramfs 포함) 경로로 부팅하고, firmware가 overlay까지 적용한 DT가 U-Boot를 거쳐 그대로 커널에 도착함을 확인했다(2026-11-10, T-B01). 근거는 `/chosen/u-boot,version` = `2026.10`, 전환 전후 `/proc/cmdline` 동일, `i2c-sensor` overlay 노드와 `interrupt-parent` = `0x07`, 원복 후 직접 부팅이다. 구조·스크립트·재현 절차는 [U-Boot 문서](../boot/uboot/uboot-boot.md)에 있다. `config.txt`는 원복했고 부트 파티션의 `u-boot.bin`·`boot.scr`는 쓰이지 않은 채 남아 있다.

직전 단계: 같은 MPU6050을 mainline `inv_mpu6050`에 연결해 학습용 IIO 드라이버와 비교했다(2026-11-10, T-001). rpi `i2c-sensor` overlay를 부팅 시 적용해 `inv-mpu6050-i2c` 바인딩, GPIO17 Edge IRQ, scan 배치(온도 포함 index 0~7)를 확인했다. 표준 도구 `iio_generic_buffer`로 6축+timestamp 5샘플을 받았다(IRQ 0→5, 간격 약 20ms). 실행 중 `dtoverlay` 적용에서는 `interrupt-parent`가 해결되지 않아 buffer가 생성되지 않았다. 차이와 트러블슈팅은 [IIO 문서 2-4·4-7·5장](../examples/mpu6050_iio/iio-direct.md#2-4-mainline-inv_mpu6050와의-차이)에 있다. `config.txt`는 원복했다.

## 완료 요약
- WSL ARM64 크로스 빌드·Pi 실행, 커스텀 커널 6.18.59-v8-bsp-study01 부팅·외부 모듈 실습 완료. overlay 매핑 누락으로 발생한 화면 장애도 복구 확인.
- 문자 디바이스 open/close·echo read/write·ioctl CLEAR와 사용자 앱 검증 완료.
- Device Tree·platform 매칭·속성 읽기·sysfs 조회·정상 제거 확인.
- MPU6050 식별·초기화·raw 및 단위 환산, GPIO17 IRQ 수집 확인. 별도 FIFO·단일 소비자 예제의20개 연속 수신·단절0·정상 해제 확인.
- timestamp 추가 전 IIO raw·scale 조회, 6채널/12바이트 버퍼 수집·SI 단위 출력·정상 앱 종료·버퍼 중지·장치 제거 확인. 앱 없이 켠 버퍼의 포화(-EBUSY)와 재시작 후 수신 결과를 구분해 기록했다.
- 현재 IIO timestamp 버전의20개 수신·종료0, 19개 IRQ 간격 평균10.015ms와 정상 정리 확인. 상세 수치·대표 로그는 IIO 문서에 기록했다.
- 연속 수집판 60초 정지 수집·CSV 기록·정상 정리와 축별 평균·표준편차 기준선 확보.
- mainline `inv_mpu6050` + `i2c-sensor` overlay 연결, 표준 도구 버퍼 수신, 학습용 드라이버와의 채널·scale·IRQ 종류·timestamp·FIFO 차이 정리(roadmap 9단계 완료).
- U-Boot 경유 부팅과 firmware DT·bootargs 전달, 백업 `config.txt`로 원복 확인(roadmap 11단계 완료).

## 막힌 점·미확인
진행을 막는 문제는 없다. IIO 버퍼 활성 중 raw 접근 제한, 오류/수집 중 제거, 현재판 앱 자체 종료 코드는 미검증이다. 가속도 크기 0.92 g의 원인(영점·감도 오차 추정)은 보정 없이 분리하지 않았다. 짧은 IRQ 간격 관찰만으로 장기 주기 안정성·전체 센서 샘플 무손실·측정 정확도를 판정하지 않는다.

mainline 비교에서는 가속도 ay 차이가 60초 표준편차의 약 4배로 남았고(자세 재현·50/100Hz 필터 차이 미분리), 원복 뒤 IRQ 행·IIO 장치 부재 직접 확인과 실행 중 `dtoverlay`의 `interrupt-parent` 미해결 원인은 확인 필요다.

U-Boot는 PC SD 리더 복구 경로, 배너·스크립트 메시지 원문, 프롬프트 키보드 입력을 확인하지 않았다.

기존 커널 복귀 시험, 이름을 바꾼 hello 모듈의 재검증, 센서 슬립 재읽기·AD0 외부 선 및 기판 회로 확인은 남아 있다. 기존 FIFO의 여러 패킷 drain·오류 복구·중복 open 거절도 실제 검증한 범위와 구분한다.

## 다음 작업
roadmap 13단계 카메라 IMX219(T-S01, 유형 B, 브랜치 `feat/camera`)를 계획한다. roadmap 12단계 Yocto 최소 이미지는 [T-B02 계획 초안](tasks/T-B02-yocto-minimal.md)을 만들어 두었고, 새 SD 카드가 오면 데스크톱에서 이어서 확정한다. Python의 가속도 전환·창 닫기 후 버퍼 중지는 선택 확인이다. IIO 센서 FIFO 연결·EIS 보완은 현재 범위에 추가하지 않는다.

## 상세 문서
- [현재 환경](bringup.md)
- [커널 소스·설정·빌드·부팅·복구](kernel-build.md)
- [모듈 기초](../examples/hello_module/kernel-module.md)
- [문자 디바이스](../examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](../examples/platform_device/platform-device.md)
- [첫 사용자 프로그램](../examples/hello/cross-compile.md)
- [MPU6050 I2C·IRQ·FIFO](../examples/mpu6050/mpu6050.md)
- [MPU6050 IIO·버퍼·실측·트러블슈팅](../examples/mpu6050_iio/iio-direct.md)
- [U-Boot 경유 부팅](../boot/uboot/uboot-boot.md)
- [전체 로드맵](roadmap.md)
