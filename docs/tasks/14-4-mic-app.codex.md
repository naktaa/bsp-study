# 14-4 alsa-lib 캡처 앱과 실시간 파형 — Codex 지시서

작업 노트: [14-4-mic-app.md](14-4-mic-app.md)

## 지시 (Claude)
- 선행 조건: 사용자가 WSL에 `libasound2-dev:arm64`를 설치함(노트 0단계). 없으면 구현 보고에 적고 멈춘다
- 변경 파일:
  - 새 파일 `examples/mic_app/mic_capture.c`
  - 새 파일 `examples/mic_app/mic_plot.py`
  - 새 파일 `examples/mic_app/Makefile`
  - `.gitignore`: `/examples/mic_app/mic_capture` 한 줄 추가
- 요구사항 — `mic_capture.c` (사용자 공간, alsa-lib):
  - 옵션(`getopt`): `-D 장치`(기본 `hw:CARD=inmp441,DEV=0`), `-r 레이트`(48000), `-p period 프레임`(1024), `-b buffer 프레임`(4096), `-g 정수 이득`(1, 1 이상), `-s 소비 지연 ms`(0), `-d 초`(5, 0이면 Ctrl+C까지), `-o 출력`(필수, 파일 경로면 WAV, `-`면 stdout 원시 샘플). 잘못된 값은 사용법 출력 후 종료
  - 고정 설정: `SND_PCM_STREAM_CAPTURE`, 블로킹, `SND_PCM_ACCESS_RW_INTERLEAVED`, `SND_PCM_FORMAT_S32_LE`, 2채널(Pi I2S 캡처 2채널 고정)
  - 레이트·period·buffer는 `snd_pcm_hw_params_set_rate_near`·`set_period_size_near`·`set_buffer_size_near`로 설정하고, `snd_pcm_hw_params()` 뒤 실제 값을 다시 읽어 stderr에 한 줄 출력: 요청 값과 협상 값, period·buffer의 ms 환산
  - 읽기 루프: `snd_pcm_readi()`로 period 프레임씩. `-EPIPE`면 xrun 횟수를 늘리고 stderr에 `[XRUN] n` 출력 후 `snd_pcm_recover()`. 그 밖의 음수는 `snd_pcm_recover()`가 실패하면 `snd_strerror()`와 함께 종료. 읽은 프레임이 요청보다 적은 경우도 그 수만큼 처리
  - 샘플 처리: 프레임마다 ch0만 사용(ch1 버림). `(int32_t)sample >> 8`로 24비트 값, 이득을 곱해 ±8388607로 클리핑
  - 1초(레이트만큼의 프레임)마다 stderr에 한 줄: 이득 적용 전 ch0 peak·RMS, 클리핑된 샘플 수, 누적 xrun
  - `-s`: 매 읽기 뒤 지정 ms만큼 잠들어 느린 소비자를 흉내 냄
  - WAV 출력: 모노, 레이트 그대로, 32비트 PCM. 값은 클리핑된 24비트 값 `<< 8`(왼쪽 정렬). 헤더를 먼저 쓰고 종료 때 데이터 크기를 다시 써서 고친다
  - stdout 출력(`-o -`): 클리핑된 24비트 값을 `int32_t` 리틀엔디언으로 그대로 쓰고 period마다 `fflush`. 파이프가 닫히면(`EPIPE`/`SIGPIPE`) 정상 종료
  - `SIGINT`: 플래그만 세우고 루프를 빠져나와 WAV 헤더를 마무리하고 장치를 닫는다
  - 해제: 모든 종료 경로에서 `snd_pcm_close`, 파일 닫기
  - AGENTS.md 코드 가독성(처리 단위 사이 빈 줄, 한글 주석은 필요한 곳만). 사용자 앱이므로 커널 스타일(탭 8, 함수 중괄호 다음 줄)에 맞춘다
- 요구사항 — `mic_plot.py` (Pi 데스크톱, python3 + matplotlib):
  - stdin에서 `int32` 리틀엔디언 모노 샘플을 읽는다. 읽기는 별도 스레드에서 하고 그리기는 `FuncAnimation`
  - 인자: `--rate`(48000), `--window-ms`(50, 파형 창), `--history-s`(10, 레벨 기록)
  - 위: 최근 `window-ms` 파형(y축 ±8388608 고정). 아래: 최근 `history-s` 동안 블록별 RMS
  - stdin이 끝나면 그리기를 멈추고, 창을 닫으면 종료(앞쪽 앱은 SIGPIPE로 끝남)
  - `examples/mpu6050_iio/imu_plot.py`의 구성 방식을 참고하되 IIO 관련 부분은 쓰지 않는다
