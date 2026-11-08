# bsp-study

Raspberry Pi 4 기반 BSP 학습 저장소. VEDA에서 배운 내용을 실제 환경과 비교하며 커널·Device Tree·드라이버를 직접 구성하고 센서 데이터를 수집·검증한다.

## 필수 목표
- 개발 PC에서 빌드한 프로그램과 커널·모듈을 보드에서 실행하고 부팅·복구 과정 이해
- 문자 디바이스로 사용자/커널 경계와 데이터 전달 실습
- 센서 데이터시트를 읽고 I2C 레지스터 접근 및 최소 센서 드라이버 직접 구현
- Device Tree 매칭, probe/remove, 오류 처리와 자원 수명 이해
- 인터럽트·FIFO와 IIO 버퍼를 통한 연속 수집 및 단위·축·주기·시간 검증
- 문제 발생 시 로그·설정·코드·측정 결과로 원인을 좁히고 재현 절차 기록

센서는 MPU-6050을 기준으로 계획하되 실제 모델·보유 여부부터 확인한다. 기존 프로젝트는 센서 사용 사례와 비교 기준으로 활용한다.

## 선택 과제
EIS 입력 연동, Yocto 이미지 재현, U-Boot 및 카메라 경로 분석은 필수 과정 이후 필요에 따라 진행한다.

## 현재 상태

WSL에서 ARM64 프로그램·커널·외부 모듈을 크로스 빌드하고 Pi 4의 커널 `6.18.59-v8-bsp-study01`에서 실행했다. 커널 부팅·화면 장애 복구, 문자 디바이스·Device Tree/platform 실습을 확인했다.

MPU6050의 직접 작성한 I2C·IRQ·FIFO 예제를 거쳐 IIO raw·scale과 IRQ 버퍼 수집을 확인했다. IIO 앱은20개 샘플을 SI 단위로 출력하고 정상 종료했으며, 버퍼 중지·I2C/IIO 장치 제거도 확인했다. 버퍼 포화 로그의 원인과 재현 절차는 [IIO 문서](examples/mpu6050_iio/iio-direct.md#실제-트러블슈팅)에 정리했다. 타임스탬프·측정 정확도·오류/경쟁 경로의 미검증 항목은 [worklog](docs/worklog.md)에 구분한다.

## 복습·재현

- [첫 사용자 프로그램](examples/hello/cross-compile.md)
- [커널 소스·설정·빌드·백업·설치·부팅·복구](docs/kernel-build.md)
- [모듈 코드·Makefile·빌드·로드·해제](examples/hello_module/kernel-module.md)
- [문자 디바이스 코드·데이터 왕복·ioctl](examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](examples/platform_device/platform-device.md)
- [MPU6050 I2C 매칭·식별값 읽기](examples/mpu6050/mpu6050.md)
- [MPU6050 IIO 채널·scale·IRQ 버퍼](examples/mpu6050_iio/iio-direct.md)

실습 코드 수정·빌드·설치·실행은 사용자가 직접 하고, 에이전트는 설명·읽기 검토·문서 정리를 기본으로 한다.

## 시작
1. 이 저장소와 같은 상위 폴더의 `VEDA`, `VEDA_Final_Project`를 참고한다. 기존 clone이 있으면 그 위치를 사용한다.
2. 에이전트와 `AGENTS.md`, `docs/worklog.md`, `docs/roadmap.md`의 현재 단계를 확인한다.
3. 보드와 개발 PC의 실제 환경을 조사해 `docs/bringup.md`를 채운다.
4. 계획 설명·승인 → 사용자 직접 실습 → 결과와 개념 대화 → 학습 문서·현재 상태 갱신 순서로 진행한다.
5. 의미 있는 변경은 검증 후 추천 커밋 메시지(`Feat:`, `Fix:`, `Chore:` 등)만 남기고 사용자가 직접 커밋한다. 에이전트는 특정 파일의 커밋을 명시적으로 요청받은 경우에만 수행한다.

참고 레포를 이 저장소 내부에 넣거나 함께 커밋하지 않는다. 커널 소스도 별도 관리한다.

## 문서
- [에이전트 지침](AGENTS.md)
- [학습 순서와 완료 기준](docs/roadmap.md)
- [환경·배선·참고 자료](docs/bringup.md)
- [현재 상태와 진행 기록](docs/worklog.md)

## 참고 저장소
- https://github.com/naktaa/VEDA — 교육 자료와 수업 코드
- https://github.com/naktaa/VEDA_Final_Project — 기존 IMU 수집 방식과 센서 사용 사례

필요한 구현과 검증 도구를 단계별로 추가한다.
