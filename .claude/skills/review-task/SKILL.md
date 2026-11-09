---
name: review-task
description: Codex가 구현한 TASK를 git diff 기준으로 리뷰하고 수정 지시와 이해 확인 질문을 남긴다
disable-model-invocation: true
argument-hint: [T-번호]
allowed-tools: Read Grep Glob Bash(git diff *) Bash(git status *) Bash(git log *)
---

대상: `docs/tasks/T-$ARGUMENTS*.md`

1. TASK 파일의 범위·완료 기준·구현 보고를 읽고, `git status`·`git diff`로 실제 변경을 확인한다.
2. 아래를 점검한다. 근거는 파일:줄로 적는다.
   - 범위: 변경 파일이 '변경 파일/새로 만드는 파일' 목록과 일치하는가
   - 오류·해제: probe 실패 경로와 remove의 해제 순서가 등록의 역순인가, 반환값을 놓치지 않는가
   - 문맥: hard IRQ/threaded IRQ/프로세스 문맥에서 sleep 가능 여부, 잠금 범위, IRQ 재발생(storm) 가능성
   - 과잉 구현: 완료 기준에 없는 통계·옵션·추상화·잠금
   - 스타일: AGENTS.md 코드 가독성 규칙, 커널 스타일
   - DT/overlay: compatible·reg·interrupts·pinctrl이 bringup.md 배선과 일치하는가
3. 채팅으로 리뷰 결과를 요약하고, TASK '리뷰' 섹션에 기록한다(이 기록은 승인 불필요). 코드는 직접 고치지 않는다.
4. 수정이 필요하면 Codex에 붙여넣을 지시를 한 블록으로 작성하고 상태를 `구현`으로 되돌린다. 재작업이 2회를 넘으면 TASK 범위를 다시 나누자고 제안한다.
5. 통과 시 상태를 `실측`으로 바꾸고, 면접에서 나올 수준의 이해 확인 질문 3개(왜 이 API·문맥·순서인가)와 모범 답 요지를 적는다. 사용자가 보드에서 실행할 검증 절차를 다시 한 번 짧게 안내한다.
