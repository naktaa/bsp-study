# 현재 진행 상태

## 현재 단계
MPU6050 IIO 연속 수집판(경과 초·소수 둘째 자리 CSV)으로 60초 정지 기준선을 확보했다(2026-11-10). 5990샘플/59.98초로 실제 주기 약 99.85Hz와 일치하고, 종료 후 버퍼 중지·raw 재개·장치·모듈 정리를 확인했다. 가속도 크기는 0.92 g, 자이로 bias는 -2.31/-0.60/+0.57 °/s로 보정 전 관측값이다([IIO 문서 6장](../examples/mpu6050_iio/iio-direct.md#6-미검증한계)). Python 자이로 라이브 그래프는 사용자 화면으로 확인했다.

## 완료 요약
- WSL ARM64 크로스 빌드·Pi 실행, 커스텀 커널 6.18.59-v8-bsp-study01 부팅·외부 모듈 실습 완료. overlay 매핑 누락으로 발생한 화면 장애도 복구 확인.
- 문자 디바이스 open/close·echo read/write·ioctl CLEAR와 사용자 앱 검증 완료.
- Device Tree·platform 매칭·속성 읽기·sysfs 조회·정상 제거 확인.
- MPU6050 식별·초기화·raw 및 단위 환산, GPIO17 IRQ 수집 확인. 별도 FIFO·단일 소비자 예제의20개 연속 수신·단절0·정상 해제 확인.
- timestamp 추가 전 IIO raw·scale 조회, 6채널/12바이트 버퍼 수집·SI 단위 출력·정상 앱 종료·버퍼 중지·장치 제거 확인. 앱 없이 켠 버퍼의 포화(-EBUSY)와 재시작 후 수신 결과를 구분해 기록했다.
- 현재 IIO timestamp 버전의20개 수신·종료0, 19개 IRQ 간격 평균10.015ms와 정상 정리 확인. 상세 수치·대표 로그는 IIO 문서에 기록했다.
- 연속 수집판 60초 정지 수집·CSV 기록·정상 정리와 축별 평균·표준편차 기준선 확보.

## 막힌 점·미확인
진행을 막는 문제는 없다. IIO 버퍼 활성 중 raw 접근 제한, 오류/수집 중 제거, 현재판 앱 자체 종료 코드는 미검증이다. 가속도 크기 0.92 g의 원인(영점·감도 오차 추정)은 보정 없이 분리하지 않았다. 짧은 IRQ 간격 관찰만으로 장기 주기 안정성·전체 센서 샘플 무손실·측정 정확도를 판정하지 않는다.

기존 커널 복귀 시험, 이름을 바꾼 hello 모듈의 재검증, 센서 슬립 재읽기·AD0 외부 선 및 기판 회로 확인은 남아 있다. 기존 FIFO의 여러 패킷 drain·오류 복구·중복 open 거절도 실제 검증한 범위와 구분한다.

## 다음 작업
mainline `inv_mpu6050` 드라이버를 overlay로 연결해 학습용 드라이버와 출력·scale·sysfs 구조를 비교한다(roadmap 9단계 남은 항목, TASK 유형 B). 이후 roadmap 2부 11단계(U-Boot)로 넘어간다. Python의 가속도 전환·창 닫기 후 버퍼 중지는 선택 확인이다. IIO 센서 FIFO 연결·EIS 보완은 현재 범위에 추가하지 않는다.

## 상세 문서
- [현재 환경](bringup.md)
- [커널 소스·설정·빌드·부팅·복구](kernel-build.md)
- [모듈 기초](../examples/hello_module/kernel-module.md)
- [문자 디바이스](../examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](../examples/platform_device/platform-device.md)
- [첫 사용자 프로그램](../examples/hello/cross-compile.md)
- [MPU6050 I2C·IRQ·FIFO](../examples/mpu6050/mpu6050.md)
- [MPU6050 IIO·버퍼·실측·트러블슈팅](../examples/mpu6050_iio/iio-direct.md)
- [전체 로드맵](roadmap.md)
