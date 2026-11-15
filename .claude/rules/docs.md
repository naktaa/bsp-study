---
paths:
  - "**/*.md"
---

# 문서 작성 규칙

문서는 Claude Code가 작성·갱신한다.

## 작업 문서
- 작업 노트 `docs/tasks/NN-주제.md`(NN = roadmap 단계 번호): `docs/tasks/_note-template.md` 형식. 사용자가 보면서 진행하는 문서다. '결과'에는 대표 출력 몇 줄과 판단만 적고 원문 로그를 쌓지 않는다.
- Codex 지시서 `docs/tasks/NN-주제.codex.md`: 코드 구현이 필요할 때만 `docs/tasks/_codex-template.md`로 만든다. 섹션 제목의 작성자 표시(`(Claude)`·`(Codex)`)는 유지하고 "역할 예외" 같은 문구는 쓰지 않는다.
- 작업 노트 상단 `상태:` 값은 `계획`·`진행`·`대기`·`완료` 중 하나로 쓰고, 부가 설명은 뒤에 괄호로 붙인다(예: `상태: 대기 (15-A: 보드 도착 후)`). 진행 중 상태는 이 줄만 기준으로 한다.
- 기존 `T-001`·`T-B01`·`T-B02` 파일은 위치·내용을 그대로 둔다(T-B02는 12단계 참고 자료).
- 기능 브랜치에서는 코드·overlay·레시피·config, 작업 노트, Codex 지시서, 학습 문서만 고친다.
- worklog·roadmap·README와 hardware.md 상단 장치 표의 상태는 main에서만 고친다. 머지 후 한 커밋(`Docs: NN 상태 반영`)으로 갱신하고, 계획 변경은 main에서 `Docs:` 커밋으로 한다.
- 예외: bringup.md와 hardware.md의 장치별 섹션은 배선·사양이 바뀐 브랜치에서 고친다(핀 충돌은 알아야 하는 충돌이다).
- docs/worklog.md: main에서만 갱신하고 '다음 작업' 3~5줄만 둔다. 머지할 때와 브랜치를 시작·대기로 돌릴 때 고친다. 미검증·한계는 해당 학습 문서에 둔다.
- docs/roadmap.md: 학습 순서·단계 상태·완료 기준. 상태 표에는 머지된 단계의 `완료`만 적고 `진행`은 적지 않는다. 단계 본문에는 완료 요약을 쌓지 않고 `작업 노트: [..] · 학습 문서: [..]` 링크 한 줄만 둔다.
- README: 프로젝트 개요·문서 진입점. docs/bringup.md: 환경·장비·배선. docs/hardware.md: 장치 사양·데이터시트·커널 대응.

## 학습 문서
- 위치는 `docs/learning/NN-주제.md`(NN = roadmap 단계 번호, 여러 단계를 다루면 대표 단계)이고 코드는 `examples/`·`boot/`·`yocto/`에 그대로 둔다. 형식은 `docs/learning/_template.md`를 따른다(1 한눈에 보기 → 2 구조 → 3 알아야 할 개념 → 4 구현 흐름 → 5 재현 → 6 트러블슈팅과 한계). 기준 예시는 `docs/learning/06-mpu6050.md`다. 처음 네 섹션만 읽어도 구조·파이프라인·직접 작성한 것·왜 그렇게 했는지 이해되게 쓰고, 복잡하게 만들지 않는다.
- 분량 기준: 코드 블록 비율 30% 이하. 파일 전체·함수 전체를 붙이지 않는다. 발췌는 블록당 15줄 이내, 나머지는 줄 링크.
- "알아야 할 개념"은 5개 이내. 넘치면 6장 "더 보기"에 한 줄 질문 + 링크로 둔다.
- 검증은 현재 버전 결과 표 1개. 버전별 이력·과거 실측은 작업 노트에만 둔다.
- 한 문단 3문장 이내. 단서·주의가 3개를 넘으면 목록으로 쓴다. "~를 보장하지 않는다" 같은 단서는 6장 미검증·한계에 모은다.
- 재현(5장)은 `<details>`로 접는다. 절차 자체가 주제인 문서(07 커널 빌드)는 예외로 펼쳐 둔다.
- 02·03·05에서 설명한 기초(Makefile·Kbuild, 매칭 테이블, file_operations, printf 형식 등)는 뒤 문서에서 반복하지 않고 링크한다.
- 숫자와 한글 사이 띄어쓰기를 지킨다(`20개`는 되고 `받은20개`는 안 된다).
- 파일 이름은 주제를 드러내는 짧은 이름으로 짓고, 이름을 바꾸면 모든 링크를 함께 갱신한다. 중복 문서를 만들지 않고 기존 문서를 보완한다.
- 현재 구현 기준으로 쓴다. 중간 코드·명령·상태 보고를 누적하지 않고, 과거 결과는 현재 이해에 필요한 차이와 근거만 짧은 표·대표 로그로 남긴다. 트러블슈팅에 필요한 로그만 둔다.
- 명령마다 WSL/Pi, 작업 디렉터리와 선행 조건을 적고 변수는 같은 블록에서 정의한다. 실제 사용한 커밋·버전·설정·명령과 관측 결과를 기준으로 하고, 확인하지 못한 것은 "확인 필요"로 적는다.
- VEDA 비교는 실제 확인한 코드 차이와 이유만 남긴다. 예상 동작·일반 주의사항을 실제 트러블슈팅으로 쓰지 않는다. 취소·문구 수정 같은 행정 이력은 넣지 않는다.
- 단계 완료는 보드 실측 등 근거가 있을 때만 표시한다. 복구 절차 준비와 실제 복구 성공, 정적 검토와 재실행 검증을 구분한다.
- 사용자가 직접 수정한 문장은 사실 오류가 아니면 유지한다. 오류로 보이면 바꾸기 전에 근거와 함께 알린다.

