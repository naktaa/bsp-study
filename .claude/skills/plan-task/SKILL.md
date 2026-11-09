---
name: plan-task
description: 사용자가 다음 작업, 새 장치·roadmap 단계의 계획을 확정하자고 할 때 사용한다. TASK 유형과 범위를 설명하고 승인받은 뒤 docs/tasks/에 TASK 파일을 만든다
argument-hint: [주제 또는 비워두면 roadmap 기준 다음 단계]
allowed-tools: Read Grep Glob Bash(git status *) Bash(git log *) Bash(ls *)
---

요청 주제: $ARGUMENTS

1. `git status`로 현재 폴더·브랜치를 보고 트랙(없음·부팅 `boot`·센서 `sensor`)을 확인한다. `docs/worklog.md`, `docs/roadmap.md`, `docs/bringup.md`와 `docs/tasks/`의 기존 TASK를 읽는다. 주제가 비어 있으면 roadmap·worklog 기준 다음 단계를 고른다. 트랙 안에서 진행 중인 TASK가 있으면 새 TASK를 만들지 않는다.
2. 대상 장치·기능에 대해 대상 커널 소스(`external/linux`)와 `out/kernel-rpi4/.config`에서 mainline 드라이버·DT binding·overlay·config 존재 여부를 확인한다. 확인하지 못한 것은 "확인 필요"로 둔다.
3. 채팅으로 아래를 먼저 설명하고 **승인을 기다린다** (파일은 아직 만들지 않는다).
   - **TASK 유형(AGENTS.md: A 직접 구현 / B mainline 드라이버+DT / C 부팅·빌드 체계)과 그 이유**. 유형별 역할 분담을 함께 적는다. 유형 C는 백업·복구 경로를 반드시 포함한다.
   - 목표와 이 TASK가 최종 목표(VEDA Final 장치 전체 BSP 처리)에서 차지하는 위치
   - Codex가 바꿀 파일과 하지 않을 것
   - 완료 기준과 사용자가 보드에서 할 검증
   - 예상 소요(보드 하루 안에 끝나는 크기인지). 크면 TASK를 나눈다.
4. 승인되면 `docs/tasks/_template.md` 형식으로 TASK 파일을 만들고 유형·트랙을 채운다. 상태는 `승인`.
   - 트랙 밖: `docs/tasks/T-NNN-<짧은-주제>.md`, 번호는 기존 `T-NNN` 최대값+1
   - 부팅 트랙: `docs/tasks/T-Bxx-<짧은-주제>.md`, 센서 트랙: `docs/tasks/T-Sxx-<짧은-주제>.md`, 번호는 같은 접두사의 최대값+1(01부터)
5. 마지막에 Codex에 그대로 붙여넣을 한 줄 지시를 준다:
   `docs/tasks/<TASK 파일명>.md를 읽고 AGENTS.md 규칙에 따라 구현 계획을 먼저 설명해줘.`
