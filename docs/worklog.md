# 현재 진행 상태

## 현재 단계
9단계 IIO의 raw·scale 조회와 IRQ 트리거·버퍼의 기본 수신·장치 제거를 확인했다. 새 앱20행·종료0, 버퍼 중지·I2C/IIO 장치 부재와 REMOVE가 근거다. 상세 결과와 버퍼 포화 처리는 IIO 학습 문서에 모았다.

## 완료 요약
- WSL ARM64 크로스 빌드·Pi 실행, 커스텀 커널 6.18.59-v8-bsp-study01 부팅·외부 모듈 실습 완료. overlay 매핑 누락으로 발생한 화면 장애도 복구 확인.
- 문자 디바이스 open/close·echo read/write·ioctl CLEAR와 사용자 앱 검증 완료.
- Device Tree·platform 매칭·속성 읽기·sysfs 조회·정상 제거 확인.
- MPU6050 식별·초기화·raw 및 단위 환산, GPIO17 IRQ 수집 확인. 별도 FIFO·단일 소비자 예제의20개 연속 수신·단절0·정상 해제 확인.
- IIO raw·scale 조회, 6채널/12바이트 버퍼 수집·SI 단위 출력·정상 앱 종료·버퍼 중지·장치 제거 확인. 앱 없이 켠 버퍼의 포화(-EBUSY)와 재시작 후 수신 결과를 구분해 기록했다.

## 막힌 점·미확인
진행을 막는 문제는 없다. IIO raw 접근 제한·중지 후 raw 재개, 오류/수집 중 제거, 정확한 주기·무손실·측정 정확도는 미검증이다. 이번 IIO의 제거 후 IRQ·모듈 해제 조회 결과도 별도 출력이 없어 확정하지 않는다.

기존 커널 복귀 시험, 이름을 바꾼 hello 모듈의 재검증, 센서 슬립 재읽기·AD0 외부 선 및 기판 회로 확인은 남아 있다. 기존 FIFO의 여러 패킷 drain·오류 복구·중복 open 거절도 실제 검증한 범위와 구분한다.

## 다음 작업
사용자가 IIO 코드·문서 변경을 검토하고 직접 커밋한다. 후속 구현은 IIO 타임스탬프·센서 FIFO 등 필요한 범위를 별도 계획한 뒤 진행한다. 아직 EIS 시간 정렬이나 보정이 완료된 상태는 아니다.

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
