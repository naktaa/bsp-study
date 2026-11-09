---
name: interview-note
description: 완료한 BSP 학습 단계를 면접용 1페이지 요약과 예상 질문으로 정리한다
disable-model-invocation: true
argument-hint: [주제 예: imu, kernel-build, uboot]
allowed-tools: Read Grep Glob Bash(git log *) Bash(git diff *)
---

주제: $ARGUMENTS

1. 관련 TASK 파일, 학습 문서, 실제 소스를 읽는다. 문서보다 소스와 실측 기록을 우선 근거로 삼는다.
2. `docs/interview/$ARGUMENTS.md`의 구성안을 채팅으로 설명하고 승인을 기다린다. 파일이 이미 있으면 바꿀 부분만 제시한다.
3. 승인되면 `.claude/rules/docs.md`의 interview 문서 구성으로 작성한다. 한 페이지(약 120줄) 이내.
   - 한 줄 요약: 무엇을 했고 왜 했는지
   - 구조: 장치 → DT → 드라이버 → 커널 프레임워크 → 사용자 공간 흐름
   - 핵심 API와 선택 이유, 대안과 비교(예: misc+자체 큐 → IIO 버퍼)
   - 실측값과 측정 조건
   - 트러블슈팅 1~2개: 증상 → 확인한 계층 → 원인 → 조치
   - 예상 질문 5~8개와 답 요지(30초 분량)
   - 하지 않은 것·한계: VEDA Final에서 한 일과 bsp-study에서 한 일을 구분
4. 존댓말·두괄식, 과장 없이 쓴다. 미검증 항목을 완료처럼 쓰지 않는다.
