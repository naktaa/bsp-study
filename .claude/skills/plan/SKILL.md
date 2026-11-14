---
name: plan
description: 사용자가 다음 작업, 새 장치·roadmap 단계의 계획을 세우자고 할 때 사용한다. 대화로 계획을 확정하고 승인받은 뒤 작업 노트(필요하면 Codex 지시서)를 만든다
argument-hint: [주제 또는 비워두면 roadmap 기준 다음 단계]
allowed-tools: Read Grep Glob Bash(git status *) Bash(git log *) Bash(ls *)
---

요청 주제: $ARGUMENTS

1. `git status`·`git branch -a`로 현재 브랜치와 열린 기능 브랜치를 확인한다. `docs/roadmap.md`, `docs/bringup.md`, main의 `docs/worklog.md` '다음 작업'과 끝나지 않은 노트(`grep -L "^상태: 완료" docs/tasks/[0-9]*.md`)를 읽는다. 같은 브랜치에서 진행 중인 노트가 끝나지 않았으면 새 노트를 만들지 않는다. 주제가 비어 있으면 roadmap·worklog 기준 다음 단계를 고른다.
2. 대상 장치·기능의 mainline 드라이버·DT binding·overlay·config 존재 여부를 `external/linux`와 `out/kernel-rpi4/.config`에서 확인한다. 확인하지 못한 것은 "확인 필요"로 둔다.
3. 채팅으로 아래를 설명하고 **승인을 기다린다**(파일은 아직 만들지 않는다).
   - 목표와 roadmap에서의 위치
   - 기본은 mainline 드라이버 + DT 연결. 직접 드라이버 작성이나 코드 구현이 필요한지와 그 이유
   - Codex가 바꿀 파일과 하지 않을 것(코드가 필요할 때만)
   - 완료 기준과 사용자가 보드에서 할 확인, 부팅·SD 카드를 바꾸면 백업·복구 경로
   - 보드에서 하루 안에 끝나는 크기인지. 크면 나눈다
4. 승인되면 `docs/tasks/_note-template.md`로 `docs/tasks/NN-주제.md`(NN = roadmap 단계 번호)를 만든다. 코드 구현이 필요하면 `docs/tasks/_codex-template.md`로 `docs/tasks/NN-주제.codex.md`도 만들고 '지시'를 채운다.
5. 마지막에 추천 브랜치 이름(`<타입>/<짧은-설명>`, 최신 main에서 생성)과 main의 worklog '다음 작업'에 넣을 한 줄을 준다. git 명령은 사용자가 요청할 때만 준다. Codex 지시서를 만들었으면 붙여넣을 한 줄 지시를 준다:
   `docs/tasks/<NN-주제>.codex.md를 읽고 AGENTS.md 규칙에 따라 구현 계획을 먼저 설명해줘.`
