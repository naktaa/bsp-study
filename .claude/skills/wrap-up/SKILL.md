---
name: wrap-up
description: 사용자가 보드 결과를 주고 작업을 마무리하자고 할 때 사용한다. 노트 '결과'로 완료 기준을 대조하고 학습 문서·worklog·roadmap을 승인 없이 바로 갱신한다
argument-hint: [NN]
allowed-tools: Read Grep Glob Bash(git diff *) Bash(git status *)
---

대상: `docs/tasks/$ARGUMENTS*.md` (인자가 없으면 상태가 `진행`인 노트)

1. 노트 '결과'가 비어 있으면 마무리하지 않고 필요한 출력이 무엇인지 알려준다. 사용자가 채팅으로 준 출력은 대표 몇 줄과 판단만 '결과'에 옮긴다.
2. 결과를 완료 기준과 대조해 충족·미충족·미검증을 구분한다. 실측에 없는 것을 성공으로 쓰지 않는다.
3. 계획 설명·승인 없이 바로 진행한다. `git diff`로 사용자가 직접 바꾼 내용을 확인한 뒤, 학습 문서(`docs/learning-template.md` 형식)를 작성·갱신하고 worklog·roadmap·bringup(배선이 바뀐 경우)의 필요한 부분만 고친다. 트러블슈팅에 필요한 로그만 학습 문서에 남긴다.
4. 끝나면 바꾼 파일과 내용을 짧게 보고한다.
5. 노트 상태를 `완료`로 바꾸고 추천 커밋 메시지(`Feat:`/`Fix:`/`Docs:`/`Chore:`/`Refactor:` 한 줄)를 준다. roadmap 큰 단계가 끝났으면 태그 이름도 추천한다. 머지·태그 명령은 사용자가 요청할 때만 준다.
