# 13-2 카메라 노출·게인·테스트 패턴 — Codex 지시서

작업 노트: [13-2-camera-ctrl.md](13-2-camera-ctrl.md) — '진행 순서' 3~5단계가 두 프로그램의 사용법이다.

## 지시 (Claude)
- 변경 파일(새로 만듦):
  - `examples/camera/v4l2_capture.c`
  - `examples/camera/raw_tool.py`
  - `examples/camera/Makefile`
- 기준(읽기 전용):
  - `external/linux/include/uapi/linux/videodev2.h`: `v4l2_requestbuffers`·`v4l2_buffer`·`v4l2_format`, `VIDIOC_*`
  - `external/linux/Documentation/userspace-api/media/v4l/mmap.rst`: MMAP 스트리밍 순서
  - `examples/mic_app/Makefile`: 사용자 앱 Makefile 형식(CROSS_COMPILE·APP_CC·PI_HOST·PI_APP_DIR, `deploy`)

### 요구사항

**`v4l2_capture.c`** (Pi에서 실행, 150줄 안팎)
- 사용법: `v4l2_capture <video 장치> <프레임 수> <출력 RAW 파일>`
- 포맷은 설정하지 않는다. `VIDIOC_G_FMT`로 현재 포맷을 읽어 width·height·bytesperline·sizeimage를 한 줄 출력한다(포맷은 노트 2단계에서 `media-ctl`·`v4l2-ctl`로 정함)
- `VIDIOC_REQBUFS`(count 4, `V4L2_MEMORY_MMAP`) → 버퍼마다 `VIDIOC_QUERYBUF` + `mmap` → 모두 `VIDIOC_QBUF` → `VIDIOC_STREAMON`
- 프레임 수만큼 `VIDIOC_DQBUF` → 한 줄 출력: `sequence`, `timestamp`(초.마이크로초), 직전 프레임과의 간격(ms, 첫 프레임은 0), `bytesused` → 다시 `VIDIOC_QBUF`
- 마지막 프레임 하나를 `bytesused`만큼 출력 파일에 저장한다
- 끝에 한 줄 요약: 받은 프레임 수, 평균 간격(ms, 첫 프레임 제외), sequence 끊김 수(`sequence != 직전 + 1`인 횟수)
- `VIDIOC_STREAMOFF` → `munmap` → `close`. 모든 ioctl·mmap·파일 쓰기 실패는 `perror`로 알리고, 그때까지 만든 자원을 정리한 뒤 0이 아닌 값으로 종료
- `poll`·select·timeout은 넣지 않는다(blocking `DQBUF`)

**`raw_tool.py`** (Pi에서 실행, numpy·Pillow)
- 사용법: `raw_tool.py <RAW 파일> <width> <height> --stride <bytes per line> [--png <출력 PNG>]`
- 입력: unpacked 10bit(`RG10`, little-endian 16bit 컨테이너, 값 0~1023), 한 줄 `stride` 바이트(뒤 패딩은 버림), Bayer 순서 RGGB 고정
- 출력(표준 출력)
  - black level 64를 뺀 값(0 미만은 0)의 채널별 평균 R·Gr·Gb·B와 전체 평균
  - 포화 픽셀 비율(원래 값이 1023인 픽셀 수 / 전체, %)
- `--png`: 2×2 묶음을 한 픽셀로 디베이어(R, (Gr+Gb)/2, B)해 절반 해상도 RGB를 만들고, black level 차감 후 `(1023 − 64)`으로 나눠 0~255로 저장. 색 보정·감마·화이트밸런스는 하지 않음
- 80줄 이하. argparse 외 추가 옵션 없음

**`Makefile`**
- 타깃 `all`(= app)·`app`(`v4l2_capture`, `$(APP_CC) $(APP_CFLAGS)`)·`deploy`(`v4l2_capture`·`raw_tool.py`를 `$(PI_HOST):$(PI_APP_DIR)/`로 scp)·`clean`, `.PHONY`
- `APP_CFLAGS ?= -Wall -Wextra -O2`

**스타일**
- C는 커널 스타일(탭 8, 함수 중괄호 다음 줄)과 AGENTS.md 가독성 규칙(처리 단위 사이 빈 줄, 한글 주석은 의도·제약에만)
- Python은 함수 2~3개(읽기, 통계, PNG) 수준으로 단순하게

### 하지 말 것
- 포맷 설정(`S_FMT`)·컨트롤 설정을 앱에 넣지 않음(노트의 `media-ctl`·`v4l2-ctl`이 맡음)
- `DMABUF`·`USERPTR`, 다중 스레드, `poll`, 이미지 처리(감마·AWB·노출 계산), 영상 저장·인코딩
- 통계·재시도·옵션 추가, 다른 디렉터리 파일 수정
- 보드 조작(scp 포함), 커밋

### 빌드 확인 명령
```bash
# [WSL] ~/bspworkspace/bsp-study
make -C examples/camera
file examples/camera/v4l2_capture
external/linux/scripts/checkpatch.pl --no-tree -f examples/camera/v4l2_capture.c
python3 -m py_compile examples/camera/raw_tool.py
wc -l examples/camera/v4l2_capture.c examples/camera/raw_tool.py
```

