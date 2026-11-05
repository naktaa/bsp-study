# 현재 진행 상태

## 현재 단계
8단계 IRQ 기반 최신 샘플 수집·wait queue를 통한 사용자 전달의 정상 경로를 확인했다. 앱20건·Seq2632~2651 연속·Skip0·종료0, 새 IRQ/CHAR 로그와 등록을 확인했다. 정상 제거 후 IRQ 행·장치 노드 부재, REMOVE count=120925, 모듈 해제도 확인했다. 센서 FIFO·정확도/주기 기준선·대기 중 제거 검증은 아직 남아 있다.

## 완료 요약
- 8(IRQ 샘플 전달): 최신 한 건 저장·파일별 순번·blocking read16바이트 전달과 앱 단위 표시 확인.20건 정상 수신·IRQ/노드/모듈 정상 해제 완료.
- 8(IRQ 발생): GPIO17 Level·Linux IRQ61, 약2초에200회 증가 확인. 앱20행·종료0과 공존, 제거 시 IRQ 행/노드 부재·REMOVE count=20268·rmmod 후 lsmod 부재 확인. IRQ 기반 샘플 전달은 아직 미구현.
- 6(단위 표시): 커널 raw 전달·앱 g/deg/s 환산 책임을 유지하며 표 출력과 새 INIT 범위 로그 확인. 두 실행 모두 20행·정상 종료. 현재 단위 표시 버전도 사용자가 정리 완료를 보고했다(추가 명령 출력은 미수신).
- 6(사용자 전달): /dev/mpu6050 자동 생성(0600, major 10/minor 266), CHAR ready·바인딩 확인. 앱 OPEN → raw 20줄 → CLOSE·종료 상태 0 확인. 앱 재실행·움직임 반응·종료 상태 0, overlay 제거 후 노드·client 부재와 REMOVE, rmmod 후 lsmod 부재도 확인. 정확한 주기와 측정 정확도 검증은 별도다.
- 6(raw): 초기화 후 가속도·자이로 3축 raw 로그와 driver 링크 확인. 단일 샘플 수신까지 확인했으며 정확도·연속 수집 검증과 구분한다.
- 6(초기화): 식별 후 리셋·슬립 해제·축 활성화·측정 범위 설정의 readback과 ready·driver 링크 확인. 제거 로그와 제공 구간에 POWER 실패 경고 없음 확인. 슬립 상태 재읽기는 미수행.
- 6(식별): ARM64 모듈·vermagic·별칭 확인, overlay 매칭과 커널 WHO_AM_I=0x68 확인. overlay 제거 시 장치 부재·REMOVE·드라이버 유지, rmmod 후 드라이버 부재 확인.
- 4(식별): 사용자 배선 확인, 기존 driver 링크 부재 확인 후 i2ctransfer로 레지스터 0x75를 읽어 0x68 수신. 측정값 수집은 미수행.
- 1: WSL에서 ARM64 사용자 프로그램 크로스 빌드·Pi 실행 및 종료 상태 0 확인.
- 7: 커널 6.18.59-v8-bsp-study01 크로스 빌드·설치·SSH 부팅 확인, 기존 ext4 루트 유지. 백업과 기존 커널 복귀 절차 준비(복귀 시험 미수행). 화면 미출력은 매핑 누락 복구 후 정상 출력 확인. 2026-11-04 최종 config.txt 전문·복구 후 커널 버전 확인.
- 2: 모듈 Makefile 작성·크로스 빌드·clean·증분 빌드와 Pi 로드·해제 확인. loaded/unloaded 로그 및 lsmod 등록·제거 확인.

- 3(기초): Pi에서 char_drv vermagic 일치·major 236 등록·minor 0 노드 생성·셸 open/release·등록 해제·노드 별도 삭제 확인. C 앱 open/close와 종료 상태 0·콜백 확인. write 14바이트 반환·커널 수신 문자열 일치 확인. 길이 경계 검증은 생략(미수행). 고정 응답 read 19바이트·EOF 앱 출력 확인. echo 14바이트 길이·내용 일치 앱 출력 확인. ioctl CLEAR 성공·초기화 후 EOF 앱 출력 확인. 동시성 검증은 미수행.

- 5(조회): DT의 I2C 노드 → fe804000.i2c → i2c-bcm2835 → i2c-1과 of_node 연결을 사용자 출력으로 확인.

- 5(드라이버): platform_drv의 vermagic·OF 별칭과 Pi 모듈 로드·platform-demo 등록·module 링크 확인. 장치 생성 전 probe 로그 없음. runtime overlay 추가 시 probe·바인딩, 제거 시 remove·장치 부재·드라이버 유지, rmmod 후 드라이버 부재 확인. DT label과 정수 42 읽기·바인딩·REMOVE 및 최종 정리도 확인.

## 막힌 점·미확인
- 화면 장애와 최종 부팅 설정 확인은 완료. 보완한 정상 배포 절차 전체 재실행과 기존 커널 롤백 시험은 미수행이며 진행을 막는 항목은 아님.
- hello 모듈은 현재 `hello` 이름으로 정리됐으며 변경 후 빌드·로드·해제는 미검증이다. 기존 `bsp_hello` 성공 로그는 당시 결과로 보존한다.
- 사용자가 GY-521/MPU-6050 보유와 핀 표기를 확인했다. 전원·SDA/SCL 배선과 주소 0x68의 식별 응답을 확인했다. 정확한 기판 회로·AD0 풀다운·외부 AD0 선 상태는 미확인이다. 식별 전용 I2C 드라이버의 정상 경로는 검증했다. 초기화 버전 readback·바인딩·REMOVE는 확인했고, raw 한 샘플 출력은 확인했다. 현재 문자 인터페이스의 노드 생성·앱 20회 읽기는 확인했다. 앱 재실행·노드 자동 제거·모듈 해제도 확인했다. 새 단위 표시 버전의 두 실행과 정상 종료도 확인했다. 해당 버전도 사용자가 정리 완료를 보고했다. 슬립 재읽기·오류 경로·제거 중 열린 파일 수명·측정 정확도도 미확인이다. 연결 제안과 핀 현황은 bringup에 모았다.

## 다음 작업
최신 한 건 보관과 센서 FIFO의 차이, FIFO 데이터 형식·용량·읽기 및 overflow 처리 순서를 설명하고 승인 후 다음 구현을 진행한다. 대기 중 제거 등 미검증 경로는 완료로 표시하지 않는다.

## 상세 문서
- [현재 환경](bringup.md)
- [커널 소스·설정·빌드·설치·부팅·복구](kernel-build.md)
- [모듈 코드·Makefile·빌드·로드·해제](../examples/hello_module/kernel-module.md)
- [문자 디바이스 등록·open/release·정리 실측](../examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](../examples/platform_device/platform-device.md)
- [첫 사용자 프로그램](../examples/hello/cross-compile.md)
- [전체 로드맵과 완료 기준](roadmap.md)
- [MPU6050 I2C 드라이버·식별 읽기](../examples/mpu6050/mpu6050.md)
