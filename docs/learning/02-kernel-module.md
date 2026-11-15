# 커널 모듈: hello.ko를 Kbuild로 만들어 Pi 커널에 올리고 내리기

## 1. 한눈에 보기
- **목적:** 외부 모듈(out-of-tree)이 대상 커널의 빌드 출력에 맞춰 만들어지고, `insmod`·`rmmod`로 커널에 연결·해제되는 과정을 이해한다. 뒤의 모든 드라이버 예제가 이 Makefile 구조를 그대로 쓴다.
- **기능:** 로드할 때 `hello: loaded`, 해제할 때 `hello: unloaded`를 커널 로그에 남긴다. 장치·`/dev` 노드·read/write는 없다.
- **직접 작성 / 가져다 쓴 것:** [hello.c](../../examples/hello_module/hello.c)와 [Makefile](../../examples/hello_module/Makefile)은 직접 작성했다. Kbuild, 모듈 로더(kmod의 `insmod`·`rmmod`), `printk`는 커널 것을 쓴다.
- **결과(실측):** 6.18.59-v8-bsp-study01과 같은 vermagic의 `.ko`가 Pi에서 로드·해제되고 loaded·unloaded 로그가 남았다(당시 이름 `bsp_hello`). 상세는 [6장](#6-트러블슈팅과-한계).
- **범위 밖:** 장치 등록, 심볼 export, `modprobe`용 설치(`modules_install`).
- **관련 파일:** [hello.c](../../examples/hello_module/hello.c), [Makefile](../../examples/hello_module/Makefile), 커널 빌드 출력은 [07 문서](07-kernel-build.md)

## 2. 구조

**빌드·실행 흐름:** WSL `make` → 커널 빌드 출력(`out/kernel-rpi4`)의 Kbuild 재귀 호출 → ARM64 `hello.ko` → Pi 전송 → `insmod` → `hello_init()` → `rmmod` → `hello_exit()`

```text
 +----------------+  make -C KDIR M=$PWD  +----------------+  scp   +-----------+  insmod / rmmod  +-------------+
 | hello_module/  |---------------------->| out/kernel-rpi4|------->| hello.ko  |----------------->| hello_init  |
 | Makefile       |<----------------------| Kbuild         |        | on Pi     |                  | hello_exit  |
 +----------------+  reads obj-m again    +----------------+        +-----------+                  +-------------+
```

앱처럼 `.ko`를 실행하지 않고 커널에 올린다. insmod가 끝나도 모듈은 커널에 남아 있고, rmmod로 따로 내린다.

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| WSL 빌드 | 예제 Makefile, Kbuild | 바깥 호출 → 커널 Makefile → 예제 Makefile 재읽기(`obj-m`) → `hello.ko` |
| 커널 빌드 출력 | `out/kernel-rpi4` | `.config`·생성 헤더·`Module.symvers` 제공 → vermagic·심볼 버전 결정 |
| Pi 커널 | 모듈 로더 | vermagic·심볼 확인 후 연결, `module_init` 함수 호출 |
| 직접 작성 모듈 | `hello.ko` | init·exit에서 로그 |

## 3. 알아야 할 개념

### 왜 Makefile이 두 번 읽히는가
예제 폴더에서 `make`를 치면 `KERNELRELEASE`가 비어 있는 **바깥 호출**이라 `else` 쪽 `all`이 돈다. `all`은 `$(MAKE) -C "$(KDIR)" M="$(CURDIR)" modules`로 커널 Makefile에 들어가고, Kbuild가 `M` 디렉터리의 Makefile을 다시 읽을 때는 `KERNELRELEASE`가 정해져 있어 `obj-m := hello.o`만 읽는다([Makefile L1](../../examples/hello_module/Makefile#L1)). `ifneq`가 이 두 문맥을 나눈다.

### 왜 커널 소스가 아니라 빌드 출력을 KDIR로 쓰는가
모듈은 실행 중인 커널과 같은 설정·헤더·심볼 버전으로 만들어야 로드된다. 이 정보는 소스 트리가 아니라 전체 빌드 출력(`.config`, `include/generated`, `Module.symvers`)에 있다. 그래서 `KDIR`은 `out/kernel-rpi4`이고, 커널을 만들 때처럼 `LOCALVERSION=`(빈 값)을 넘겨 버전 문자열을 맞춘다([L4](../../examples/hello_module/Makefile#L4)). WSL의 `uname -r`은 Pi 커널이 아니므로 쓰지 않는다.

### 왜 vermagic을 먼저 대조하는가
`modinfo -F vermagic`의 버전 부분이 Pi의 `uname -r`과 다르면 insmod가 거부한다. 다만 문자열이 같다고 모든 호환이 보장되지는 않는다. 같은 소스·`.config`·`Module.symvers`·호환 툴체인으로 만들어야 하고, 커널 `Module.symvers`는 `modules_prepare`가 아니라 전체 빌드에서만 생긴다.

### 왜 module_init으로 함수를 연결하는가
모듈에는 `main`이 없다. `module_init(hello_init)`은 "로드할 때 이 함수를 불러라"를 등록할 뿐이고 그 줄에서 호출하지 않는다([hello.c L17](../../examples/hello_module/hello.c#L17)). init은 성공이면 0, 실패면 음수 오류를 돌려주고, 음수면 로드가 실패한다. `__init`은 초기화 뒤 회수되는 섹션이라 계속 쓸 콜백에는 붙이지 않는다.

### 왜 insmod를 쓰고 modprobe를 쓰지 않는가
`insmod`는 지정한 `.ko` 경로를 그대로 올리고, `modprobe`는 `/lib/modules/<버전>`에 설치된 모듈과 의존성 정보를 쓴다. 이 실습은 의존 모듈이 없는 파일을 `~/bsp/modules`에서 바로 올리므로 insmod를 쓴다. 의존 모듈이 있는 경우(예: [09 IIO](09-iio-direct.md)의 `industrialio_triggered_buffer`)는 먼저 `modprobe`로 올려야 한다.

## 4. 구현 흐름

### 4-1. 코드: init·exit와 메타데이터

```c
static int __init hello_init(void) {
    pr_info("hello: loaded\n");
    return 0;
}

static void __exit hello_exit(void) {
    pr_info("hello: unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
```

- `static`은 이 이름을 다른 C 파일에서 못 보게 할 뿐이다. 매크로로 등록하면 커널은 호출할 수 있다.
- `MODULE_LICENSE`·`MODULE_AUTHOR`·`MODULE_DESCRIPTION`은 `modinfo`로 보이는 메타데이터다([L20](../../examples/hello_module/hello.c#L20)). GPL 표기가 out-of-tree 여부를 바꾸지는 않는다.
- `pr_info`는 `printk(KERN_INFO ...)`의 짧은 표현이다. 장치 객체가 있는 드라이버는 `dev_info`·`dev_err`로 장치 이름을 함께 남긴다(05·06 예제).

### 4-2. Makefile: 변수와 Kbuild 호출

| 변수·인자 | 역할 |
| --- | --- |
| `obj-m := hello.o` | `hello.c`를 로드 가능한 모듈 `hello.ko`로 빌드. 이름은 함수가 아니라 이 선언에서 정해진다 |
| `KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)` | 저장소의 커널 빌드 출력. `?=`라 명령줄 `make KDIR=...`로 바꿀 수 있다 |
| `ARCH=arm64`, `CROSS_COMPILE=aarch64-linux-gnu-` | Pi용 ARM64 코드를 WSL에서 만든다 |
| `$(MAKE) -C "$(KDIR)" M="$(CURDIR)"` | 커널 Makefile로 들어가 외부 모듈 디렉터리를 알려 준다. `$(MAKE)`는 옵션·병렬 정보를 이어 준다 |
| `LOCALVERSION=` | 커널 빌드 때와 같은 버전 문자열 유지 |
| `modules`, `clean` | 외부 모듈 빌드·정리. `clean`은 커널 전체 출력을 지우지 않는다 |
| `.PHONY: all clean` | 같은 이름의 파일이 있어도 타깃을 건너뛰지 않게 한다 |

레시피 줄 앞은 공백이 아니라 탭이다. `all`은 예약어가 아니라 처음 나온 일반 타깃이라 기본으로 선택된다.

### 4-3. 산출물
`make`가 끝나면 `hello.ko` 외에 `hello.o`, `hello.mod.c`(자동 생성 메타데이터), `modules.order`, 예제의 `Module.symvers`(이 모듈이 내보내는 심볼, 여기서는 0바이트), `.*.cmd`(명령·의존성 기록)가 생긴다. 소스와 명령이 그대로면 다시 `make`할 때 대부분 단계를 건너뛴다(증분 빌드).

### 4-4. 로드와 해제
`insmod ./hello.ko` → 로더가 vermagic·심볼을 확인하고 연결 → `hello_init` 호출 → `lsmod`에 등록. 처음 out-of-tree 모듈을 올리면 `loading out-of-tree module taints kernel.`이 한 번 찍히는데 로드 실패가 아니다. `rmmod hello`는 파일 경로가 아니라 **모듈 이름**으로 내린다.

### 4-5. VEDA 수업 코드와의 차이
비교 대상은 VEDA `c2f2ace95a`의 `리눅스커널/수업코드/kmodule/hello_module.c`·`Makefile`이다(저장소 밖이라 텍스트로 적음).

| 수업 코드 | 현재 코드 | 이유 |
| --- | --- | --- |
| `printk(KERN_NOTICE ...)`, `kernel.h` | `pr_info`, `printk.h` | 로그 의도를 직접 드러냄(수준은 NOTICE → INFO로 다름) |
| `module_exit` 아래 단독 문자 `i` | 없음 | 유효한 C 문장이 아님 |
| `KDIR=/usr/src/linux`, `default`에서 `make` 직접 호출 | `out/kernel-rpi4`, `KERNELRELEASE` 분기, `$(MAKE)` | 대상 커널 빌드 출력에 맞추고 호출 문맥을 나눔 |
| 빌드 인자 고정 | `ARCH`·`CROSS_COMPILE`·`LOCALVERSION=`을 빌드·clean 모두에 전달 | 크로스 빌드와 버전 일치 |

## 5. 재현

<details><summary>빌드 → 전송 → 로드 → 해제 (펼치기)</summary>

선행 조건: [07 커널 빌드](07-kernel-build.md)의 전체 빌드 출력 `out/kernel-rpi4`(커밋 `b3495ea401`, `bcm2711_defconfig`, `LOCALVERSION=-v8-bsp-study01`), SSH 별칭 `pi`, Pi 커널 `6.18.59-v8-bsp-study01`.

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/hello_module — 빌드·전송
make && echo $?                     # 0
file hello.ko                       # ELF 64-bit LSB relocatable, ARM aarch64
modinfo -F vermagic ./hello.ko      # 6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64
ssh pi 'mkdir -p ~/bsp/modules' && scp hello.ko pi:bsp/modules/
```

```bash
# [Pi] ~/bsp/modules — 버전 대조 → 로드 → 해제
uname -r; modinfo -F vermagic ./hello.ko     # 버전 부분이 같아야 함
sudo insmod ./hello.ko && lsmod | grep '^hello '
sudo rmmod hello && sudo dmesg | grep -F 'hello:' | tail -2   # hello: loaded / hello: unloaded
```

증분 빌드를 보려면 WSL에서 `make`(대부분 생략) → `make clean` → `make`(다시 생성)를 차례로 실행한다.

</details>

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| `lsmod \| grep '^hello '`가 종료 상태 1 | 해제 뒤 항목이 없어 grep이 못 찾음 | 해제 실패가 아니다. 출력이 없는지만 본다 |
| 새 `.ko`를 보냈는데 로그가 그대로 | 파일 전송만으로 로드된 코드는 바뀌지 않음 | rmmod → insmod |

**현재 버전 검증 (2026-11-03, 이름 변경 전 `bsp_hello`)**

| 항목 | 결과 |
| --- | --- |
| 빌드 | 종료 상태 0, ARM aarch64 relocatable, vermagic `6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64` |
| 증분·clean | 수정 없는 make는 생략, clean 뒤 재빌드 |
| 산출물 | 예제 `Module.symvers` 0바이트(내보내는 심볼 없음) |
| Pi 로드·해제 | `bsp_hello 12288 0`, taint 메시지, loaded·unloaded 로그 |

- 미검증: 이름을 `hello`로 바꾼 뒤와 별칭 `pi`·`~/bsp/modules` 경로로 바꾼 뒤의 빌드·로드 재실행, 버전 불일치 시 insmod 오류 메시지.
- 한계: 자원을 잡지 않는 예제라 init 실패 시 정리 경로가 없다. 자원을 잡는 예제는 [03](03-char-device.md)·[06](06-mpu6050.md)을 본다.

**더 보기**
- `:=`와 `?=`의 차이는? → `:=`는 읽을 때 값을 계산해 고정, `?=`는 아직 정의되지 않았을 때만 기본값. 명령줄 `make KDIR=...`가 둘 다보다 우선한다
- `lsmod`의 숫자는? → 크기는 파일이 아니라 모듈 메모리 크기, 마지막 숫자는 사용 참조 수
- taint 표시는 언제 사라지나 → 같은 부팅 동안 남는다. 모듈을 내려도 지워지지 않는다
- `=y`와 `=m`은? → 커널 설정에서 내장(`=y`)과 별도 모듈(`=m`)
- 공식 문서 → [외부 모듈 빌드](https://docs.kernel.org/kbuild/modules.html), [printk](https://docs.kernel.org/core-api/printk-basics.html), [tainted kernels](https://docs.kernel.org/admin-guide/tainted-kernels.html)
