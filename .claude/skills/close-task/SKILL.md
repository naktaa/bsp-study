---
name: close-task
description: 사용자 보드 실측 결과로 TASK를 마감하고 학습 문서·worklog·roadmap을 갱신한다
disable-model-invocation: true
argument-hint: [T-번호]
allowed-tools: Read Grep Glob Bash(git diff *) Bash(git status *)
---

대상: `docs/tasks/T-$ARGUMENTS*.md`

1. TASK의 '실측 결과'가 비어 있으면 마감하지 않고, 필요한 출력·로그가 무엇인지 알려준다.
2. 실측 결과를 완료 기준과 대조해 충족·미충족·미검증을 구분한다. 실측에 없는 것을 성공으로 쓰지 않는다.
3. `.claude/rules/docs.md`를 따라 갱신할 문서 목록과 각 문서에 들어갈 내용 요약을 채팅으로 설명하고 **승인을 기다린다**. 보통 대상:
   - 해당 실습 학습 문서 (`examples/<주제>/*.md` 또는 `docs/*.md`) — 해당 섹션만 수정
   - `docs/worklog.md` — 현재 상태·다음 작업
   - `docs/roadmap.md` — 단계 완료 표시(실측 근거가 있을 때만)
   - `docs/bringup.md` — 배선·장치가 바뀐 경우
   - README 현재 상태 — 큰 단계가 끝난 경우
4. 승인되면 `git diff`로 사용자가 직접 바꾼 내용이 있는지 확인한 뒤 필요한 부분만 Edit한다.
5. TASK '마감' 섹션에 갱신 문서·미검증 항목·추천 커밋 메시지(`Feat:`/`Fix:`/`Docs:` 한 줄)를 적고 상태를 `완료`로 바꾼다.
6. 면접 요약이 필요한 단계면 `/interview-note`를 제안한다.
