---
name: review
description: 사용자가 Codex 구현이 끝났다고 하거나 구현 리뷰를 요청할 때 사용한다. Codex 지시서의 범위·구현 보고와 git diff를 대조해 리뷰를 기록하고 수정 지시 또는 보드 실행 안내를 준다
argument-hint: [NN 또는 지시서 이름]
allowed-tools: Read Grep Glob Edit(docs/tasks/**) Bash(git diff *) Bash(git status *) Bash(git log *)
---

대상: `docs/tasks/$ARGUMENTS*.codex.md` (인자가 없으면 구현 보고가 있고 리뷰가 비어 있는 지시서. 없거나 둘 이상이면 묻는다)

1. 지시서의 지시·구현 보고와 작업 노트의 완료 기준을 읽고, `git status`·`git diff`로 실제 변경을 확인한다.
2. 아래를 점검한다. 근거는 파일:줄로 적는다.
   - 범위: 변경 파일이 지시서의 변경 파일과 일치하는가, config 변경은 `configs/bsp-study.config`에만 있는가
   - 오류·해제: probe 실패 경로와 remove 해제 순서가 등록의 역순인가, 반환값을 놓치지 않는가
   - 문맥: hard IRQ/threaded IRQ/프로세스 문맥의 sleep 가능 여부, 잠금 범위, IRQ storm 가능성
   - 과잉 구현: 완료 기준에 없는 통계·옵션·추상화·잠금
   - 스타일: AGENTS.md 코드 가독성, 커널 스타일, checkpatch 결과와 남긴 경고의 이유
   - DT/overlay: compatible·reg·interrupts·pinctrl이 bringup.md 배선과 맞는가
   - 부팅·이미지 변경: 노트의 백업·복구 경로를 깨지 않는가
3. 결과를 채팅으로 요약하고 지시서 '리뷰'에 기록한다. 코드는 직접 고치지 않는다.
4. 수정이 필요하면 Codex에 붙여넣을 지시를 한 블록으로 준다. 재작업이 2회를 넘으면 범위를 나누자고 제안한다.
5. 통과면 노트 '진행 순서'의 보드 실행 단계를 짧게 다시 안내한다.
