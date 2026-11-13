# hello 커널 모듈 — 코드·Kbuild·로드와 해제

현재 [hello.c](../../examples/hello_module/hello.c)와 [Makefile](../../examples/hello_module/Makefile)을 기준으로 커널 모듈의 수명과 빌드·실행 경계를 설명한다. 과거 이름 bsp_hello의 실측은 [검증 현황](#검증-현황)에 분리했다.

목차:

- [1. 구현 기능과 범위](#1-구현-기능과-범위)
- [2. 전체 실행 흐름](#2-전체-실행-흐름)
- [3. 코드 블록별 해설](#3-코드-블록별-해설)
- [4. 헷갈리기 쉬운 개념과 질문](#4-헷갈리기-쉬운-개념과-질문)
- [5. VEDA 코드와의 차이](#5-veda-코드와의-차이)
- [6. 빌드·전송·로드·해제와 검증](#6-빌드전송로드해제와-검증)

## 1. 구현 기능과 범위

hello 모듈은 로드 시 loaded, 해제 시 unloaded 로그를 출력한다. 하드웨어·문자 장치 번호·`/dev` 노드를 등록하지 않고 read/write도 제공하지 않는다. 작은 예제로 Kbuild가 만든 .ko가 Pi 커널에 연결되는 과정을 익힌다. [사용자 프로그램 hello](01-cross-compile.md)의 main 실행과 구분한다.

## 2. 전체 실행 흐름

```text
WSL에서 make
  → 예제 Makefile이 Pi 커널 빌드 출력의 Kbuild 호출
  → ARM64 hello.ko 생성 → Pi 전송
Pi에서 insmod ./hello.ko
  → 커널이 모듈을 로드·연결하고 hello_init() 호출
  → loaded 로그, 0 반환으로 초기화 성공
Pi에서 rmmod hello
  → 커널이 hello_exit() 호출
  → unloaded 로그, 모듈 해제
```

앱처럼 .ko를 직접 실행하지 않고 커널에 로드한다. insmod가 끝난 뒤에도 모듈은 남아 있으므로 별도로 rmmod로 해제한다. dmesg는 코드 실행이 아니라 커널 로그를 조회하는 명령이다.

## 3. 코드 블록별 해설

<a id="c-코드의-핵심"></a>

### 헤더·초기화·종료 함수

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define DRIVER_AUTHOR "Nakta"
#define DRIVER_DESC "hello test driver"

static int __init hello_init(void) {
    pr_info("hello: loaded\n");
    return 0;
}

static void __exit hello_exit(void) {
    pr_info("hello: unloaded\n");
}
```

init.h는 초기화·종료 섹션 표시, module.h는 모듈 등록·메타데이터, printk.h는 로그 선언을 제공한다. DRIVER_AUTHOR·DRIVER_DESC는 뒤의 메타데이터 선언에 사용할 문자열 매크로다.

`hello_init(void)`는 인자 없이 호출되며 성공 시 0, 실패 시 음수 오류를 반환하는 초기화 함수다. 현재는 확보할 자원이 없어 로그 후 바로 0을 반환한다. 파일 범위 함수의 static은 이 이름을 다른 C 파일에서 직접 참조하지 못하도록 제한한다. 아래 매크로로 등록하면 커널이 호출할 수 있다.

`__init`는 초기화 코드 섹션을 지정한다. 성공적인 초기화 후 회수할 수 있는 코드이므로 이후 파일 연산 콜백처럼 계속 사용할 함수에 무분별하게 붙이지 않는다. `hello_exit(void)`는 인자·반환값 없이 해제 작업을 수행하고 `__exit`는 종료 코드 섹션을 표시한다. 현재는 자원이 없어 로그만 남긴다. 나중에 자원을 확보한다면 적절한 실패 정리와 정상 해제가 함께 필요하다.

### 커널에 함수와 메타데이터 연결

```c
module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
```

module_init·module_exit는 해당 함수를 로드·해제 지점으로 연결하는 매크로다. 이 줄에서 hello_init()을 일반 함수처럼 즉시 호출하는 것이 아니다. 사용자 프로그램의 main도 필요하지 않다.

MODULE_LICENSE는 라이선스 정보를, AUTHOR·DESCRIPTION은 modinfo로 확인할 작성자·설명을 남긴다. GPL 표기는 라이선스 메타데이터이며 out-of-tree 여부를 바꾸지는 않는다. 이 선언으로 `/dev` 노드가 생기는 것도 아니다.

### 로그 수준과 printk·pr_info·dev_info

현재 `pr_info("hello: ...")`는 INFO 수준이며 `printk(KERN_INFO ...)`에 대응한다. 수업의 KERN_NOTICE는 NOTICE라 같은 수준의 짧은 표현은 pr_notice다. pr_info가 NOTICE까지 포함한다는 뜻은 아니다.

pr_info/pr_err 등은 수준을 짧고 명확하게 표현하는 편의 매크로이고 printk도 유효하다. 디바이스 드라이버에서 항상 pr_info만 더 나은 것은 아니다. 실제 장치 객체가 있으면 dev_info/dev_err/dev_dbg로 장치 문맥을 함께 남기는 방식이 유용하다. 현재 예제는 장치 객체 없이 모듈 로그만 출력한다. 이 출력은 printf의 터미널 출력과 달리 커널 로그에서 조회한다.

<a id="makefile의-핵심"></a>

### Makefile과 Kbuild의 두 단계

```make
ifneq ($(KERNELRELEASE),)
obj-m := hello.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

.PHONY: all clean

all:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
endif
```

사용자가 예제 폴더에서 make를 실행하면 KERNELRELEASE가 설정되지 않은 바깥 호출에서 else 부분의 all이 동작한다. 레시피가 커널 빌드 디렉터리로 들어가고, Kbuild가 예제 Makefile을 다시 읽을 때는 KERNELRELEASE가 설정돼 obj-m 선언을 읽는다. `ifneq`는 이 두 문맥을 구분하는 Make 조건문이다. 조건문 없이 수업처럼 구성하는 방식도 가능하다.

`obj-m := hello.o`는 로드 가능한 모듈 대상으로 지정한다. Kbuild가 hello.c와 필요한 모듈 정보를 컴파일·연결해 hello.ko를 생성한다. 이름은 소스·대상 선언에서 결정되며 hello_init이라는 함수 이름으로 정해지는 것이 아니다.

| 변수·인자 | 역할 |
| --- | --- |
| `CURDIR` | 지금 예제 Makefile을 실행하는 디렉터리 |
| `abspath .../../../out/kernel-rpi4` | 예제에서 두 단계 올라가 저장소의 커널 빌드 출력 절대 경로 계산 |
| `KDIR ?=` | 위 경로를 기본값으로 사용하고 필요하면 명령줄에서 재정의 |
| `ARCH=arm64` | Pi 대상 아키텍처 |
| `CROSS_COMPILE=aarch64-linux-gnu-` | WSL에서 실행해 ARM64 코드를 만드는 도구 접두사 |
| `$(MAKE) -C "$(KDIR)"` | 커널 빌드 출력의 Makefile로 Kbuild 재귀 호출 |
| `M="$(CURDIR)"` | 외부 모듈 소스가 있는 현재 디렉터리 전달 |
| `LOCALVERSION=` | 커널 구축 때와 동일하게 추가 접미사를 비워 버전 조건 유지 |
| `modules`, `clean` | 외부 모듈 빌드 또는 산출물 정리 요청 |

커널 소스만 있는 경로가 아니라 .config·생성 헤더·심볼 정보를 가진 빌드 출력을 KDIR로 쓴다. WSL의 uname -r은 Pi 커널이 아니므로 이 경로 계산에 사용하지 않는다. `$(MAKE)`는 재귀 Make 호출에서 옵션·병렬 실행 정보가 이어지게 한다. 레시피 앞은 공백 대신 탭이다.

## 4. 헷갈리기 쉬운 개념과 질문

### Make 변수·기본 타깃

- `:=`는 Makefile을 읽을 때 오른쪽 값을 계산해 저장한다.
- `?=`는 아직 정의되지 않은 변수에만 기본값을 준다. 빈 값으로 정의된 변수도 덮어쓰지 않는다.
- 명령줄의 `make KDIR=...`는 일반적인 Makefile 대입보다 우선한다. 복잡한 변수를 지시하는 새 절차가 아니라 현재 기본값의 변경 방법이다.
- all과 수업의 default는 예약어가 아닌 일반 타깃 이름이다. 바깥 호출에서 처음 등장한 일반 타깃 all이 기본 선택된다.
- .PHONY는 all·clean이 같은 이름의 파일 유무로 생략되지 않게 한다. 실제 컴파일의 증분 판단까지 모두 무효화하는 것은 아니다.

<a id="2-산출물과-증분-빌드"></a>

### 산출물과 증분 빌드

| 파일 | 역할 |
| --- | --- |
| `hello.o` | 작성한 C 소스의 오브젝트 |
| `hello.mod`, `.mod.c`, `.mod.o` | 구성 목록·자동 생성 모듈 메타데이터와 그 오브젝트 |
| `.module-common.o` | vermagic 등 공통 모듈 정보 |
| `hello.ko` | Pi에 전송할 최종 모듈 |
| `modules.order` | 모듈 순서 정보 |
| `Module.symvers` | 이번 외부 모듈이 내보내는 심볼 정보 |
| `.*.cmd` | 컴파일 명령·의존성 추적 정보 |

소스·헤더·명령 조건이 그대로면 다시 make할 때 많은 CC/LD 단계를 생략한다. 필요한 부분만 다시 만드는 것이 증분 빌드다. make clean 뒤에는 예제 산출물이 없어져 다시 생성한다. clean은 외부 모듈 산출물 범위를 정리하며 커널 전체 출력을 지우지 않는다. 실제 명령은 아래 단일 재현 절차에 둔다.

### 버전·로드 도구·커널 내장

vermagic에는 커널 버전과 모듈 관련 설정 정보가 담긴다. 문자열이 같다는 사실만으로 모든 호환성을 보장하지는 않는다. 같은 소스·.config·생성 헤더·커널 Module.symvers와 호환 툴체인을 사용해야 한다. 커널 Module.symvers는 전체 빌드의 산출물이며 modules_prepare만으로 생성되지 않는다. 예제 폴더의 Module.symvers와 커널 출력의 Module.symvers는 대상이 다르다.

insmod는 지정한 .ko 경로를 로드하고 rmmod는 로드된 모듈 이름으로 해제한다. modprobe는 보통 `/lib/modules/<실행버전>`의 설치·의존성 정보를 사용한다. 이 실습은 의존 모듈 없는 별도 파일을 직접 로드하므로 insmod를 사용한다. 커널 설정 =y는 내장, =m은 별도 모듈로 빌드하는 선택이다.

lsmod의 크기는 파일 크기와 같은 의미가 아닌 모듈 메모리 크기이고 마지막 숫자는 사용 참조 수다. out-of-tree taint는 외부에서 빌드한 모듈을 로드했다는 표시라 그 메시지 자체는 로드 실패가 아니다. 모듈을 해제해도 같은 부팅 동안 이 표시는 남는다.

## 5. VEDA 코드와의 차이

참고 저장소 커밋 `c2f2ace95af528c6f806c8fc273f02bdb10120c6`의 [hello_module.c](../../../VEDA/리눅스커널/수업코드/kmodule/hello_module.c)와 [Makefile](../../../VEDA/리눅스커널/수업코드/kmodule/Makefile)을 읽기 비교했다.

| 확인한 수업 코드 | 현재 코드·이유 |
| --- | --- |
| init/exit 함수와 module_init/module_exit | 같은 구조 유지. 이름·중괄호 표현 차이는 실행 구조를 바꾸지 않음 |
| printk(KERN_NOTICE ...) | pr_info로 INFO 로그 사용. 수준 차이는 위 로그 해설 참고 |
| kernel.h 포함 | 로그 의도를 직접 드러내는 printk.h 포함 |
| module_exit 아래 단독 문자 i | 유효한 C 문장이 아니므로 현재 구현에 포함하지 않음 |
| KDIR=/usr/src/linux | 사용자 소스 경로와 분리한 out/kernel-rpi4 빌드 출력 참조 |
| default 타깃에서 make 직접 호출 | all·KERNELRELEASE 조건 분기·$(MAKE)로 호출 문맥 구분 |
| 고정된 빌드 인자·정리 명령 | ARCH/CROSS_COMPILE 기본값과 동일한 버전 인자를 빌드·clean에 전달 |

## 6. 빌드·전송·로드·해제와 검증

### 선행 조건

WSL에 [커널 구축 문서](07-kernel-build.md)의 전체 빌드 출력이 남아 있어야 한다. 소스 커밋은 `b3495ea401d709af0701a85babcba5e3d788a602`, 설정은 bcm2711_defconfig·LOCALVERSION=-v8-bsp-study01·LOCALVERSION_AUTO=n이며 명령의 LOCALVERSION=을 유지한다. GCC 11.4.0 ARM64 크로스 도구·Make·file·modinfo(kmod)·ssh·scp가 필요하다.

Pi는 `6.18.59-v8-bsp-study01`로 부팅하고 SSH 별칭 pi와 sudo·kmod·dmesg를 사용할 수 있어야 한다. 현재 배포 기준에 따라 모듈은 `~/bsp/modules/`에 둔다.

### 1) 빌드와 증분·정리 확인

**WSL · 작업 디렉터리: hello 모듈 예제 · 위 커널 빌드 환경 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/hello_module
make
echo $?
file hello.ko
modinfo -F vermagic ./hello.ko
```

종료 상태 0·ARM aarch64 relocatable·대상 커널과 같은 vermagic을 확인한다. 빌드가 실패하면 전송으로 넘어가지 않는다. 증분·clean 차이를 복습하려는 경우에만 다음을 수행한다.

**WSL · 작업 디렉터리: hello 모듈 예제 · 최초 빌드 성공 후, 증분 동작 복습 시**

```bash
cd ~/bspworkspace/bsp-study/examples/hello_module
make
make clean
make
```

첫 명령은 변경이 없으면 대부분 생략하고, 마지막 명령은 clean 후 모듈을 다시 만든다. 따라서 다음 전송에 사용할 파일은 마지막 make 성공으로 준비된다.

### 2) 전송과 버전 대조

**WSL · 작업 디렉터리: hello 모듈 예제 · 빌드 확인과 SSH 별칭 pi 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/hello_module
ssh pi 'mkdir -p ~/bsp/modules'
scp hello.ko pi:bsp/modules/hello.ko
ssh pi
```

**Pi · 작업 디렉터리: `~/bsp/modules` · 전송 완료 후**

```bash
cd ~/bsp/modules
uname -r
modinfo -F vermagic ./hello.ko
```

버전 부분이 일치하는지 확인한다. 이미 hello가 로드돼 있다면 먼저 해제하고 새 파일을 로드한다. 파일 전송만으로 로드된 코드가 바뀌지는 않는다.

### 3) 로드·로그·해제

**Pi · 작업 디렉터리: `~/bsp/modules` · 대상 버전 일치와 hello 미로드 상태 확인 후**

```bash
cd ~/bsp/modules
sudo insmod ./hello.ko
lsmod | grep '^hello '
sudo dmesg | grep -F 'hello:'
```

현재 코드의 예상 로그는 `hello: loaded`다. 과거 다른 모듈 이름의 이력과 구분해 이번 시점의 로그를 확인한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · hello 정상 로드 상태에서 해제**

```bash
cd ~/bsp/modules
sudo rmmod hello
lsmod | grep '^hello '
sudo dmesg | grep -F 'hello:'
```

lsmod에 항목이 없어지고 `hello: unloaded`가 추가돼야 한다. grep이 항목을 찾지 못하면 종료 상태 1이므로 모듈 해제 실패와 혼동하지 않는다. 로그는 이력이므로 이전 loaded도 남는다.

### 검증 현황

2026-11-03 사용자 실측은 이름 변경 전 bsp_hello 버전이다.

| 근거 | 당시 결과 |
| --- | --- |
| WSL 빌드·file | 종료 상태 0, ARM aarch64 relocatable |
| vermagic | 6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64 |
| 증분·clean | 수정 없는 make의 생략, clean 뒤 재빌드 확인 |
| 모듈 정보 | 외부 모듈의 Module.symvers 0바이트. 내보내는 심볼이 없는 예제 |
| Pi 로드·해제 | lsmod 등록·제거, loaded/unloaded 로그 확인 |

대표 과거 로그:

```text
bsp_hello              12288  0
[12843.102465] bsp_hello: loading out-of-tree module taints kernel.
[12843.104640] bsp_hello: loaded
[12932.342295] bsp_hello: unloaded
```

현재 hello라는 소스·모듈·로그 이름으로 변경한 뒤의 빌드·로드·해제 결과는 별도 미확인이다. 과거 전송은 직접 IP 주소·Pi의 /tmp 경로였고 위 절차는 현재 별칭 pi·~/bsp/modules 기준으로 정리한 안내다. 이 경로 변경의 재실행 결과도 별도로 받지 않았다. 이번 문서 정리는 명령·링크·코드 발췌만 정적으로 대조했으며 실제 빌드·로드를 수행하지 않았다.

버전 불일치·전송 파일 부재는 실패 시 확인할 항목이지만 이 예제에서 실제 발생한 장애 사례로 기록하지 않는다. 실패하면 insmod 오류와 커널 로그를 함께 확인하고 대상 커널·KDIR·설정을 대조한다.

공식 참고: [외부 모듈 빌드](https://docs.kernel.org/kbuild/modules.html), [커널 로그](https://docs.kernel.org/core-api/printk-basics.html), [커널 taint](https://docs.kernel.org/admin-guide/tainted-kernels.html).