기대: 경고 없이 빌드, `ELF 64-bit LSB ... ARM aarch64`, checkpatch 결과 기록(사용자 앱이라 남는 경고는 이유를 적음), py_compile 성공

## 구현 보고 (Codex)
- 변경 요약:
  - `examples/camera/v4l2_capture.c`: 현재 포맷 조회, MMAP 버퍼 요청·매핑·큐잉, blocking 캡처, 프레임별 sequence·timestamp·간격·bytesused 출력, 마지막 프레임 RAW 저장, 평균 간격·sequence 끊김 요약을 구현했다. 오류 시 STREAMOFF·munmap·close를 수행하고 실패로 종료한다. RAW의 지연된 쓰기 오류도 `fclose`에서 확인한다.
  - `examples/camera/raw_tool.py`: little-endian 16bit 컨테이너의 RGGB RAW에서 stride 패딩을 제외하고, black level 64 차감 후 채널별·전체 평균과 원래 값 기준 포화 비율을 출력한다. 선택한 PNG는 2×2 Bayer 묶음의 절반 해상도 RGB로 저장한다.
  - `examples/camera/Makefile`: CROSS_COMPILE·APP_CC·APP_CFLAGS·PI_HOST·PI_APP_DIR와 all/app/deploy/clean을 추가했다. `deploy`는 앱 빌드를 선행하고 두 파일을 함께 전송하도록 작성했다.
  - 위 세 파일과 이 구현 보고 외 파일은 수정하지 않았다.