- 요구사항 — `Makefile`: `examples/mpu6050_iio/Makefile`의 앱·deploy 부분 형식
  - `CROSS_COMPILE ?= aarch64-linux-gnu-`, `APP_CC ?= $(CROSS_COMPILE)gcc`, `APP_CFLAGS ?= -Wall -Wextra -O2`, 링크 `-lasound -lm`(필요한 것만)
  - 크로스 gcc가 multiarch 경로(`/usr/include/alsa`, `/usr/lib/aarch64-linux-gnu`)를 기본으로 찾는지 확인하고, 못 찾을 때만 `-I`/`-L`을 추가한다. 어느 쪽인지 구현 보고에 적는다
  - `all`(= app), `app`, `deploy`(`mic_capture`, `mic_plot.py` → `pi:bsp/apps/`), `clean`, `.PHONY`
- 하지 말 것: 드라이버·overlay·`examples/mic*/` 기존 파일 수정, mmap·poll·스레드(C 앱), 재생, 필터·FFT, 채널 선택·포맷 선택 옵션 추가, 보드 조작
- 빌드 확인 명령:

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic_app
make
file mic_capture
aarch64-linux-gnu-readelf -d mic_capture | grep NEEDED        # libasound.so.2 포함
aarch64-linux-gnu-readelf -V mic_capture | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1   # 2.35 이하
python3 -m py_compile mic_plot.py
git check-ignore -v mic_capture
make -n deploy
```

## 구현 보고 (Codex)
- 변경 요약:
  - 선행 조건 재확인: `libasound2-dev:arm64`·`libasound2:arm64` 모두 `install ok installed`, 버전 `1.2.6.1-1ubuntu1.2`. ALSA 헤더와 ARM64 링크 라이브러리 경로도 확인한 뒤 구현을 재개함.
  - `examples/mic_app/mic_capture.c`: 옵션 검사, 블로킹 ALSA 캡처 설정·협상 값 출력, 짧은 읽기 처리, xrun 집계·복구, ch0 변환·이득·클리핑, 1초 프레임 단위 통계, 소비 지연, WAV 또는 원시 stdout 출력과 종료·해제를 구현함.
  - `examples/mic_app/mic_plot.py`: 별도 stdin 읽기 스레드, 샘플 경계에 걸친 읽기 처리, 최근 파형·블록 RMS 기록, EOF 애니메이션 중지와 창 닫기 종료를 구현함. `examples/mpu6050_iio/imu_plot.py`의 `main()`·`FuncAnimation`·종료 처리 구성을 참고함.
  - `examples/mic_app/Makefile`: 지정된 컴파일 변수와 `all/app/deploy/clean`을 구성함. `examples/mpu6050_iio/Makefile`의 앱·배포 타깃 형식을 참고함. `.gitignore`에는 지정된 실행 파일 경로 한 줄만 추가함.
- 빌드 결과:
  - 아래 명령은 `examples/mic_app`에서 실행함.
  - `make`: 성공(종료 코드 0), `aarch64-linux-gnu-gcc -Wall -Wextra -O2 mic_capture.c -o mic_capture -lasound -lm`, 컴파일러 경고 없음.
  - `file mic_capture`: `ELF 64-bit LSB pie executable, ARM aarch64`, 동적 링커 `/lib/ld-linux-aarch64.so.1` 확인.
  - `aarch64-linux-gnu-readelf -d mic_capture | rg NEEDED`: `libasound.so.2`, `libm.so.6`, `libc.so.6`, `ld-linux-aarch64.so.1` 확인.
  - `aarch64-linux-gnu-readelf -V mic_capture | rg -o 'GLIBC_[0-9.]+' | sort -uV | tail -1`: `GLIBC_2.34`, 요구 기준 2.35 이하.
  - `python3 -m py_compile mic_plot.py`: 성공(종료 코드 0).
  - `git check-ignore -v mic_capture`: `.gitignore:9:/examples/mic_app/mic_capture` 적용 확인.
  - `make -n deploy`: `scp mic_capture pi:bsp/apps/`와 `scp mic_plot.py pi:bsp/apps/` 출력 확인. 실제 배포는 실행하지 않음.
  - 저장소 루트의 `git diff --check`: 성공. 추가 정적 컴파일 확인 `aarch64-linux-gnu-gcc -Wall -Wextra -O2 -fanalyzer -fsyntax-only examples/mic_app/mic_capture.c`: 진단 없이 종료 코드 0.
  - 크로스 GCC의 기본 헤더 검색 경로에 `/usr/include`, 라이브러리 검색 경로에 `/usr/lib/aarch64-linux-gnu`가 있음. 실제 빌드도 성공하여 별도 `-I`·`-L`은 추가하지 않음.
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f examples/mic_app/mic_capture.c`): 오류 0개, 경고 1개(종료 코드 1). `volatile sig_atomic_t stopped`에 대한 `Use of volatile is usually wrong` 경고는 유지함. 사용자 공간 SIGINT 핸들러와 메인 루프가 공유하는 시그널 플래그이므로 해당 타입이 필요하며, 커널 동시성 제어를 위해 `volatile`을 사용한 경우가 아님.
- 판단이 필요했던 부분:
  - `-d`는 단조 시계의 실제 경과 시간으로 판단함. 소비 지연·xrun 복구 시간도 포함하므로 지연 실험에서 녹음 샘플 수가 계속 늘어 실행 시간이 길어지는 구조를 피함. 종료 시점은 진행 중인 블로킹 읽기·소비 지연에 따라 늦어질 수 있음.
  - 이득 곱셈은 `int64_t`로 수행하고 ±8388607로 자름. WAV 왼쪽 정렬은 음수의 좌시프트가 미정의 동작이 되지 않도록 `uint32_t`로 변환한 뒤 시프트함. 일반 RIFF 헤더의 32비트 크기 범위를 넘는 WAV 출력은 오류로 종료하고 기록한 크기로 헤더를 마무리함.
  - SIGPIPE는 무시하여 stdout 쓰기·flush의 `EPIPE`를 정상 종료로 처리함. SIGINT 핸들러는 플래그만 바꾸고, 정상 정리 경로에서 WAV 헤더 보정과 파일·PCM 닫기를 수행함.
  - 파형·RMS의 시간축은 누적 수신 프레임 수와 `--rate`로 계산함. EOF에서는 마지막 수신 데이터까지 표시한 후 애니메이션만 멈추고, 창을 닫으면 종료함.
  - 작업 노트 5단계 (a)의 기본 버퍼·소비 지연 50ms에서 xrun 0이라는 예상은 보장할 수 없음. 48kHz에서 1024프레임은 약 21.3ms인데 매 읽기 뒤 50ms를 쉬면 장기 소비율이 생성률보다 낮아 4096프레임 버퍼도 결국 넘칠 수 있음. (a)·(b) 모두 실제 보드 출력으로 판단해야 하며, 작업 노트 본문은 수정하지 않음.
  - 보드 배포·실행, 5초 WAV 재생·이득 비교, xrun 발생·복구, SIGINT·파이프 종료의 실제 동작, Pi 데스크톱 그래프 반응은 미검증. 전체 커널·Yocto 빌드와 보드 조작은 실행하지 않음.

