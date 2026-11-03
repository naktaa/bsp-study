# hello 커널 모듈 복습·재현

2026-11-03 사용자가 WSL 크로스 빌드·증분 빌드·clean 및 Pi 로드·해제를 직접 확인했다. 기존 [사용자 프로그램 hello](../hello/README.md)와 달리 `.ko`는 커널에 로드하는 코드이며 실행 파일처럼 직접 실행하지 않는다.

## 목적·핵심 개념

목표는 같은 커널 빌드 환경으로 만든 모듈의 초기화·정리 함수가 Pi에서 실행되는 것을 확인하는 것이다.

### 선행 조건

- [커널 빌드 문서](../../docs/kernel-build.md)의 소스·설정·전체 빌드가 WSL에 남아 있어야 한다.
- 소스 커밋은 `b3495ea401d709af0701a85babcba5e3d788a602`, 출력은 저장소의 `out/kernel-rpi4`이다.
- 설정은 `bcm2711_defconfig`, LOCALVERSION=-v8-bsp-study01, LOCALVERSION_AUTO=n. 빌드 시 추가 `LOCALVERSION=` 유지.
- WSL은 ARM64 GCC 11.4.0·Make·file·modinfo(kmod), Pi는 sudo·insmod·rmmod·lsmod·dmesg 사용 가능.
- Pi에서 실행 중인 커널은 `6.18.59-v8-bsp-study01`이어야 한다. 주소는 현재 `nakta@192.168.45.224`이며 변경 시 scp·ssh 주소도 바꾼다.
- 명령 블록마다 실행 환경과 cd를 명시했다. 이전 터미널의 임시 변수는 필요 없다. 실제 수정·빌드·실행은 사용자가 한다.

### C 코드의 핵심

원본은 [bsp_hello.c](bsp_hello.c)다. 하드웨어나 `/dev` 장치를 등록하지 않고 로그만 출력한다.

| 요소 | 의미 |
| --- | --- |
| `hello_init()`와 `module_init()` | 로드 시 호출할 함수 등록. 0 반환은 초기화 성공 |
| `hello_exit()`와 `module_exit()` | 해제 시 호출할 정리 함수 등록 |
| `__init`, `__exit` | 초기화·해제 코드에 대한 커널 섹션 표시 |
| `pr_info()` | INFO 수준의 커널 로그. 터미널 출력과 구분 |
| `MODULE_LICENSE/AUTHOR/DESCRIPTION` | 모듈 라이선스·작성자·설명 메타데이터 |

[수업 hello_module.c](../../../VEDA/리눅스커널/수업코드/kmodule/hello_module.c)와 init/exit 구조는 같다. 차이와 이유:

- `printk(KERN_NOTICE ...)`는 NOTICE, 현재 pr_info는 INFO다. 같은 수준의 짧은 표현은 pr_notice이며 pr_info는 printk(KERN_INFO ...)에 대응한다. 기존 printk도 유효하다.
- 로그 기능을 직접 나타내는 `<linux/printk.h>`를 포함하고, 모듈 이름을 로그에 넣어 검색하기 쉽게 했다.
- 수업 소스의 단독 `i`는 문법 오류이므로 사용하지 않았다. 작성자·함수명·중괄호 등 표현 차이는 로드 구조를 바꾸지 않는다.
- 실제 장치에 연결된 드라이버에서는 dev_info/dev_err/dev_dbg처럼 장치를 식별하는 로그를 검토한다. 지금 예제에는 장치 객체가 없다.

### Makefile의 핵심

원본은 [Makefile](Makefile)이며 소스 수정 없이 빌드 도구만 설정한다.

| 요소 | 의미 |
| --- | --- |
| `ifneq ($(KERNELRELEASE),)` | Kbuild가 읽을 때의 obj-m 부분과 사용자가 make할 때의 편의 규칙 분리. 수업의 조건문 없는 구성도 가능 |
| `obj-m := bsp_hello.o` | Kbuild에 모듈 대상 지정. 최종 산출물은 bsp_hello.ko |
| `KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)` | 현재 모듈 폴더에서 저장소 루트로 두 단계 올라가 커널 출력 선택 |
| `ARCH ?= arm64`, `CROSS_COMPILE ?= aarch64-linux-gnu-` | ARM64 크로스 빌드 기본값 |
| `all`, `clean`, `.PHONY` | 빌드·정리 작업. 일반 파일 타깃과 구분. 첫 일반 타깃 all이 기본이며 default라는 이름도 특별한 예약어는 아님 |
| `$(MAKE) -C ... M=...` | 커널 빌드 시스템 호출 및 외부 모듈 경로 전달. 단순 gcc 호출이 아님 |
| `LOCALVERSION=` | 커널을 빌드할 때와 같은 버전 조건 전달 |

`:=`는 읽는 시점에 값을 계산해 저장하고 `?=`는 아직 정의되지 않았을 때 기본값을 준다. ?=는 빈 값으로 정의된 변수도 덮어쓰지 않는다. 보통 명령줄 변수는 :=로 작성한 일반 대입에도 우선한다. 레시피 들여쓰기는 탭이다.
[수업 Makefile](../../../VEDA/리눅스커널/수업코드/kmodule/Makefile)은 `/usr/src/linux`를 고정 KDIR로 사용했지만 여기서는 소스와 분리한 **커널 빌드 출력**을 사용한다. WSL의 uname -r은 Pi 커널이 아니므로 기본 KDIR 계산에 사용하지 않는다.

## 실행 순서

### 1. WSL 빌드·메타데이터 확인

**WSL · 작업 디렉터리: 모듈 폴더**

```bash
cd ~/bspworkspace/bsp-study/examples/hello_module
make
echo $?
file bsp_hello.ko
modinfo ./bsp_hello.ko
```

