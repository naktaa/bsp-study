---
name: readme
description: 사용자가 README를 정리·갱신하자고 하거나 단계가 머지된 뒤 README 반영이 필요할 때 사용한다. 아래 틀로 루트 README를 갱신한다
allowed-tools: Read Grep Glob Bash(git status *) Bash(git diff *) Bash(git branch *)
---

1. `git branch --show-current`가 main인지 확인한다. 아니면 멈춘다(README는 main에서만 고친다).
2. `docs/learning/00-overview.md`, `docs/components/` 문서들, `docs/roadmap.md` 상태 표, 학습 문서 6장을 읽고 현재 README와 대조해 오래된 문장을 찾는다. `git diff README.md`로 사용자가 직접 바꾼 내용이 있는지 본다.
3. 아래 틀의 섹션 순서·제목대로 바꿀 내용을 설명하고 승인받는다.
4. 끝나면 바뀐 섹션을 짧게 보고하고 추천 커밋 메시지(`Docs: README …`)만 준다. 커밋·push는 사용자가 요청할 때만 한다.

## 틀 (이 순서, 이 제목)

````markdown
# bsp-study

한 줄 소개

```text
00-overview 지도를 hardware / DT / kernel / user 4줄로 줄인 것
```
`*` 직접 작성. 상세 지도는 00-overview 링크.

## 무엇을 했나
- **장치·영역:** 직접 작성한 것과 연결한 것 한 줄씩
- **검증:** devcheck로 두 카드 6개 PASS 한 줄

## 기술 스택
한 줄

## Quick Start (Yocto 이미지)
실제 사용한 명령 블록 1개 + 직접 빌드 커널 경로 링크 한 줄

## 배운 점 · 대표 트러블슈팅
- **현상 → 원인 → 배운 점** 한 줄씩 3~4개, 각 컴포넌트 문서 6장 링크

## 문서
전체 지도, 컴포넌트 문서, 학습 문서, 환경·배선·사양 링크

## 한계
설계상 제약 2~3개
````

## 쓰기 규칙
- 전체 100줄 이내. 배지·로고·Mermaid·이미지 없음. 표는 쓰지 않거나 3열 이하.
- 수치·검증 결과는 README에 쓰지 않고 컴포넌트 문서에 둔다(devcheck PASS 한 줄만 예외).
- Quick Start 명령은 학습 문서 12·18과 `yocto/meta-bsp-study/conf/templates/bsp-study`에서 실제 사용한 것만 쓴다. 경로는 템플릿의 상대 경로가 맞는 실제 위치(`~/yocto`, `~/bspworkspace/bsp-study`)로 쓴다.
- "배운 점"은 컴포넌트 문서 6장의 실제 사례에서 고르고, 사례마다 링크한다.
- "한계"에는 설계상 제약만 쓴다. "안 했다", "미검증" 식 항목(모터·서보 실구동 등)은 넣지 않는다. 학습 문서 6장에 둔다.
- 사용자가 직접 쓴 문장은 사실 오류가 아니면 유지한다. 오류로 보이면 근거와 함께 먼저 알린다.

## 금지
- 개발 방식(에이전트 하네스), 진행 중인 작업, worklog·브랜치 내용
- 학습 문서 목록을 단계별로 길게 나열하는 것(00-overview가 맡는다)
- 장치 사양·배선 반복(hardware.md·bringup.md 링크)
