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

## 최종 목표
VEDA Final 프로젝트에서 사용한 모듈·센서(IMU, 카메라, I2S 마이크, PCA9685 팬·틸트, 초음파, 모터)를 BSP 관점에서 처리하고 U-Boot·Yocto로 묶는다. 필수 목표 이후 [roadmap 2부](docs/roadmap.md#2부-부팅-체계와-장치-확장)에서 부팅 체계(U-Boot → Yocto 최소 이미지)를 먼저 갖추고, 카메라부터 모터까지 장치를 하나씩 추가한다. EIS 보완은 현재 범위에서 제외한다.

## 현재 상태

WSL에서 ARM64 프로그램·커널·외부 모듈을 크로스 빌드하고 Pi 4의 커널 `6.18.59-v8-bsp-study01`에서 실행했다. 커널 부팅·화면 장애 복구, 문자 디바이스·Device Tree/platform 실습을 확인했다.

MPU6050의 직접 작성한 I2C·IRQ·FIFO 예제를 거쳐 IIO raw·scale과 IRQ 버퍼 수집을 확인했다. 현재 앱은 IRQ 타임스탬프를 포함한20개 샘플을 SI 단위로 출력하고 정상 종료했다. 관찰한 IRQ 간격은 평균10.015ms이며, 버퍼 중지 후 raw 조회 재개와 장치·모듈 정리도 확인했다. 코드 해설·실측과 버퍼 포화 문제는 [IIO 문서](examples/mpu6050_iio/iio-direct.md)에 정리했다. 장기 수집·측정 정확도·오류/경쟁 경로 등 미검증 항목은 IIO 문서 6장에 구분한다.

2부에서는 Pi firmware 다음 단계에 U-Boot를 넣어 같은 커스텀 커널을 부팅하고, firmware가 만든 DT가 그대로 커널에 전달됨을 확인했다([U-Boot 문서](boot/uboot/uboot-boot.md)). Yocto 최소 이미지는 계획 초안 단계다.

## 복습·재현

- [첫 사용자 프로그램](examples/hello/cross-compile.md)
- [커널 소스·설정·빌드·백업·설치·부팅·복구](docs/kernel-build.md)
- [모듈 코드·Makefile·빌드·로드·해제](examples/hello_module/kernel-module.md)
- [문자 디바이스 코드·데이터 왕복·ioctl](examples/char_device/char-device.md)
- [Device Tree·platform 드라이버](examples/platform_device/platform-device.md)
- [MPU6050 I2C 매칭·식별값 읽기](examples/mpu6050/mpu6050.md)
- [MPU6050 IIO 채널·scale·IRQ 버퍼·mainline 비교](examples/mpu6050_iio/iio-direct.md)
- [U-Boot 경유 커스텀 커널 부팅](boot/uboot/uboot-boot.md)
- [카메라 IMX219 센서·CSI 수신 드라이버](examples/camera/camera.md)
- [I2S 마이크 INMP441·ASoC](examples/mic/mic.md)

실습 코드는 Codex, 계획·리뷰·문서는 Claude Code가 작성하고, 모든 수정은 사용자 승인 후 진행한다. 보드 배포·실행·실측과 커밋은 사용자가 한다.

## 시작
1. 이 저장소와 같은 상위 폴더의 `VEDA`, `VEDA_Final_Project`를 참고한다. 기존 clone이 있으면 그 위치를 사용한다.
2. `AGENTS.md`(공통), `CLAUDE.md`(Claude Code)를 읽고, `docs/roadmap.md`(완료 상태)와 `docs/worklog.md`(다음 작업·열린 브랜치)를 확인한다. 진행 중인 작업은 해당 브랜치의 작업 노트 `상태:` 줄을 본다.
3. 보드와 개발 PC의 실제 환경은 `docs/bringup.md`에서 관리한다.
4. 작업 노트 단위로 진행한다.

```
[사용자·Claude] PM 대화로 계획 확정 (plan) → docs/tasks/NN-주제.md 작업 노트
[Claude]        코드 구현이 필요하면 docs/tasks/NN-주제.codex.md 지시서
[Codex]         지시서 읽기 → 구현 계획 승인 → 구현·WSL 빌드 → 구현 보고
[Claude]        리뷰 (review) → 수정 지시 또는 통과
[사용자]        노트 진행 순서대로 보드 실행 → 대표 출력을 노트 '결과'에
[Claude]        마무리 (wrap-up) → 브랜치에서 학습 문서 작성·갱신 → 추천 커밋 메시지·머지 후 반영 블록
[사용자]        커밋·머지
[Claude]        머지 후 main에서 worklog·roadmap 갱신 (Docs: NN 상태 반영)
```

NN은 roadmap 단계 번호다. skill(`plan`·`review`·`wrap-up`)은 대화로 요청해도 적용된다. 브랜치·커밋 규칙은 [AGENTS.md 커밋·브랜치](AGENTS.md#커밋브랜치)에 있다.

참고 레포를 이 저장소 내부에 넣거나 함께 커밋하지 않는다. 커널 소스도 별도 관리한다.

## 문서
- [공통 에이전트 지침](AGENTS.md) · [Claude Code 지침](CLAUDE.md) · [문서 규칙](.claude/rules/docs.md)
- [작업 노트](docs/tasks/) · [학습 문서 템플릿](docs/learning-template.md)
- [학습 순서와 완료 기준](docs/roadmap.md)
- [환경·배선·참고 자료](docs/bringup.md)
- [다음 작업과 열린 브랜치](docs/worklog.md)

## 참고 저장소
- https://github.com/naktaa/VEDA — 교육 자료와 수업 코드
- https://github.com/naktaa/VEDA_Final_Project — 기존 IMU 수집 방식과 센서 사용 사례

필요한 구현과 검증 도구를 단계별로 추가한다.
