# 첫 크로스 컴파일 실습

WSL2 Ubuntu 22.04.5 x86_64, aarch64-linux-gnu-gcc 11.4.0에서 빌드한 프로그램을 Raspberry Pi 4의 ARM64 Debian 13 환경에서 실행했다. 2026-11-02 사용자 제공 출력으로 확인.

## WSL에서 빌드·전송

```bash
cd ~/bspworkspace/bsp-study
mkdir -p /tmp/bsp-study
aarch64-linux-gnu-gcc -Wall -Wextra -O0 -g \
    examples/hello/hello.c -o /tmp/bsp-study/hello-arm64
file /tmp/bsp-study/hello-arm64
scp /tmp/bsp-study/hello-arm64 \
    nakta@192.168.45.224:/tmp/bsp-study-hello-arm64
```

`file`에서 ELF 64-bit, ARM aarch64, dynamically linked, interpreter `/lib/ld-linux-aarch64.so.1`, debug_info를 확인했다. 보드 IP가 바뀌면 전송 주소를 바꾼다.

## 보드에서 실행

```bash
/tmp/bsp-study-hello-arm64
echo $?
```

실제 관측:

```text
Hello, BSP!
Running on: aarch64
Kernel: 6.18.50+rpt-rpi-v8
0
```

보드의 glibc는 2.41이며 동적 로더 경로와 실행을 확인했다. `uname()`은 실행하는 장치의 아키텍처와 커널 정보를 반환한다. WSL 커널 버전이 출력되는 것이 아니다.

## 문제와 해결

최초 `~/bspworkspace`에서 상대 경로 `examples/hello/hello.c`를 사용해 소스를 찾지 못했다. `bsp-study` 디렉터리로 이동한 뒤 빌드 성공. `/tmp` 산출물은 삭제될 수 있으므로 필요하면 다시 빌드·전송한다.