## 그림·링크
- Mermaid를 쓰지 않는다. 파이프라인은 `A → B → C` 한 줄(경로가 여럿이면 목록 한 항목에 한 줄), 계층은 `계층 | 구성 요소 | 하는 일` 표로 쓴다. 상자와 연결이 여럿인 구조는 `text` 코드 블록의 ASCII 블록도(`+---+` 상자, `|`·`-->`·`v` 연결)로 보충한다. 상자 라벨은 영어·코드 이름만 쓰고(한글은 폭이 달라 테두리가 어긋남) 한글 설명은 그림 아래에 둔다. SVG·draw.io 등 이미지 파일은 만들지 않는다.
- 줄 링크는 시작 줄만 건다(`[mpu6050_iio.c#L161](mpu6050_iio.c#L161)`). 코드 변경으로 줄 번호가 바뀌면 wrap-up에서 해당 문서의 줄 링크를 함께 갱신한다. Git 대상이 아닌 `external/` 경로는 링크 대신 텍스트로 적는다.

## 문서별 주제
- docs/learning/00-overview.md: 전체 장치·커널 서브시스템·사용자 공간 지도, 단계별 한 줄 요약, 복습 순서
- docs/learning/01-cross-compile.md: ARM64 사용자 프로그램 크로스 빌드·실행
- docs/learning/02-kernel-module.md: 모듈 코드·Makefile·로드·해제
- docs/learning/03-char-device.md: 문자 디바이스·사용자/커널 경계
- docs/learning/05-platform-device.md: Device Tree·platform 매칭·probe/remove
- docs/learning/06-mpu6050.md: MPU6050 I2C·IRQ·FIFO 학습용 드라이버
- docs/learning/07-kernel-build.md: 커널 소스·설정·빌드·백업·배포·initramfs·부팅·복구
- docs/learning/09-iio-direct.md: IIO 채널·버퍼·timestamp, mainline `inv_mpu6050` 비교
- docs/learning/11-uboot-boot.md: U-Boot 빌드·부트 스크립트, DT·bootargs 전달
- docs/learning/12-yocto-minimal.md: Yocto 레이어·템플릿·MACHINE·이미지 레시피, 최소 이미지 빌드·카드 부팅·Wi-Fi
- docs/learning/13-camera.md: IMX219 센서·unicam 드라이버, 노출·타임스탬프 경로, libcamera 없이 subdev 컨트롤·V4L2 캡처(13-2), DT 속성·전원 순서, 고정 모드 센서 드라이버 직접 작성(13-3)
- docs/learning/14-mic.md: INMP441 I2S 마이크, ASoC CPU DAI·코덱·machine과 overlay
- docs/learning/15-pca9685.md: PCA9685 PWM, 업스트림 `pwm-pca9685`와 직접 작성 provider 비교
- docs/learning/16-ultrasonic.md: HC-SR04 초음파, 업스트림 IIO `srf04`·overlay, 거리 값 확인
- docs/learning/18-integration.md: Yocto 6.6 통합 레이어, 직접 작성 모듈 자동 적재·6.6/6.18 compat, devcheck로 두 카드 비교, HDMI DRM 미리보기
