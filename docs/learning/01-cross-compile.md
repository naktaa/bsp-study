# ARM64 사용자 프로그램 — 크로스 컴파일과 실행

현재 [hello.c](../../examples/hello/hello.c)를 읽으며 WSL에서 만든 ARM64 프로그램이 Pi에서 실행되는 과정을 익힌다. 현재 환경은 [bringup](../bringup.md), 실행 근거와 남은 확인은 [검증 현황](#검증-현황)에 둔다.

목차:

- [1. 구현 기능과 범위](#1-구현-기능과-범위)
- [2. 전체 실행 흐름](#2-전체-실행-흐름)
- [3. 코드 블록별 해설](#3-코드-블록별-해설)
- [4. 헷갈리기 쉬운 개념](#4-헷갈리기-쉬운-개념)
- [5. 빌드·전송·실행과 검증](#5-빌드전송실행과-검증)
- [6. 실제 트러블슈팅](#6-실제-트러블슈팅)

## 1. 구현 기능과 범위

인사말과 실행 장치의 아키텍처·커널 버전을 출력하는 작은 사용자 프로그램이다. 드라이버·장치 노드·커널 모듈을 만들지는 않는다. 별도 Makefile 없이 크로스 GCC 한 명령으로 컴파일과 링크를 수행한다. Make 규칙은 후속 [모듈 실습](02-kernel-module.md)과 [문자 디바이스 앱](03-char-device.md)에서 다룬다.

## 2. 전체 실행 흐름

```text
WSL x86_64에서 aarch64-linux-gnu-gcc 실행
  → ARM64용 컴파일·링크 → hello-arm64 생성
  → file로 대상 형식 확인 → Pi에 전송
Pi에서 hello-arm64 실행
  → 동적 로더가 libc 등 실행 환경 준비
  → main()에서 uname(&info) 호출
  → 실패: perror 출력, 종료 상태 1
  → 성공: 인사말·info.machine·info.release 출력, 종료 상태 0
```

컴파일러는 WSL에서 실행되지만 산출물은 Pi의 ARM64 환경에서 실행한다. 커널 버전은 빌드 당시 WSL 버전이 아니라 프로그램이 실행되는 시점의 시스템 정보를 읽는다.

## 3. 코드 블록별 해설

### 헤더·출력 버퍼·실패 처리

```c
#include <stdio.h>
#include <sys/utsname.h>

int main(void)
{
    struct utsname info;

    if (uname(&info) == -1) {
        perror("uname");
        return 1;
    }
```

stdio.h는 puts·printf·perror, sys/utsname.h는 struct utsname과 uname 선언을 제공한다. info는 결과를 받을 구조체 객체다. `&info`는 그 객체의 주소이며 uname은 해당 메모리에 결과를 채운다. 구조체를 값으로 넘기는 것과 포인터로 저장 위치를 알려주는 것을 구분한다.

uname은 성공 시 0, 실패 시 -1을 반환하고 errno로 원인을 제공한다. 실패한 결과를 읽지 않도록 perror로 원인을 출력하고 즉시 1을 반환한다. perror는 오류 메시지를 표준 오류로 출력한다. 여기서는 구조체를 선언할 때 0으로 채우는 대신 uname의 성공 여부를 먼저 확인해 필요한 필드를 사용한다.

### 성공 출력과 종료

```c
    puts("Hello, BSP!");
    printf("Running on: %s\n", info.machine);
    printf("Kernel: %s\n", info.release);
    return 0;
}
```

puts는 문자열 뒤에 줄바꿈을 붙이고, printf의 `%s`에는 문자열을 전달한다. `info.machine`과 `info.release`는 구조체의 멤버에 접근하는 C 문법이며 각각 실행 시스템의 아키텍처와 커널 release 문자열이다. 사용자 출력이므로 터미널에 나타나고 dmesg에 기록하는 방식은 아니다.

main의 0 반환은 셸에 정상 종료를 알린다. 셸의 `echo $?`는 바로 직전 명령의 상태를 보여주므로 프로그램 실행 직후 확인해야 한다.

## 4. 헷갈리기 쉬운 개념

| 구분 | 이번 실습에서의 의미 |
| --- | --- |
| 호스트 / 타깃 | 빌드하는 WSL x86_64 / 실행하는 Pi ARM64 |
| 일반 gcc / 크로스 GCC | 기본 호스트 코드를 만드는 도구 / Pi ARM64 코드를 만드는 도구 |
| 아키텍처 / 커널 버전 | 명령어·ABI 계열 / 실행 중인 Linux 커널의 release |
| 사용자 실행 파일 / .ko | 로더·libc와 함께 사용자 공간에서 실행 / 커널에 로드하는 모듈 |

ARM64라는 조건만으로 모든 실행 환경이 같아지는 것은 아니다. 동적으로 링크한 앱은 Pi의 동적 로더와 필요한 libc 심볼을 사용할 수 있어야 한다. 이 사용자 프로그램은 특정 Pi 커널의 Module.symvers나 빌드 헤더를 요구하지 않으며, 이후 커널 모듈의 버전 일치 조건과 구분한다.

file의 interpreter `/lib/ld-linux-aarch64.so.1`은 앱 실행에 필요한 ARM64 로더 경로다. PIE는 위치 독립 실행 파일 형식, debug_info는 디버깅 정보 포함을 뜻한다. WSL x86_64에서 ARM64 파일을 직접 실행하는 것은 이번 절차가 아니며 Pi에서 실행한다.

## 5. 빌드·전송·실행과 검증

### 선행 조건

- WSL2 Ubuntu 22.04.5, aarch64-linux-gnu-gcc 11.4.0·file·ssh·scp.
- Pi: ARM64 Debian 13, glibc 2.41, `/lib/ld-linux-aarch64.so.1`. 접속은 사용자 지정 SSH 별칭 pi.
- 저장소: `~/bspworkspace/bsp-study`. 커널 소스 트리는 이 앱 빌드의 선행 조건이 아니다.

현재 재현 절차는 접속 별칭 pi와 Pi의 `~/bsp/apps/`를 사용한다. 로컬 `/tmp/bsp-study`는 임시 빌드 경로이며 없으면 다시 만든다.

**WSL · 작업 디렉터리: 저장소 루트 · 크로스 컴파일러 준비 후**

```bash
cd ~/bspworkspace/bsp-study
mkdir -p /tmp/bsp-study
aarch64-linux-gnu-gcc -Wall -Wextra -O0 -g \
    examples/hello/hello.c -o /tmp/bsp-study/hello-arm64
echo $?
file /tmp/bsp-study/hello-arm64
```

`-Wall -Wextra`는 경고, `-O0`은 최적화 비활성, `-g`는 디버깅 정보 포함, `-o`는 산출물 경로다. 성공 상태 0, ARM aarch64와 로더 경로를 확인한 다음 전송한다.

**WSL · 작업 디렉터리: 저장소 루트 · 빌드 확인과 SSH 별칭 pi 준비 후**

```bash
cd ~/bspworkspace/bsp-study
ssh pi 'mkdir -p ~/bsp/apps'
scp /tmp/bsp-study/hello-arm64 pi:bsp/apps/hello-arm64
ssh pi
```

**Pi · 작업 디렉터리: `~/bsp/apps` · 전송 완료 후**

```bash
cd ~/bsp/apps
./hello-arm64
echo $?
```

현재 설정의 예상 아키텍처는 aarch64, 커널 줄은 실행 당시 uname -r과 같아야 한다. 아래 과거 로그의 버전을 현재 기대값으로 고정하지 않는다.

### 검증 현황

2026-11-02 사용자가 기존 배포판 커널에서 실행한 실제 결과:

```text
Hello, BSP!
Running on: aarch64
Kernel: 6.18.50+rpt-rpi-v8
0
```

| 근거 | 확인한 결과 |
| --- | --- |
| WSL file | ELF 64-bit LSB PIE·ARM aarch64·dynamically linked·debug_info, ARM64 interpreter |
| Pi 실행 | 인사말·aarch64·당시 커널 버전 출력, 종료 상태 0 |
| Pi 환경 | glibc 2.41과 ARM64 로더 경로 확인 |

당시에는 직접 IP 주소로 전송하고 Pi의 `/tmp/bsp-study-hello-arm64`에서 실행했다. 그 명령 전체를 중복 보존하지 않고 실행 근거와 경로 차이만 남긴다. 현재 커널은 `6.18.59-v8-bsp-study01`이며 이 커널에서 같은 앱을 재실행한 출력과 새 `~/bsp/apps/` 경로로 재현한 결과는 별도 미확인이다. 이번 문서 정리에서는 코드·빌드·전송·보드 상태를 변경하지 않았다. 별도 VEDA 사용자 hello 원본과의 비교는 수행하지 않아 차이를 추정해 기록하지 않는다.

## 6. 실제 트러블슈팅

최초 저장소 상위 `~/bspworkspace`에서 `examples/hello/hello.c`를 빌드하려 해 소스를 찾지 못했다. `bsp-study`로 이동한 뒤 같은 소스 상대 경로로 빌드해 해결했다. 뒤따른 file의 파일 부재도 컴파일 실패로 산출물이 없어서 발생한 결과였다. 위 재현 절차는 매 블록의 실행 디렉터리를 명시해 이 문제를 반영했다.
