# 첫 ARM64 사용자 프로그램 실습

## 목적·핵심 개념

WSL x86_64에서 ARM64 실행 파일을 만들어 Pi에서 실행하는 크로스 컴파일을 익힌다. [hello.c](hello.c)는 uname()으로 **실행하는 장치**의 아키텍처·커널 정보를 출력한다. 사용자 프로그램은 보드의 동적 로더·glibc를 사용하며 커널 모듈과 다르다.

선행 조건: WSL2 Ubuntu 22.04.5, aarch64-linux-gnu-gcc 11.4.0·file·scp·ssh, Pi의 ARM64 Debian 13·glibc 2.41·`/lib/ld-linux-aarch64.so.1`. 현재 SSH 주소는 `nakta@192.168.45.224`이며 변경 시 바꾼다. 소스는 이 저장소의 examples/hello/hello.c로, 커널 소스 트리는 필요 없다.

## 실행 순서

각 블록에 cd를 포함해 이전 터미널의 위치·변수에 의존하지 않는다. 초기 실습 명령을 유지했다.

**WSL · 작업 디렉터리: `~/bspworkspace/bsp-study`**

```bash
cd ~/bspworkspace/bsp-study
mkdir -p /tmp/bsp-study
aarch64-linux-gnu-gcc -Wall -Wextra -O0 -g \
    examples/hello/hello.c -o /tmp/bsp-study/hello-arm64
file /tmp/bsp-study/hello-arm64
scp /tmp/bsp-study/hello-arm64 \
    nakta@192.168.45.224:/tmp/bsp-study-hello-arm64
ssh nakta@192.168.45.224
```

`-Wall -Wextra`는 경고, `-O0 -g`는 최적화 없이 디버깅 정보 포함, `-o`는 출력 위치다. file에서 ELF 64-bit·ARM aarch64·동적 로더 경로를 확인한다.

**Pi · 작업 디렉터리: 홈, 전송 완료 후**

```bash
cd ~
/tmp/bsp-study-hello-arm64
echo $?
```

## 검증 결과

2026-11-02 사용자 실행 출력이다. **당시 기존 배포판 커널**을 사용한 결과이며 현재 커널과 구분한다.

```text
Hello, BSP!
Running on: aarch64
Kernel: 6.18.50+rpt-rpi-v8
0
```

file에서 ELF 64-bit LSB PIE·ARM aarch64·dynamically linked·interpreter `/lib/ld-linux-aarch64.so.1`·debug_info를 확인했다. 현재 커널은 `6.18.59-v8-bsp-study01`이므로 지금 다시 실행하면 Kernel 줄은 현재 실행 환경을 반영해야 한다. 새 커널에서 hello를 다시 실행한 출력은 아직 확인하지 않았다. 현재 환경은 [bringup](../../docs/bringup.md) 참고.

## 관련 트러블슈팅

- 소스 경로 오류: 최초 `~/bspworkspace`에서 `examples/hello/hello.c`를 사용해 파일을 찾지 못했다. `bsp-study`로 이동 후 같은 명령으로 빌드 성공했다. file의 후속 오류도 실행 파일 미생성 때문이었다.
- `/tmp` 산출물은 없어질 수 있다. 필요하면 위 WSL 빌드·전송을 다시 수행한다.
- 새 환경에서는 ARM64라는 조건 외에 보드의 로더·libc 호환성도 확인한다. 이 실습에서는 실제 보드 실행과 종료 상태 0까지 확인했다.