## 리뷰 (Claude)
- 결과: 통과
- 범위: 새 파일 3개(`examples/mic_app/`)와 `.gitignore` 한 줄. 드라이버·overlay·기존 예제·문서 변경 없음(`git status`). `__pycache__/`는 기존 무시 규칙에 걸림
- ALSA 설정: access·format·channels 고정 후 `_near` 3개 → `snd_pcm_hw_params()` → 실제 값 재조회·출력 순서가 맞음(`mic_capture.c` L156~203)
- 읽기 루프: `-EPIPE`에서 xrun 집계 후 `snd_pcm_recover()`, 복구 실패만 종료(L312~321). 캡처는 기본 start_threshold로 다음 `readi`가 다시 시작하므로 별도 `snd_pcm_start()`가 필요 없음. SIGINT는 `SA_RESTART` 없이 등록되어 블로킹 `readi`가 깨어나고 `stopped` 확인으로 빠져나옴(L314)
- 샘플 처리: ch0 `>> 8`, `int64_t` 이득·±8388607 클리핑, 음수 좌시프트 회피(L244~264). 통계는 이득 적용 전 값이라 지시와 같음
- 해제: `out`에서 WAV 헤더 보정 → `fclose` → `snd_pcm_close`. 열기·설정 실패 경로에서도 NULL 확인으로 안전함(L421~444). stdout 파이프 종료(`EPIPE`)는 정상 종료로 처리(L338, L434)
- `mic_plot.py`: 읽기 스레드 + 잠금 한 개 + `FuncAnimation`. 4096바이트 경계 처리, 창 닫기·EOF 종료 경로 있음
- checkpatch: 오류 0, 경고 1(`volatile sig_atomic_t`). 시그널 핸들러 공유 플래그는 C 표준상 이 타입이 맞으므로 유지가 맞음
- 구현 보고의 지적이 맞음: 노트 5단계 (a) `-s 50`은 period(21.3ms)보다 길어 소비가 생산보다 느리므로 buffer 크기와 관계없이 결국 xrun이 난다. 노트 5단계를 "period보다 짧은 지연 / 긴 지연 / 긴 지연 + 큰 buffer"로 고침
- 참고(수정 요구 아님): 옵션별 최댓값 표(L101~109), 협상 값 범위 재검사(L194~196), WAV 4GB 검사(L326~330)는 이 실습 값에서는 걸리지 않는 방어 코드라 읽을 때 건너뛰어도 됨
- 수정 지시: 없음
