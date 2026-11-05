# 현재 진행 상태

## 현재 단계
센서 구매품 도착 전까지 4단계 I2C 실물 실습을 보류하고, 5단계 Device Tree·장치 매칭을 진행한다. 기존 I2C 연결 조회와 실습 platform 드라이버·runtime overlay의 매칭·probe/remove·전체 정리까지 확인했다. DT 문자열·정수 읽기와 해제까지 사용자 출력으로 확인했다. sysfs 두 값·0444 권한·overlay 제거 후 파일 부재와 REMOVE 로그 확인, rmmod 명령도 오류 없이 완료됐다.

3단계 결과 — 문자 디바이스 등록·셸 open/release·해제·노드 정리 확인. 사용자 C 앱 open/close·종료 상태 0·커널 콜백 확인. write 14바이트 정상 전달과 커널 수신 로그 확인. 고정 응답 19바이트·EOF 앱 출력 확인. 공유 버퍼 echo 14바이트 길이·내용 일치 앱 출력 확인. ioctl CLEAR 성공·초기화 후 EOF 앱 출력 확인. 진행 순서는 1 → 7 → 2까지 완료했으며 기존 번호를 유지한다.

## 완료 요약
- 1: WSL에서 ARM64 사용자 프로그램 크로스 빌드·Pi 실행 및 종료 상태 0 확인.
- 7: 커널 6.18.59-v8-bsp-study01 크로스 빌드·설치·SSH 부팅 확인, 기존 ext4 루트 유지. 백업과 기존 커널 복귀 절차 준비(복귀 시험 미수행). 화면 미출력은 매핑 누락 복구 후 정상 출력 확인. 2026-11-04 최종 config.txt 전문·복구 후 커널 버전 확인.
- 2: 모듈 Makefile 작성·크로스 빌드·clean·증분 빌드와 Pi 로드·해제 확인. loaded/unloaded 로그 및 lsmod 등록·제거 확인.

- 3(기초): Pi에서 char_drv vermagic 일치·major 236 등록·minor 0 노드 생성·셸 open/release·등록 해제·노드 별도 삭제 확인. C 앱 open/close와 종료 상태 0·콜백 확인. write 14바이트 반환·커널 수신 문자열 일치 확인. 길이 경계 검증은 생략(미수행). 고정 응답 read 19바이트·EOF 앱 출력 확인. echo 14바이트 길이·내용 일치 앱 출력 확인. ioctl CLEAR 성공·초기화 후 EOF 앱 출력 확인. 동시성 검증은 미수행.

- 5(조회): DT의 I2C 노드 → fe804000.i2c → i2c-bcm2835 → i2c-1과 of_node 연결을 사용자 출력으로 확인.

- 5(드라이버): platform_drv의 vermagic·OF 별칭과 Pi 모듈 로드·platform-demo 등록·module 링크 확인. 장치 생성 전 probe 로그 없음. runtime overlay 추가 시 probe·바인딩, 제거 시 remove·장치 부재·드라이버 유지, rmmod 후 드라이버 부재 확인. DT label과 정수 42 읽기·바인딩·REMOVE 및 최종 정리도 확인.

## 막힌 점·미확인
- 화면 장애와 최종 부팅 설정 확인은 완료. 보완한 정상 배포 절차 전체 재실행과 기존 커널 롤백 시험은 미수행이며 진행을 막는 항목은 아님.
- hello 모듈은 현재 `hello` 이름으로 정리됐으며 변경 후 빌드·로드·해제는 미검증이다. 기존 `bsp_hello` 성공 로그는 당시 결과로 보존한다.
- 현재 사용할 센서 모듈이 없어 구매품 도착을 기다린다. 실물 모델·핀·전원·배선 확인과 I2C 실측은 도착 후 진행한다.

## 다음 작업
DT 설정·장치별 상태·sysfs 조회·devm 수명을 복습하고 현재 단계 문서를 검토한다. 필수 속성 실패 경로와 메모리 해제 추적은 미검증이다. 센서 도착 후 4단계 I2C로 돌아간다.

## 상세 문서
- [현재 환경](bringup.md)
- [커널 소스·설정·빌드·설치·부팅·복구](kernel-build.md)
- [모듈 코드·Makefile·빌드·로드·해제](../examples/hello_module/kernel-module.md)
- [문자 디바이스 등록·open/release·정리 실측](../examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](../examples/platform_device/platform-device.md)
- [첫 사용자 프로그램](../examples/hello/cross-compile.md)
- [전체 로드맵과 완료 기준](roadmap.md)