성공 기준은 종료 상태 0, file의 ARM aarch64 형식, Pi 실행 버전과 일치하는 vermagic이다.

버전 문자열만 같다고 모든 호환성을 보장하는 것은 아니다. 같은 커널 소스·.config·생성 헤더·Module.symvers와 호환 도구로 빌드하고 실제 로드까지 확인한다. `out/kernel-rpi4`의 Module.symvers는 전체 커널 빌드의 산출물이며 modules_prepare만으로 생기지 않는다.

### 2. 산출물과 증분 빌드

| 파일 | 역할 |
| --- | --- |
| `bsp_hello.o` | 작성한 C 소스의 오브젝트 |
| `bsp_hello.mod`, `.mod.c`, `.mod.o` | 구성 오브젝트 목록, 자동 생성 모듈 정보와 그 오브젝트 |
| `.module-common.o` | vermagic 등 공통 모듈 정보 |
| **`bsp_hello.ko`** | Pi에 전송할 최종 모듈 |
| `modules.order`, `Module.symvers` | 모듈 순서와 이번 모듈이 내보내는 심볼 정보. 이 예제의 Module.symvers는 0바이트 |
| `.*.cmd` | 명령·의존성 추적. 소스·헤더·옵션 변경 시 재빌드 판단에 사용 |

**WSL · 작업 디렉터리: 모듈 폴더**

```bash
cd ~/bspworkspace/bsp-study/examples/hello_module
make
make clean
make
```

첫 명령은 기존 빌드 후 수정이 없으면 CC/LD 대부분을 생략하는 증분 빌드다. clean 후 make는 모듈을 다시 빌드한다. 현재 clean은 이 외부 모듈의 산출물만 지우며 커널 전체 빌드 디렉터리를 지우지 않는다. 모두 사용자 실행으로 확인했다.

### 3. 전송·커널 버전 비교

**WSL · 작업 디렉터리: 모듈 폴더**

```bash
cd ~/bspworkspace/bsp-study/examples/hello_module
scp bsp_hello.ko nakta@192.168.45.224:/tmp/bsp_hello.ko
ssh nakta@192.168.45.224
```

**Pi · 작업 디렉터리: 홈**

```bash
cd ~
uname -r
modinfo -F vermagic /tmp/bsp_hello.ko
```

두 출력의 버전 부분이 `6.18.59-v8-bsp-study01`인지 확인 후 로드한다. 다르면 멈추고 WSL의 KDIR와 Pi에서 선택한 커널을 확인한다.

### 4. 로드·로그·해제

**Pi · 작업 디렉터리: 홈**

```bash
cd ~
sudo insmod /tmp/bsp_hello.ko
lsmod | grep '^bsp_hello'
sudo dmesg | grep 'bsp_hello:'
```

`insmod`는 파일 경로의 모듈을 로드한다. 이미 로드되어 있으면 중복 insmod하지 않는다. 이 예제는 의존 모듈이 없고 /tmp의 파일로 실습하므로 insmod를 사용했다. modprobe는 일반적으로 `/lib/modules/<실행버전>`의 설치·의존성 정보를 이용해 이름으로 의존 모듈까지 처리한다. 커널 내장(=y)은 별도 .ko 로드 없이 동작하고 모듈(=m)은 로드 가능한 .ko가 된다.

**Pi · 작업 디렉터리: 홈**

```bash
cd ~
sudo rmmod bsp_hello
lsmod | grep '^bsp_hello'
sudo dmesg | grep 'bsp_hello:'
```

rmmod는 파일 경로가 아닌 모듈 이름을 받는다. dmesg는 이력이므로 loaded도 남는다. 사용자 프로그램 종료처럼 자동으로 해제되는 것은 아니다.

## 검증 결과

- 사용자 빌드 종료 상태 0, ARM aarch64 relocatable 형식 확인. 커널이 로드·연결하는 모듈이므로 사용자 실행 파일과 형식이 다르다.
- vermagic: `6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64`.
- clean·재빌드 및 수정 없는 make의 작업 생략은 사용자 직접 실행으로 확인했다.

실제 관측:

```text
bsp_hello              12288  0
[12843.102465] bsp_hello: loading out-of-tree module taints kernel.
[12843.104640] bsp_hello: loaded
```

12288은 lsmod가 보고하는 모듈 메모리 크기로 파일 크기와 다르며 0은 사용 참조 수다. out-of-tree taint는 외부에서 빌드한 모듈 로드 이력이며 이 메시지 자체는 로드 실패가 아니다. 해제 후에도 같은 부팅 동안 taint는 남는다.

해제 후 lsmod 검색 결과는 없었고 `[12932.342295] bsp_hello: unloaded`가 추가됐다. 코드·버전·로드·초기화 로그·해제까지 확인했으며 센서·문자 디바이스 기능은 아직 없다.

## 관련 트러블슈팅

- 버전 불일치: 다른 버전 커널로 부팅했다면 현재 .ko를 그대로 사용하지 않는다. .config나 커널을 바꾸면 대상 환경과 산출물을 다시 맞춘다.
- 전송 파일 없음: `/tmp` 파일은 재부팅 후 없어질 수 있다. 새 세션에서 필요하면 빌드 결과를 다시 전송한다.
- 실패 시 insmod 오류와 커널 로그를 함께 확인한다. 외부 모듈 taint는 위 관측처럼 정상 로드에도 나타난다. 성공 로그를 예상값만으로 기록하지 않는다.

## 참고

- [외부 모듈 빌드](https://docs.kernel.org/kbuild/modules.html)
- [커널 로그](https://docs.kernel.org/core-api/printk-basics.html)
- [외부 모듈 taint](https://docs.kernel.org/admin-guide/tainted-kernels.html)
