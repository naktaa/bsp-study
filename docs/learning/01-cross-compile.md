# ARM64 사용자 프로그램: WSL에서 크로스 컴파일하고 Pi에서 실행

## 1. 한눈에 보기
- **목적:** 호스트(WSL x86_64)에서 만든 실행 파일이 타깃(Pi ARM64)에서 도는 조건을 이해한다. 이후 모든 사용자 앱(char_app, mpu6050_app, devcheck)이 같은 방식으로 만들어진다.
- **기능:** 인사말과 실행 중인 시스템의 아키텍처·커널 버전(`uname`)을 출력한다. Makefile 없이 크로스 GCC 한 명령으로 컴파일·링크한다.
- **직접 작성 / 가져다 쓴 것:** [hello.c](../../examples/hello/hello.c)는 직접 작성했다. 크로스 GCC(`aarch64-linux-gnu-gcc` 11.4.0)와 Pi의 glibc·동적 로더는 배포판 것을 쓴다.
- **결과(실측):** Pi에서 `Running on: aarch64`, 종료 상태 0. 상세는 [6장](#6-트러블슈팅과-한계).
- **범위 밖:** 드라이버·커널 모듈(→ [02](02-kernel-module.md)), Make 규칙(→ [02](02-kernel-module.md)·[03](03-char-device.md)), 정적 링크.
- **관련 파일:** [hello.c](../../examples/hello/hello.c), 환경은 [bringup](../bringup.md)

## 2. 구조

**흐름:** WSL `aarch64-linux-gnu-gcc` → `hello-arm64`(ARM64 ELF) → `file`로 형식 확인 → `scp` → Pi 동적 로더 + libc → `main()` → `uname()` → 출력

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 호스트 | WSL2 Ubuntu 22.04.5, 크로스 GCC | x86_64에서 실행되며 ARM64 코드를 만든다 |
| 산출물 | `hello-arm64` | 동적 링크 ARM64 PIE, interpreter `/lib/ld-linux-aarch64.so.1` |
| 타깃 사용자 공간 | Debian 13, glibc 2.41, 동적 로더 | 실행 파일을 올리고 libc 심볼을 연결 |
| 타깃 커널 | `uname` 시스템 콜 | 실행 중인 커널의 `machine`·`release`를 채워 준다 |

## 3. 알아야 할 개념

### 왜 크로스 컴파일러가 따로 필요한가
일반 `gcc`는 자기가 도는 호스트(x86_64)용 코드를 만든다. Pi에서 돌릴 코드는 ARM64 명령어와 ABI로 만들어야 하므로 타깃 접두사가 붙은 `aarch64-linux-gnu-gcc`를 쓴다. 만든 파일은 WSL에서 바로 실행하지 않고 Pi로 보내서 실행한다.

### 왜 file로 먼저 확인하는가
`file` 출력의 `ARM aarch64`와 `interpreter /lib/ld-linux-aarch64.so.1`이 Pi에서 실행될 조건이다. 동적 링크 앱은 같은 ARM64라도 Pi에 그 로더와 필요한 libc 심볼이 있어야 돈다. 커널 모듈처럼 커널 버전·`Module.symvers`를 맞출 필요는 없다.

### 왜 출력된 커널 버전이 빌드 환경과 다른가
`uname()`은 빌드할 때가 아니라 **실행하는 순간** 시스템의 정보를 읽는다. 그래서 같은 바이너리가 배포판 커널에서는 `6.18.50+rpt-rpi-v8`, 커스텀 커널에서는 그 커널 버전을 출력한다.

## 4. 구현 흐름

### 4-1. uname 호출과 실패 처리

```c
    struct utsname info;

    if (uname(&info) == -1) {
        perror("uname");
        return 1;
    }
```

`&info`는 결과를 채울 구조체의 주소다. `uname`은 성공이면 0, 실패면 -1을 돌려주고 원인을 `errno`에 둔다. 실패하면 채워지지 않은 값을 읽지 않도록 `perror`로 원인을 표준 오류에 찍고 1로 끝낸다([L8](../../examples/hello/hello.c#L8)).

### 4-2. 출력과 종료 상태
`info.machine`(아키텍처)과 `info.release`(커널 release)를 `printf`로 출력하고 0을 반환한다([L13](../../examples/hello/hello.c#L13)). 셸의 `echo $?`는 바로 앞 명령의 종료 상태라서 실행 직후에 확인한다. 사용자 출력이라 터미널에 나오고 `dmesg`에는 남지 않는다.

## 5. 재현

<details><summary>빌드 → 전송 → 실행 (펼치기)</summary>

선행 조건: WSL에 `aarch64-linux-gnu-gcc`·`file`·`ssh`·`scp`, SSH 별칭 `pi`. 커널 소스는 필요 없다.

```bash
# [WSL] ~/bspworkspace/bsp-study — 빌드·형식 확인·전송
mkdir -p /tmp/bsp-study
aarch64-linux-gnu-gcc -Wall -Wextra -O0 -g examples/hello/hello.c -o /tmp/bsp-study/hello-arm64
file /tmp/bsp-study/hello-arm64     # ELF 64-bit LSB pie executable, ARM aarch64, ... interpreter /lib/ld-linux-aarch64.so.1
ssh pi 'mkdir -p ~/bsp/apps' && scp /tmp/bsp-study/hello-arm64 pi:bsp/apps/
```

```bash
# [Pi] ~/bsp/apps — 실행
./hello-arm64; echo $?              # Hello, BSP! / Running on: aarch64 / Kernel: <uname -r> / 0
```

`-Wall -Wextra`는 경고, `-O0`은 최적화 끄기, `-g`는 디버깅 정보다.

</details>

## 6. 트러블슈팅과 한계

**현재 버전 검증 (2026-11-02, 배포판 커널)**

| 항목 | 결과 |
| --- | --- |
| WSL `file` | ELF 64-bit LSB PIE, ARM aarch64, dynamically linked, debug_info, ARM64 interpreter |
| Pi 실행 | `Hello, BSP!`, `Running on: aarch64`, `Kernel: 6.18.50+rpt-rpi-v8`, 종료 상태 0 |
| Pi 환경 | glibc 2.41, `/lib/ld-linux-aarch64.so.1` |

- 미검증: 커스텀 커널 `6.18.59-v8-bsp-study01`에서 재실행, 별칭 `pi`·`~/bsp/apps` 경로로 재현(당시에는 IP 주소와 Pi `/tmp`를 씀).
- 한계: 동적 링크라 Pi의 glibc가 빌드 툴체인보다 오래되면 심볼 부족으로 실행이 실패할 수 있다(이 환경에서는 발생하지 않음).

**더 보기**
- PIE는? → 위치 독립 실행 파일. 로더가 임의 주소에 올릴 수 있다
- 사용자 실행 파일과 `.ko`의 차이는? → 앱은 로더·libc와 함께 사용자 공간에서 실행, `.ko`는 커널에 로드한다 ([02](02-kernel-module.md))