- 빌드 결과:
  - `make -C examples/camera`: 종료 0. `aarch64-linux-gnu-gcc -Wall -Wextra -O2 v4l2_capture.c -o v4l2_capture`, 컴파일 경고 없음.
  - `file examples/camera/v4l2_capture`: `ELF 64-bit LSB pie executable, ARM aarch64`, 인터프리터 `/lib/ld-linux-aarch64.so.1` 확인.
  - `python3 -m py_compile examples/camera/raw_tool.py`: 종료 0, 최종 수정 후에도 성공.
  - `wc -l examples/camera/v4l2_capture.c examples/camera/raw_tool.py`: C 170줄, Python 61줄. C는 입력·버퍼 검사와 오류 정리를 생략하지 않아 150줄보다 20줄 길다. Python은 80줄 이하이다.
  - `make -n -C examples/camera deploy`: 종료 0. `scp v4l2_capture raw_tool.py pi:bsp/apps/` 명령 생성만 확인했다. 실제 deploy·scp는 실행하지 않았다.
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f examples/camera/v4l2_capture.c`):
  - 종료 0, `total: 0 errors, 0 warnings, 170 lines checked`.
- 판단이 필요했던 부분:
  - 대상 소스 `external/linux/Makefile`에서 6.18.59를 확인하고, 지정된 UAPI 구조체·ioctl 및 `mmap.rst`의 single-planar 순서를 읽어 구현했다. Makefile은 `examples/mic_app/Makefile` 형식을 따랐다.
  - 버퍼는 4개를 요청하되 REQBUFS가 실제 반환한 개수만큼 할당·매핑한다. 0개 반환, 잘못된 DQBUF 인덱스·길이는 실패로 처리한다.
  - 마지막 프레임은 DQBUF로 소유한 동안 `bytesused`만큼 저장한 뒤 다시 QBUF한다. 첫 프레임 간격은 0이며 평균에서 제외하고, 1프레임만 받으면 평균도 0이다. sequence의 32bit 순환은 연속으로 취급한다.
  - 채널 통계는 각 원시 픽셀에서 64를 빼고 0으로 제한한다. PNG는 지시서 순서대로 원시 Gr·Gb를 먼저 평균낸 후 RGB에서 64를 빼고 0으로 제한하여 959 기준으로 스케일한다.
  - RGGB 2×2 처리에 맞춰 양의 짝수 해상도를 요구하고, 부족한 stride·파일 길이와 10bit 범위 밖 값을 거부한다. 옵션·통계·재시도는 추가하지 않았다.
  - WSL Python에는 numpy·Pillow가 없어 RAW 통계·PNG 생성의 실제 실행은 검증하지 않았다. `py_compile` 성공은 문법 확인이며, 보드 캡처·파일 저장·프레임 간격·sequence·이미지 결과는 사용자 보드 검증이 필요하다.
  - 보드 조작, 전체 커널·Yocto 빌드, 커밋은 수행하지 않았다.

### 재작업 1회차

- 변경 요약:
  - `examples/camera/v4l2_capture.c`의 `video_ioctl()`에 `const char *name`을 추가하고 실패 시 `perror(name)`을 호출하도록 수정했다. 모든 호출부에 해당 `VIDIOC_*` 이름 문자열을 전달한다.
  - `main()`의 입력 검사, open·G_FMT, REQBUFS·calloc, QUERYBUF·mmap 루프, QBUF 루프, STREAMON, 캡처 루프 내부 처리, 정리 단위 사이에 빈 줄을 추가했다. 주석은 추가하지 않았다.
- 검증 결과:
  - `make -C examples/camera`: 종료 0. `aarch64-linux-gnu-gcc -Wall -Wextra -O2 v4l2_capture.c -o v4l2_capture`, 경고 없이 재빌드 성공.
  - `external/linux/scripts/checkpatch.pl --no-tree -f examples/camera/v4l2_capture.c`: 종료 0, `total: 0 errors, 0 warnings, 191 lines checked`.
  - `wc -l examples/camera/v4l2_capture.c examples/camera/raw_tool.py`: C 191줄, Python 61줄, 합계 252줄.
  - 수정 전 소스에 ioctl 이름 인자·`perror(name)` 변경을 적용한 내용과 수정 후 소스를 공백 제외로 비교하여 일치함을 확인했다. 요청한 실패 메시지 변경 외 동작·출력 형식·처리 순서는 유지했다.
- 판단이 필요했던 부분: 추가 판단이나 범위 확장 없음. 소스는 C 파일만 수정했고, 문서는 이 구현 보고만 덧붙였다. 보드 조작·커밋은 수행하지 않았다.

## 리뷰 (Claude)
- 결과: 수정 필요(경미 2건, 동작 변경 없음)
- 범위: 새 파일 3개만. `__pycache__`는 `.gitignore` 대상. 포맷·컨트롤 설정을 앱에 넣지 않음(지시대로)
- `v4l2_capture.c`
  - 흐름: G_FMT(L60) → REQBUFS(L66) → 버퍼별 QUERYBUF·mmap(L77) → QBUF(L93) → STREAMON(L101) → DQBUF·출력·QBUF 반복(L105) → 마지막 프레임 저장(L132)
  - 해제는 STREAMOFF → munmap(매핑된 개수만, L151) → close → fclose로 역순이고, 실패 경로도 같은 정리를 거침
  - DQBUF index·bytesused 검사(L113), sequence 32bit 순환 처리(L122) 적절
  - 수정 1: `video_ioctl()`(L13)이 `perror("ioctl")`만 출력해 어느 ioctl이 실패했는지 알 수 없음. 보드 장애 때 바로 필요한 정보라 이름을 넘겨 출력하게 함
  - 수정 2: `main()` 안에 처리 단위 사이 빈 줄이 거의 없음(L47·L60·L72·L77·L93·L101 앞 등). AGENTS.md 코드 가독성 규칙
  - 170줄로 목표(150줄 안팎)를 넘지만, 오류 정리를 유지한 결과라 수용함. 수정 2로 늘어나는 줄도 수용함
- `raw_tool.py`: stride 패딩 제거(L19~20), 10bit 범위 검사(L21), black level 차감 후 채널 평균(L27), 원래 값 1023 기준 포화 비율(L33), 2×2 디베이어 PNG(L36). 지시와 일치. 수정 없음
- `Makefile`: `deploy`가 `app`을 선행함(지시에 없지만 형식상 무해). 수정 없음
- 미검증: WSL에 numpy가 없어 `raw_tool.py`는 실행하지 않았음(Pi에서 확인)

### 수정 지시 (재작업 1회차)

```text
docs/tasks/13-2-camera-ctrl.codex.md '리뷰 (Claude)'의 수정 2건만 examples/camera/v4l2_capture.c에 반영해 줘. 동작·출력 형식·처리 순서는 바꾸지 마.
1. video_ioctl()에 ioctl 이름 문자열을 인자로 추가하고, 실패 시 perror(이름)으로 출력해. 호출부는 "VIDIOC_G_FMT"처럼 해당 이름을 넘겨.
2. main() 안의 처리 단위 사이에 빈 줄을 넣어(입력 검사 / open·G_FMT / REQBUFS·calloc / QUERYBUF·mmap 루프 / QBUF 루프 / STREAMON / 캡처 루프 안의 DQBUF 검사·간격 계산·출력·저장·QBUF / 정리). 주석은 추가하지 마.
수정 전 변경 내용을 짧게 설명하고 승인받은 뒤 구현해. make, checkpatch, wc -l을 다시 실행하고 '구현 보고 (Codex)'에 "재작업 1회차" 항목으로 덧붙여. 보드 조작·커밋은 하지 마.
```

### 재작업 1회차 리뷰
- 결과: 통과
- 수정 1: `video_ioctl()`에 이름 인자 추가(L13), 실패 시 `perror(name)`(L19). 호출부 7곳 모두 해당 `VIDIOC_*` 이름을 넘김(L63·L70·L88·L106·L110·L120·L157·L165)
- 수정 2: 처리 단위 사이 빈 줄 추가(L48·L56·L62·L69·L76·L82·L100·L109·L127·L135·L142·L156·L160·L167·L175·L180·L185)
- 동작 변화 없음: ioctl 순서, 출력 형식, 정리 순서가 1차와 같음. checkpatch 0/0, 191줄(빈 줄 증가분, 수용)
- 수정 지시: 없음
