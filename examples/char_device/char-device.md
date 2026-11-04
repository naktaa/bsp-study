# 문자 디바이스 — 등록·read/write·ioctl

## 1. 이번 단계에서 구현한 기능과 범위

[char_drv.c](char_drv.c)는 hello 모듈에서 한 단계 더 나아가, 사용자 프로그램의 open/close를 커널 콜백으로 연결하는 문자 디바이스 예제다. 먼저 모듈을 로드해 장치 동작을 등록하고, 이후 장치 파일을 열 때 등록한 함수가 호출되는 구조를 익힌다.

| 구현됨 | 아직 구현하지 않음 |
| --- | --- |
| 동적 major 할당과 문자 디바이스 등록·해제 | ioctl 데이터 인자·32비트 호환 처리 |
| open/release 콜백과 로그 | 센서·GPIO 등 하드웨어 제어 |
| 등록 실패 처리, 모듈 소유자 지정 | 자동 `/dev` 노드 생성 |
| C 앱 open/write/close·오류 처리, 앱 Make 규칙 | 길이 경계·복사 실패 경로 검증 |
| 최대 63바이트 write·저장 길이·mutex 보호 | 이진 데이터 출력 형식·긴 요청 처리 |
| 저장 데이터 read·EOF·앱의 echo 비교 | 동시 접근 실행 검증 |
| ioctl CLEAR·앱의 초기화 후 EOF 확인 | 미지원 ioctl 명령 경로 검증 |

2026-11-04 사용자 제공 Pi 출력으로 모듈 로드·장치 등록·수동 노드 생성·셸 open/release·등록 해제·노드 삭제를 확인했다. [char_app.c](char_app.c)에 C 앱을 추가했으며 사용자 실행 출력으로 open/close·종료 상태 0·콜백을 확인했다. write 확장 후 Pi에서 14바이트 전송과 수신 로그를 확인했다. 원시 WSL 빌드 출력·길이 경계·복사 실패 경로는 별도 미확인이다. 이후 앱 출력으로 고정 응답 read 19바이트와 EOF를 확인했다. 저장 데이터 echo 버전도 사용자 앱 출력으로 14바이트·내용 일치를 확인했다. 이후 스타일 정리는 동작을 유지하며 빌드·실행을 재수행하지 않았다. ioctl CLEAR도 사용자 앱 출력에서 성공과 초기화 후 EOF를 확인했다. 이번 실행의 종료 상태·커널 로그는 별도 미수신이다. write 경계값 검증은 사용자 선택으로 생략했으며 read 진행 조건으로 요구하지 않는다. [Makefile](Makefile)의 WSL 빌드 명령은 아래 재현 절차이며 WSL 빌드 종료 상태·file 출력 자체는 아직 전달받지 않았다.

## 2. 전체 실행 흐름

사용자가 실행하는 명령과 커널이 호출하는 함수를 나눠서 보면 다음과 같다.

```text
Pi에서 sudo insmod char_drv.ko
  → 커널이 모듈을 로드하고 char_init() 호출
  → register_chrdev()로 major와 char_fops 등록
  → 성공하면 loaded 로그, 실패하면 음수 오류를 반환해 로드 실패

실제 major 확인 후 /dev/char_drv 문자 장치 노드 생성 (별도 작업)

사용자 앱이 open("/dev/char_drv", ...) 호출
  → 커널이 노드의 major/minor로 등록된 장치 동작을 찾음
  → char_fops.open에 지정한 char_open() 호출
  → 성공하면 앱의 open()이 fd 반환

앱이 close(fd) 호출
  → 단순 open/close에서는 마지막 열린 파일 참조가 해제됨
  → 커널이 char_fops.release에 지정한 char_release() 호출

열린 파일을 모두 정리한 뒤 sudo rmmod char_drv
  → 커널이 char_exit() 호출
  → unregister_chrdev()로 장치 등록 해제
  → 모듈 해제 (/dev 노드는 별도 삭제)
```

사용자 앱은 `char_open()`을 직접 호출하지 않는다. 커널이 파일 연산을 처리하다가 우리가 등록한 함수 포인터를 통해 호출한다. 또한 `insmod` 자체가 장치를 open하는 것은 아니므로 로드 직후에는 `char_open()` 로그가 나올 이유가 없다.

## 3. 코드 블록별 해설

아래 블록은 [실제 C 소스](char_drv.c)에서 가져왔다. 흐름을 설명하기 위해 콜백 → 연산 표 → 등록·해제 순서로 읽는다.

### 헤더·이름·상태

```c
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/printk.h>

#define DEVICE_NAME "char_drv"
#define DRIVER_AUTHOR "Nakta"
#define DRIVER_DESC "Character device open and release practice"

static int major;
```

`init.h`는 초기화·종료 관련 표시, `fs.h`는 `struct inode`, `struct file`, `struct file_operations`와 문자 장치 등록 API, `module.h`는 모듈 등록·메타데이터, `printk.h`는 커널 로그 선언을 제공한다. 아래는 최초 open/release 단계의 헤더 발췌다. write 확장에서는 오류 코드용 `<linux/errno.h>`와 사용자 복사용 `<linux/uaccess.h>`도 추가했다.

`DEVICE_NAME`은 등록 API에 전달할 문자열을 한곳에 둔 매크로다. 등록·해제 시 같은 이름을 사용한다. 작성자·설명 매크로는 마지막 `MODULE_*` 선언에 들어갈 메타데이터다.

`major`는 등록 후 받은 주 번호를 보관해 종료 시 다시 사용한다. 파일 범위의 변수라 함수가 반환한 뒤에도 값이 유지되며, `static`은 이 식별자를 다른 C 파일에서 직접 참조하지 못하도록 범위를 제한한다. 파일 범위 변수는 명시적인 초기값이 없으면 0으로 초기화되지만, 실제 장치 번호는 등록 함수의 반환값으로 결정된다.

### open과 release 콜백

```c
static int char_open(struct inode *inode, struct file *filp)
{
    pr_info("char_drv: [OPEN] opened\n");
    return 0;
}

static int char_release(struct inode *inode, struct file *filp)
{
    pr_info("char_drv: [CLOSE] released\n");
    return 0;
}
```

두 함수는 커널의 `file_operations`가 요구하는 형태에 맞춘 콜백이다. `inode`는 장치 노드를 포함한 파일 자체의 커널 정보를, `filp`는 이번에 열린 파일의 상태를 가리키는 포인터다. 현재 구현은 로그만 남기므로 인자를 사용하지 않지만 콜백 형식에 맞게 받는다. 이후 장치 번호나 열린 파일별 상태를 다룰 때 활용할 수 있다.

`char_open()`의 `return 0`은 커널에 열기 성공을 알린다. 초기화 실패 등으로 open을 거부해야 한다면 음수 오류를 반환한다. 사용자 앱이 받는 fd 번호를 이 함수가 직접 반환하는 것은 아니다. 커널이 나머지 open 처리를 마치고 앱에 fd를 전달한다.

`char_release()`는 열린 파일의 마지막 참조를 정리할 때 호출되며 현재는 정리할 자원이 없어 0을 반환한다. 단순 open/close라면 close 후 호출되지만, `dup()`이나 `fork()` 등으로 참조를 공유하면 각 close마다 반드시 호출되는 것은 아니다. release 반환값을 앱의 close 오류 전달 수단으로 삼지도 않는다.

함수 앞의 `static`도 이 C 파일 안으로 이름의 직접 참조 범위를 제한한다. 커널이 호출할 수 있는 이유는 다음 연산 표에 함수 포인터를 전달하기 때문이다. `static`이 콜백 호출을 막는 것은 아니다.

`pr_info()`는 INFO 수준의 커널 로그다. 로그 수준과 `printk()`의 관계는 [hello 모듈 해설](../hello_module/kernel-module.md#c-코드의-핵심)을 참고한다.

### file_operations와 함수 포인터

```c
static const struct file_operations char_fops = {
    .owner = THIS_MODULE,
    .open = char_open,
    .release = char_release,
    .write = char_write,
    .read = char_read,
};
```

`struct file_operations`는 파일 연산마다 어떤 함수를 호출할지 저장하는 표다. `.open = char_open`은 구조체 멤버 이름을 지정하는 C 초기화 문법이며, open 멤버에 `char_open`의 함수 포인터를 저장한다. `char_open()`처럼 괄호를 붙여 지금 호출하는 코드와 구분해야 한다. 함수 이름은 이 문맥에서 함수 포인터로 변환되므로 `&char_open`으로 쓰는 것도 가능하다.

`static`은 표의 이름을 이 파일 안으로 제한하고 모듈이 존재하는 동안 표가 유지되게 한다. `const`는 초기화 후 이 표를 수정하지 않겠다는 뜻이다. 콜백 함수 내부의 상태까지 모두 불변으로 만드는 것은 아니다.

`.owner = THIS_MODULE`은 이 연산 표를 소유한 모듈을 지정한다. 커널은 연산 표를 사용할 때 모듈 참조를 유지하므로, 장치를 연 상태에서 일반적인 `rmmod`로 콜백 코드가 사라지는 일을 막는다. 사용 중이라 해제되지 않으면 먼저 파일을 닫고 확인한다.

초기 단계는 open/release만 등록했고, 현재는 `.write = char_write`를 추가했다. 현재는 `.read = char_read`도 연결했다. 등록하지 않은 `.unlocked_ioctl` 등은 0으로 초기화된다. read는 아래 고정 응답 단계에서 설명한다. write는 아래 C 앱으로 길이와 반환값을 함께 확인한다.

### 초기화와 동적 장치 등록

```c
static int __init char_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &char_fops);
    if (major < 0) {
        pr_err("char_drv: [MODULE] registration failed: %d\n", major);
        return major;
    }

    pr_info("char_drv: [MODULE] loaded, major=%d\n", major);
    return 0;
}
```

모듈을 로드할 때 커널이 호출할 함수다. `__init`의 섹션 의미는 [hello 모듈 해설](../hello_module/kernel-module.md#c-코드의-핵심)과 같다. 등록 인자는 다음과 연결된다.

| 인자 | 이 코드에서의 의미 |
| --- | --- |
| `0` | 특정 major를 고정하지 않고 커널에 동적 할당 요청 |
| `DEVICE_NAME` | `/proc/devices`에 표시할 등록 이름 `char_drv` |
| `&char_fops` | 연산 표 객체의 주소를 전달해 커널이 콜백을 참조하도록 함 |

`&`는 여기서 객체의 주소를 구하는 연산자다. 앞서 `.open = char_open`에 저장한 것은 함수 포인터이고, `&char_fops`는 구조체 객체를 가리키는 포인터다. 둘 다 포인터를 전달하지만 가리키는 대상이 다르다.

이 호출처럼 major 인자가 0이면 성공 시 할당된 **양수 major**가 반환된다. 실패하면 음수 오류가 반환되므로 `major < 0`을 검사한다. 고정 major를 요청하는 API 사용에서는 성공 반환값이 0일 수 있지만, 현재 코드는 동적 할당 방식이다.

실패 시 오류 수준인 `pr_err()`로 기록하고 같은 음수 값을 반환해 모듈 로드를 실패시킨다. 성공 경로에서만 `loaded`를 출력하고 init의 성공 값 0을 반환한다. **등록 함수의 성공 값 major와 init 함수의 성공 값 0은 서로 다르다.**

실패한 init 뒤에 커널이 `char_exit()`를 자동 호출해 정리해 주지는 않는다. 이 코드에는 등록 성공 후 추가로 실패할 단계가 없지만, 나중에 자원을 더 확보한다면 init 안에서 실패 지점 이전에 확보한 자원을 되돌리는 처리가 필요하다.

대상 커널의 `register_chrdev()`는 내부적으로 minor 0~255 범위를 등록한다. 이번 실습에서는 minor 0의 노드로 open/release를 확인했다. major는 매번 같은 값이라고 가정하지 않고 실제 로드 결과로 확인한다.

### 등록 해제와 모듈 연결

```c
static void __exit char_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    pr_info("char_drv: [MODULE] unloaded\n");
}

module_init(char_init);
module_exit(char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
```

`module_init()`과 `module_exit()`는 함수 이름을 커널 모듈의 초기화·종료 지점으로 연결하는 매크로다. 이 줄에서 함수를 즉시 호출하는 것이 아니다. 정상적인 모듈 해제 과정에서 커널이 `char_exit()`를 호출한다.

`unregister_chrdev()`에는 저장해 둔 major와 등록 이름을 전달한다. 반환형은 void다. 커널 안의 번호 범위와 장치 동작 등록을 정리하지만, 별도로 만든 `/dev/char_drv` 노드를 삭제하지는 않는다. 해당 노드도 필요가 없어지면 별도로 삭제해야 한다.

`MODULE_*`는 라이선스·작성자·설명을 모듈 정보로 남긴다. 장치 등록이나 open을 대신 수행하는 선언은 아니다.

### 모듈 Makefile의 핵심

```make
ifneq ($(KERNELRELEASE),)
obj-m := char_drv.o
```

[Makefile](Makefile)의 외부 모듈 빌드 구조는 hello 모듈과 같다. Kbuild가 읽는 분기에서 `char_drv.o`를 지정하므로 `char_drv.c`를 바탕으로 최종 `char_drv.ko`를 만든다. 함수 이름을 기준으로 모듈 파일명을 결정하는 것은 아니다.

`KDIR`는 이 예제 폴더에서 두 단계 위의 `out/kernel-rpi4`를 가리킨다. `ARCH=arm64`, `CROSS_COMPILE=aarch64-linux-gnu-`, `LOCALVERSION=`으로 기존 커널과 같은 빌드 조건을 사용한다. `ifneq`, `:=`, `?=`, `all/default`, KDIR 경로, clean·증분 빌드는 [hello Makefile 해설](../hello_module/kernel-module.md#makefile의-핵심)과 [증분 빌드](../hello_module/kernel-module.md#2-산출물과-증분-빌드)에 설명했다.

### 사용자 앱의 open과 오류 처리

[char_app.c](char_app.c)는 셸로 확인한 open/close를 C 코드로 옮긴 것이다. 이 절의 발췌는 최초 open/close 단계이며, 현재 추가된 message·status·write와 마지막 반환값 변경은 아래 write 확장 절에서 설명한다. 실행 흐름은 `main → open → 커널 char_open → fd 반환 → close → 커널 char_release → 앱 종료`다. main은 사용자 공간에서 실행하고 드라이버 콜백은 커널에서 실행한다. 커널 함수 주소를 앱에 연결하거나 모듈과 앱을 하나로 링크하지 않는다.

```c
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/char_drv"

int main(void)
{
    printf("=== OPEN ===\n");

    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        perror("[OPEN] " DEVICE_PATH);
        return 1;
    }
```

`fcntl.h`는 open과 O_RDWR, `stdio.h`는 printf/perror, `unistd.h`는 close 선언을 제공한다. 사용자 앱이므로 linux/fs.h 같은 커널 내부 헤더를 포함하지 않는다. `DEVICE_PATH`는 노드 경로이며 커널의 등록 문자열과는 별도다.

`O_RDWR`은 읽기·쓰기 모드로 여는 요청이다. open 자체는 데이터를 전송하지 않는다. 현재 추가한 실제 write 호출은 아래 확장 절에 있다. 앱의 open은 성공하면 0 이상의 fd, 실패하면 -1을 반환하고 errno에 원인을 남긴다. fd 0도 유효하므로 `fd <= 0`으로 검사하면 안 된다. 드라이버 `char_open()`의 성공 반환값 0과 앱이 받는 fd를 구분한다.

`perror()`는 인자로 받은 설명과 errno에 해당하는 오류 메시지를 표준 오류로 출력한다. `"open " DEVICE_PATH`는 매크로 확장 뒤 인접 문자열 리터럴이 합쳐져 `"open /dev/char_drv"`가 되는 C 문법이다. 실패하면 main에서 1을 반환해 셸에 실패 상태를 전달한다. 성공한 fd가 없으므로 이 경로에서는 close하지 않는다.

### 사용자 앱의 close와 종료

```c
    printf("device: %s\nfd: %d\n", DEVICE_PATH, fd);

    printf("\n=== CLOSE ===\n");

    if (close(fd) == -1) {
        perror("[CLOSE] " DEVICE_PATH);
        return 1;
    }

    printf("device closed\n");
    return 0;
}
```

`%s`에는 경로, `%d`에는 정수 fd를 출력한다. 앱 출력은 터미널에, 드라이버의 pr_info 로그는 커널 로그에 남는다. 둘을 함께 확인해야 사용자 호출과 콜백 실행을 연결할 수 있다.

close는 성공 시 0, 실패 시 -1을 반환한다. 성공 경로에서만 closed를 출력하고 main에서 0을 반환한다. close 오류가 났다고 같은 fd로 무조건 재시도하지 않는다. Linux에서는 오류가 나더라도 fd가 이미 해제될 수 있다. 이번 앱은 오류를 보고하고 종료한다.

### 사용자 앱 Make 규칙

```make
APP_CC ?= $(CROSS_COMPILE)gcc
APP_CFLAGS ?= -Wall -Wextra -O0 -g
```

기본 컴파일러는 `aarch64-linux-gnu-gcc`다. Make의 내장 CC 기본값이 호스트 cc일 수 있으므로 별도 APP_CC를 두어 앱도 ARM64로 컴파일한다. APP_CFLAGS는 경고·최적화·디버깅 옵션이며 커널 Kbuild 옵션과 분리한다.

```make
all: modules app
```

인자 없는 make는 모듈과 앱을 모두 빌드한다. `make modules`는 기존 Kbuild 모듈 빌드만, `make app`은 사용자 앱만 수행한다. 앱만 바꿨던 초기 단계에서는 `make app`을 사용했고, 현재처럼 드라이버도 바꾼 단계에서는 둘 다 빌드한다. all과 app 등 작업 이름은 .PHONY로 선언했다.

```make
app: char_app

char_app: char_app.c char_ioctl.h
	$(APP_CC) $(APP_CFLAGS) $< -o $@
```

`char_app`은 실제 실행 파일 타깃이고 char_app.c와 공통 헤더 char_ioctl.h가 선행 파일이다. `$<`는 첫 선행 파일 `char_app.c`, `$@`는 현재 타깃 `char_app`을 뜻한다. 소스·공통 헤더가 더 새롭거나 실행 파일이 없으면 컴파일하고, 변경이 없으면 이 레시피를 생략한다. app이 .PHONY여도 실제 파일 char_app을 매번 강제로 다시 빌드하지는 않는다.

현재는 단일 C 파일이므로 한 명령에서 컴파일과 링크를 수행한다. 별도 앱 .o 파일이나 CMake는 아직 필요하지 않다. clean은 Kbuild로 모듈 산출물을 정리한 뒤 `$(RM) char_app`으로 앱도 정리한다. 앱 실행 파일은 .gitignore에 추가해 소스와 구분했다.

### make deploy: 산출물 전송

현재 [Makefile](Makefile)은 사용자 수정에 따라 빌드와 전송을 분리한다.

```make
PI_HOST := pi
PI_MODULE_DIR := bsp/modules
PI_APP_DIR := bsp/apps
```

SSH 별칭 pi와 로그인 사용자 홈 기준 배포 디렉터리를 지정한다. Pi의 `~/bsp/modules`, `~/bsp/apps`는 앞서 사용하던 디렉터리이며 deploy가 새로 만들지는 않는다.

```make
deploy:
	scp char_drv.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp char_app $(PI_HOST):$(PI_APP_DIR)/
```

`make`로 빌드하고 `make deploy`로 두 파일을 전송한다. 이전 안내의 deploy: all과 달리 현재 deploy는 자동 빌드하지 않으므로 소스를 수정한 뒤 반드시 make를 먼저 실행한다. 전송은 로드된 모듈을 교체하지 않는다. 파일 전송 후 모듈 재로드·노드 확인·실행은 Pi에서 직접 진행한다. deploy는 .PHONY이므로 호출할 때마다 전송한다. 한 전송이 실패하면 다음 레시피 줄은 실행하지 않지만 이미 전송한 파일은 되돌리지 않는다.

### write 확장: 앱에서 커널로 전달하는 흐름

```text
앱 write(fd, message, count)
  → 커널이 char_fops.write의 char_write(filp, buf, count, f_pos) 호출
  → 길이 검사 → copy_from_user → 문자열 끝 처리 → 로그
  → 처리한 바이트 수 반환 → 앱이 요청 길이와 대조
```

앱 문자열은 사용자 공간에 있다. 커널에서 buf를 일반 문자열처럼 직접 읽거나 `%s`로 출력하지 않고, 유효한 사용자 접근 절차를 거쳐 커널 버퍼에 먼저 복사한다.

```c
static ssize_t char_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *f_pos)
{
    char data[64];

    if (count == 0)
        return 0;
    if (count >= sizeof(data))
        return -EMSGSIZE;
    if (copy_from_user(data, buf, count))
        return -EFAULT;

    data[count] = '\0';
    pr_info("char_drv: [WRITE] received %zu bytes: %s\n", count, data);
    return count;
}
```

- `filp`는 열린 파일 상태, `buf`는 사용자 버퍼 주소, `count`는 요청 바이트 수, `f_pos`는 파일 위치를 가리키는 포인터다. `__user`는 사용자 주소임을 표시하는 커널 주석용 매크로이며 메모리를 자동 복사해 주지 않는다.
- `ssize_t`는 바이트 수와 음수 오류를 모두 표현할 수 있는 부호 있는 반환형이고 `size_t`는 크기를 나타내는 부호 없는 형이다. `loff_t *`는 파일 위치 포인터다. 최초 write 단계에서는 로그만 남겨 filp·f_pos를 사용하지 않았다. 현재 echo 확장에서는 저장 버퍼·길이를 갱신하고 같은 파일의 위치를 0으로 되돌린다. 아래 echo 절의 코드가 현재 구현이다.
- `data[64]`는 콜백 호출 동안만 존재하는 커널 스택 버퍼다. 문자열 끝 한 바이트를 남겨 최대 63바이트를 받는다. 0바이트 요청은 복사 없이 0 반환, 64바이트 이상은 `-EMSGSIZE`로 거부한다. 큰 요청을 조용히 잘라서 성공시키지 않는 이번 실습의 선택이다.
- `copy_from_user()`는 복사한 수가 아니라 **복사하지 못한 바이트 수**를 반환한다. 0만 완전 성공이다. 일부라도 실패하면 `-EFAULT`를 반환하고 데이터 로그는 출력하지 않는다. 앞의 길이 검사로 커널 버퍼 범위를 지켜야 하며 복사 API가 그 검사를 대신하지 않는다.
- 성공 시 `data[count] = '\0'`으로 끝을 보장한 뒤 `%s`로 출력한다. `%zu`는 size_t 형식이다. 중간에 NUL이 있는 이진 데이터라면 로그 문자열이 중간에서 끊기므로 현재 예제는 짧은 텍스트 전달에 한정한다.
- 성공한 `return count`는 앱에 전달할 처리 길이다. 커널의 `-EMSGSIZE`, `-EFAULT`는 사용자 write에서 -1과 errno로 나타나며 앱에 같은 음수가 그대로 반환되지 않는다.

C 파일에서는 char_write 정의를 연산 표보다 앞에 두었으므로 `.write = char_write`에서 참조할 수 있다. 별도의 함수 원형을 중복 선언할 필요는 없다.

### 앱의 write와 반환 길이 검사

main 시작 부분에 `const char message[] = "Hello, driver!";`, `size_t count = sizeof(message) - 1;`, `int status = 0;`을 추가했다. 배열의 sizeof에는 끝 NUL도 포함되므로 1을 빼서 실제 텍스트 바이트만 보낸다. 이는 문자열 배열일 때의 계산이며 포인터의 sizeof로 문자열 길이를 구하면 안 된다.

```c
    printf("\n=== WRITE ===\n");

    ssize_t written = write(fd, message, count);
    if (written == -1) {
        perror("[WRITE] " DEVICE_PATH);
        status = 1;
    } else if ((size_t)written != count) {
        fprintf(stderr, "[WRITE] short write: %zd/%zu bytes\n", written, count);
        status = 1;
    } else {
        printf("sent: %zd bytes\n", written);
    }
```

`write()`가 -1이면 perror로 원인을 출력한다. 그 외의 반환값은 실제 처리 길이이며, 요청보다 적게 처리될 수도 있다. 이번 작은 앱은 짧은 전송을 오류로 보고하지만 일반 스트림 앱에서는 남은 부분을 다시 보내는 처리가 필요할 수 있다. 드라이버는 현재 완전 성공이나 오류 방식이므로 정상 경로에서는 두 길이가 같아야 한다.

음수 여부를 먼저 검사한 뒤에만 size_t로 변환해 비교한다. `%zd`는 ssize_t, `%zu`는 size_t를 출력하는 형식이다. 실패 시 status를 1로 바꾸되 아래 close는 계속 실행해 fd를 정리한다. main 마지막은 `return status`로 바꿔 write 실패가 최종 종료 상태에 반영되게 했다. close 자체의 실패도 기존처럼 1을 반환한다.

### 이전 read 단계: 고정 응답과 EOF

이 절은 앞서 확인한 고정 응답 단계의 코드와 개념을 보존한 것이다. 현재 echo 구현은 다음 절에서 설명한다. 이전 read는 write로 받은 데이터를 되돌리는 echo가 아니었다. 커널의 고정 문자열 `Hello from driver!\n`을 읽는다. write는 이전처럼 로그를 남기며 read 위치를 변경하지 않는다.

```text
앱 read(fd, reply, 31)
  → char_read()에서 현재 위치·요청 길이·남은 응답 길이 확인
  → copy_to_user()로 응답 바이트 전달
  → 성공한 만큼 *f_pos 증가, 바이트 수 반환
앱의 다음 read()
  → 위치가 응답 끝이면 0 반환 (EOF)
  → 앱은 반복을 끝내고 close
```

```c
static ssize_t char_read(struct file* filp, char __user* buf,
                         size_t count, loff_t* f_pos) {
    const char reply[] = "Hello from driver!\n";
    size_t length = sizeof(reply) - 1;

    if (*f_pos < 0)
        return -EINVAL;
    if (*f_pos >= (loff_t)length || count == 0)
        return 0;
    if (count > length - *f_pos)
        count = length - *f_pos;
    if (copy_to_user(buf, reply + *f_pos, count))
        return -EFAULT;

    *f_pos += count;
    pr_info("char_drv: [READ] sent %zu bytes\n", count);
    return count;
}
```

`buf`는 앱이 받을 사용자 버퍼다. write와 반대로 커널 → 사용자 방향이므로 `copy_to_user()`를 사용한다. `filp`는 열린 파일 상태, `count`는 앱이 요청한 최대 크기, `f_pos`는 이번 열린 파일의 위치를 가리키는 포인터다. 현재 filp 자체는 사용하지 않는다.

`reply`는 고정 응답 배열이고 `length`는 마지막 NUL을 제외한 19바이트다. 배열은 호출마다 만들어지지만 내용은 같으며 실제 진행 상태는 `*f_pos`로 유지한다. 음수 위치는 `-EINVAL`, 이미 끝에 도달했거나 요청이 0바이트라면 0을 반환한다. `length`를 loff_t로 변환해 위치와 같은 부호 있는 형으로 비교한다. 이 작은 고정 길이는 해당 형에 안전하게 들어간다.

요청이 남은 데이터보다 크면 count를 남은 길이로 줄인다. 따라서 앱 버퍼 크기와 응답 범위를 모두 지킨다. `reply + *f_pos`는 배열 시작 주소에서 현재 위치만큼 이동한 주소다. 예를 들어 처음 5바이트를 읽었다면 다음에는 offset 5부터 읽을 수 있는 구현이다. 이번 앱은 응답 전체가 들어가는 크기로 읽으며 작은 조각 읽기 시험을 별도 요구하지 않는다.

`copy_to_user()`도 복사하지 못한 바이트 수를 반환한다. 실패하면 `-EFAULT`를 반환하고 위치·성공 로그를 갱신하지 않는다. 일부 복사 후 실패라면 앱 메모리 일부가 바뀔 수 있으므로 오류 반환이 복사 취소를 의미하지는 않는다. 이번 단계는 완전 복사 성공 경로를 중심으로 다룬다.

`*f_pos += count`는 포인터가 가리키는 실제 파일 위치를 갱신한다. 포인터 자체를 이동하는 `f_pos += count`와 다르다. 다음 read에서 끝 위치를 확인하면 0을 반환해 EOF를 알린다. 별도의 open으로 파일을 다시 열면 새 위치 0에서 응답을 다시 읽는다. 사용자 임의 seek를 위한 llseek 구현은 추가하지 않았다.

### 이전 고정 응답 앱의 반복 read와 문자열 출력

```c
    char reply[32];
    ssize_t received;
    while ((received = read(fd, reply, sizeof(reply) - 1)) > 0) {
        reply[received] = '\0';
        printf("read %zd bytes: %s", received, reply);
    }
    if (received == -1) {
        perror("[READ] " DEVICE_PATH);
        status = 1;
    } else {
        printf("EOF\n");
    }
```

앱 버퍼는 32바이트지만 read에는 31바이트만 요청한다. 드라이버 read는 NUL을 보내지 않으므로, 앱이 실제 받은 길이 위치에 NUL을 붙여 `%s`로 안전하게 출력한다. 요청 크기가 아닌 반환된 received를 인덱스로 쓰는 이유다.

while 조건의 대입은 read 반환값을 received에 저장하고 양수인지 검사한다. 양수면 데이터를 출력하고 다시 읽는다. 0이면 EOF여서 정상 종료, -1이면 perror와 실패 상태를 남긴다. 파일이 끝났다는 0을 오류로 처리하지 않는다. 응답 끝에 줄바꿈이 있으므로 printf에는 별도 줄바꿈을 넣지 않았다. 어떤 경로에서도 이후 close 처리로 진행한다.

### 현재 echo 단계: 저장 버퍼의 수명과 공유 상태

```text
앱 write("Hello, driver!", 14)
  → 임시 커널 버퍼로 사용자 데이터 복사
  → mutex 획득 → 공유 버퍼·길이 교체 → 현재 파일 위치 0 → 잠금 해제
앱 read()
  → mutex 획득 → 저장 길이와 위치 확인 → 사용자 버퍼로 복사
  → 읽은 만큼 위치 증가 → 잠금 해제
앱 다음 read() → EOF
  → 전체 길이·바이트 내용 비교 → close
```

```c
static char saved_data[64];
static size_t saved_length;
static DEFINE_MUTEX(data_lock);
```

이 세 객체는 파일 범위에 있으므로 모듈이 로드된 동안 유지된다. 함수 안의 임시 data 배열은 write 종료 후 사용할 수 없으므로 보관용 배열을 별도로 둔다. 시작 시 버퍼·길이는 0으로 초기화돼 write 이전 read는 EOF다. 각 write는 이전 데이터를 대체하며 이어붙이지 않는다. 0바이트 write는 기존처럼 아무것도 바꾸지 않고 0을 반환한다.

공유 데이터가 여러 호출에 노출되므로 `DEFINE_MUTEX`로 초기화한 mutex를 사용한다. mutex는 한 호출이 보호 구간을 실행하는 동안 다른 호출을 기다리게 한다. 단순 static 선언 자체는 동시 접근을 보호하지 않는다. 사용자 복사는 잠들 수 있으므로 여기서는 잠든 채 기다릴 수 있는 mutex를 사용하고 spinlock으로 감싸지 않는다.

```c
static ssize_t char_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos)
{
	char data[64];

	if (count == 0)
		return 0;
	if (count >= sizeof(data))
		return -EMSGSIZE;

	/* 사용자 복사가 실패하면 기존 저장 내용은 유지한다. */
	if (copy_from_user(data, buf, count))
		return -EFAULT;

	data[count] = '\0';

	mutex_lock(&data_lock);

	memcpy(saved_data, data, count + 1);
	saved_length = count;

	/* 같은 열린 파일에서 새 메시지를 처음부터 읽도록 한다. */
	*f_pos = 0;

	pr_info("char_drv: [WRITE] received %zu bytes: %s\n", count, saved_data);

	mutex_unlock(&data_lock);

	return count;
}

static ssize_t char_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *f_pos)
{
	ssize_t result = 0;

	if (*f_pos < 0)
		return -EINVAL;
	if (count == 0)
		return 0;

	mutex_lock(&data_lock);

	if (*f_pos >= (loff_t)saved_length)
		goto out;
	if (count > saved_length - *f_pos)
		count = saved_length - *f_pos;

	if (copy_to_user(buf, saved_data + *f_pos, count)) {
		result = -EFAULT;
		goto out;
	}

	*f_pos += count;
	pr_info("char_drv: [READ] sent %zu bytes\n", count);
	result = count;

out:
	mutex_unlock(&data_lock);

	return result;
}
```

write의 길이 검사·copy_from_user는 먼저 임시 배열에 수행한다. 완전 복사에 성공한 뒤 잠금을 잡고 `memcpy(..., count + 1)`로 NUL까지 저장한다. 이렇게 하면 사용자 복사 실패 때문에 기존 공유 데이터가 일부만 갱신되는 일을 피한다. saved_length는 실제 payload 길이이며 NUL은 제외한다. memcpy는 커널 버퍼끼리 복사할 때 사용한다.

`*f_pos = 0`은 이 write를 수행한 열린 파일이 새 내용을 처음부터 읽도록 정한 실습 동작이다. 일반 파일의 write 위치 규칙을 그대로 따르는 것은 아니다. 다른 열린 파일의 위치까지 초기화하지는 않는다.

read는 잠금을 잡은 상태로 길이·버퍼를 함께 확인하고 사용자에게 복사한다. 데이터 복사 중 다른 write가 버퍼를 바꾸지 못하게 하기 위해서다. 끝 위치에서는 result의 초기값 0, 복사 실패 시 -EFAULT, 성공 시 처리 길이를 반환한다. 모든 잠금 획득 후 경로가 `out:`으로 모여 잠금을 해제한다. goto는 이 함수 안의 정리 지점으로 이동하며, 잠금을 잡은 채 return하는 실수를 줄이는 용도다.

버퍼는 모듈 전체에서 하나이고 파일 위치는 열린 파일마다 관리된다. mutex는 **한 콜백 동안** 데이터·길이를 일관되게 보호할 뿐, 여러 read 사이의 내용을 하나의 스냅샷으로 고정하지 않는다. 다른 앱이 중간에 write하면 다음 read는 바뀐 데이터를 볼 수 있다. 이번에는 앱 하나로 확인하며, 앱별 버퍼·동시 접속 프로토콜은 추가하지 않는다.

### 현재 앱: 수신 데이터 누적과 echo 비교

```c
    char reply[64];
    char chunk[32];
    size_t total = 0;
    ssize_t received;

    printf("\n=== READ ===\n");

    while ((received = read(fd, chunk, sizeof(chunk))) > 0) {
        if ((size_t)received > sizeof(reply) - 1 - total) {
            fprintf(stderr, "[READ] reply too long\n");
            status = 1;
            break;
        }

        memcpy(reply + total, chunk, (size_t)received);
        total += (size_t)received;
        printf("received: %zd bytes\n", received);
    }

    if (received == -1) {
        perror("[READ] " DEVICE_PATH);
        status = 1;
    } else if (received == 0) {
        printf("EOF\n");
    }

    reply[total] = '\0';
    printf("reply: %s\n", reply);

    if (status == 0) {
        printf("\n=== VERIFY ===\n");

        if (total != count || memcmp(reply, message, count) != 0) {
            fprintf(stderr, "[VERIFY] echo mismatch\n");
            status = 1;
        } else {
            printf("echo matched: %zu bytes\n", total);
        }
    }
```

read는 한 번에 전체 길이를 반환한다고 가정하지 않고 chunk 단위 결과를 reply에 누적한다. total은 누적 길이다. memcpy 전 남은 공간을 검사하고, 문자열 출력용 NUL 한 칸도 남긴다. 범위를 넘으면 실패 상태로 중단한 뒤 close한다.

`reply + total`은 다음 저장 위치다. 출력에는 NUL을 붙이지만 실제 비교 길이는 total이다. `total != count`로 길이를 먼저 비교하고, 같을 때만 memcmp로 count바이트를 비교한다. C의 `||`는 왼쪽이 참이면 오른쪽을 실행하지 않으므로 길이가 다를 때 내용 비교는 생략한다. memcmp 반환값 0은 바이트 내용 일치다. 이 함수와 memcpy를 위해 앱에 string.h를 포함했다.

write/read 오류가 없고 길이·내용까지 같아야 `echo matched`를 출력한다. 다른 프로세스의 중간 write는 mismatch 원인이 될 수 있다. 정상 경로에서는 write 14 → read 14 → EOF → echo matched 14가 예상된다. 기존 고정 응답 19바이트와 구분한다.

### ioctl 확장: データ 전달과 제어 명령 구분

현재 read/write는 사용자 데이터 자체를 주고받고, ioctl은 버퍼 초기화라는 제어 동작을 요청한다. 명령만 전달하는 첫 단계로 구성하고 정수·구조체 인자 전달은 아직 추가하지 않는다.

```text
앱 echo 비교 성공
  → ioctl(fd, CHAR_CLEAR)
  → 커널이 char_fops.unlocked_ioctl의 char_ioctl(filp, cmd, arg) 호출
  → CLEAR 명령 확인 → mutex 획득 → 버퍼·길이 초기화
  → 이 열린 파일의 위치 0 → 잠금 해제 → 성공 0 반환
앱 read → 저장 길이 0이므로 EOF → close
```

공통 [char_ioctl.h](char_ioctl.h)는 앱과 드라이버에 같은 명령 정의를 제공한다.

```c
#ifndef CHAR_IOCTL_H
#define CHAR_IOCTL_H

#include <linux/ioctl.h>

#define CHAR_IOC_MAGIC 'C'
#define CHAR_CLEAR _IO(CHAR_IOC_MAGIC, 1)

#endif
```

헤더 가드는 같은 헤더의 중복 처리를 막는다. 이 헤더는 커널 내부 자료구조 없이 사용자 공간에서도 사용 가능한 ioctl 정의를 포함한다. `_IO(type, nr)`는 부가 데이터 인자 없이 요청하는 명령 번호를 만든다. 여기서는 구분용 문자 C와 명령 번호 1을 사용하며, 전역적으로 고유한 번호를 확보했다는 뜻은 아니다. 각 fd의 드라이버가 요청을 해석한다. 앱에는 호출 선언을 위한 `<sys/ioctl.h>`도 별도로 필요하다.

Makefile의 `char_app: char_app.c char_ioctl.h`로 공통 헤더를 앱 의존성에 추가했다. `$<`는 여전히 첫 선행 파일 char_app.c다. 모듈 쪽 헤더 변경은 기존 Kbuild 의존성 추적이 담당한다. 헤더를 바꿨다면 앱과 모듈을 모두 다시 빌드한다.

```c
static long char_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case CHAR_CLEAR:
		mutex_lock(&data_lock);

		memset(saved_data, 0, sizeof(saved_data));
		saved_length = 0;

		/* EOF 뒤의 재확인도 처음 위치에서 수행하도록 한다. */
		filp->f_pos = 0;

		pr_info("char_drv: [IOCTL] buffer cleared\n");

		mutex_unlock(&data_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}
```

`filp`는 열린 파일 상태, `cmd`는 요청 번호, `arg`는 명령별 추가 인자를 담을 unsigned long이다. 이번 `_IO` 명령은 arg를 사용하지 않는다. 반환형 long은 unlocked_ioctl의 콜백 형식이다. 지원 명령은 0, 다른 명령은 `-ENOTTY`를 반환한다. 앱에서는 실패가 -1과 errno로 나타나며, 지원하지 않는 명령을 성공으로 숨기지 않는다.

연산 표에 `.unlocked_ioctl = char_ioctl`을 추가했다. 이름의 unlocked는 동시 접근 보호를 없애라는 뜻이 아니다. 드라이버가 공유 상태에 필요한 잠금을 직접 사용해야 한다. CLEAR도 기존 write/read와 같은 mutex로 보호해 버퍼·길이를 함께 바꾼다. 다른 열린 파일의 위치까지 초기화하지는 않는다.

`memset`은 저장 배열을 0으로 채우고 saved_length도 0으로 만든다. `filp->f_pos = 0`은 이번 파일의 읽기 위치를 처음으로 돌린다. echo 읽기 뒤 위치가 끝에 남아 있는 것만으로 EOF가 나오는 상황을 피하고, 처음 위치에서도 초기화된 데이터가 없음을 확인하도록 하는 동작이다.

### 앱의 CLEAR와 EOF 확인

```c
    if (status == 0) {
        printf("\n=== IOCTL: CLEAR ===\n");

        if (ioctl(fd, CHAR_CLEAR) == -1) {
            perror("[IOCTL: CLEAR] ioctl");
            status = 1;
        } else {
            printf("buffer cleared\n");

            received = read(fd, chunk, sizeof(chunk));
            if (received == -1) {
                perror("[IOCTL: CLEAR] read after clear");
                status = 1;
            } else if (received != 0) {
                fprintf(stderr, "[IOCTL: CLEAR] expected EOF after clear, got %zd bytes\n",
                        received);
                status = 1;
            } else {
                printf("read after clear: EOF\n");
            }
        }
    }
```

echo 검증이 성공했을 때만 CLEAR로 넘어간다. 실패하면 perror와 상태 1을 남기고 이후 close로 정리한다. 성공한 뒤 read를 다시 호출해 -1은 오류, 양수는 예상 밖 잔여 데이터, 0은 초기화 후 정상 EOF로 구분한다. 여기서 받는 것은 데이터가 아니라 상태이므로 기존 문자열 출력이나 echo 비교를 반복하지 않는다.

이번 기능의 의도는 단일 앱에서 버퍼 초기화 제어를 확인하는 것이다. CLEAR와 다음 read 사이에 다른 앱이 write하면 데이터가 생길 수 있으므로 두 시스템 호출 전체가 하나의 원자적 작업이라고 가정하지 않는다. 32비트 앱용 compat_ioctl이나 데이터 포인터 인자 규약은 이번 ARM64 앱 범위에 포함하지 않는다.

## 4. 헷갈리기 쉬운 개념과 질문

| 구분 | 이 예제에서의 의미 |
| --- | --- |
| 모듈 로드 / 장치 open | 로드는 코드를 커널에 넣고 등록하는 단계, open은 이미 등록한 장치에 대한 파일 열기 요청 |
| `fd` / `struct file *` | fd는 앱이 받는 정수 식별자, filp는 커널의 열린 파일 상태를 가리키는 포인터. 여러 fd가 같은 struct file을 공유할 수도 있음 |
| `struct inode *` / `struct file *` | inode는 파일 자체 정보, file은 열린 파일 상태. 같은 노드를 별도로 열면 같은 inode와 별도의 file을 사용할 수 있음 |
| 장치 등록 / `/dev` 노드 생성 | 등록은 커널의 장치 번호·연산 연결, 노드는 파일시스템의 접근 경로. register_chrdev만으로 노드가 생기지 않음 |
| 등록 해제 / 노드 삭제 | unregister_chrdev는 커널 등록 정리, 노드 삭제는 파일시스템 항목 정리. 한쪽이 다른 쪽을 자동 수행하지 않음 |
| close / rmmod | close는 열린 파일 참조 정리, rmmod는 모듈 자체 해제. 앱 종료가 모듈 해제를 의미하지 않음 |

major는 등록된 장치 동작을 찾는 주 번호이고 minor는 그 범위 안의 장치를 구분하는 부 번호다. 이 코드는 minor별 동작을 아직 구분하지 않는다. `/dev`의 **문자 장치 종류와 major/minor**가 커널 등록으로 연결되며 파일 이름 자체로 콜백을 찾지는 않는다. `DEVICE_NAME`과 노드 이름을 같게 잡는 것은 사람이 보기 쉽게 하기 위한 선택이다.

### 등록 정보 `/proc/devices`와 접근 경로 `/dev/char_drv`

이번 실습의 세 요소는 **커널 내부 등록**, **등록 정보를 읽는 창**, **앱이 장치를 여는 입구**다.

```text
register_chrdev(0, "char_drv", &char_fops)
  → 커널 내부에 major와 장치 동작을 등록
       ├─ /proc/devices를 읽으면 "236 char_drv"로 등록 정보 표시
       └─ 문자 노드 /dev/char_drv의 번호 (236, 0)로 open 요청을 연결
```

`/proc/devices`는 procfs가 제공하는 가상 정보 파일이다. 읽을 때 커널의 등록 정보를 텍스트로 보여준다. 이번 드라이버가 `/proc/char_drv` 파일을 만든 것은 아니며, `register_chrdev()`가 `/proc/devices`에 문자열을 직접 쓰는 것도 아니다. 대상 소스의 `fs/proc/devices.c`는 `chrdev_show()`를 호출하고, `fs/char_dev.c`의 그 함수는 커널 내부 등록 목록을 순회해 번호와 이름을 출력한다.

따라서 `236 char_drv`는 major 236의 이름이 등록됐다는 뜻이다. 이 줄만으로 minor별 장치, 실제 센서 연결, `/dev` 노드 존재, open 성공까지 확인할 수는 없다. `/proc` 전체가 장치 목록인 것도 아니다. 프로세스 정보 등 여러 커널 정보를 제공하며 여기서 사용하는 파일이 `/proc/devices`인 것이다.

`/dev/char_drv`는 앱이 open할 **문자 장치 특수 파일**, 즉 장치 노드다. `mknod ... c 236 0`은 파일시스템에 종류·번호를 가진 노드를 만든다. 일반 파일처럼 센서 데이터가 그 안에 저장되는 구조가 아니라, 커널이 이 번호를 보고 해당 파일 연산으로 요청을 전달한다. 노드에는 소유자·권한도 있으므로 앱 접근 권한 검사에도 사용된다.

두 이름을 같게 쓴 것은 사람이 연결을 알아보기 쉽게 하려는 선택이다. `/dev`에서 어느 드라이버를 호출할지는 노드 이름과 등록 이름의 문자열 일치가 아니라 **장치 종류와 major/minor**로 연결된다. `mknod`만 실행해서 드라이버를 등록하거나 콜백 코드를 만드는 것은 아니다. 이번 수동 방식과 달리 장치 모델·devtmpfs·udev를 활용하는 드라이버에서는 노드 생성도 자동화할 수 있지만 현재 구현에는 없다.

이번에 확인한 상태 변화를 보면 구분이 명확하다.

| 시점 | 커널 등록과 `/proc/devices` | `/dev/char_drv` |
| --- | --- | --- |
| insmod 후 | major 236 등록, `236 char_drv` 표시 | 아직 없음 |
| mknod 후 | 등록 그대로 | `c 236, 0` 노드 존재 |
| 셸 open/close | 해당 연산 표의 open/release 호출 | 접근 입구로 사용 |
| rmmod 후 | 등록 해제, 표시 사라짐 | 노드는 그대로 남음 |
| rm 후 | 이미 등록 해제됨 | 노드도 사라짐 |

rmmod 후 노드가 남았다고 드라이버가 살아 있는 것은 아니다. 반대로 노드만 삭제해도 모듈·장치 등록은 해제되지 않는다. 번호가 나중에 다른 장치에 재할당될 수도 있으므로 재실습에서는 현재 등록과 노드 번호를 다시 대조한다. 이번에는 등록 해제 후 노드를 별도로 삭제해 정리했다.

사용자가 요청한 짧은 네이밍 기준에 따라 파일·모듈·등록 이름은 `char_drv`, 함수는 `char_open`, `char_release`처럼 쓴다. 실제 LED 등 장치를 다룰 때는 `led_open`처럼 장치명과 동작을 조합한다. `bsp_` 접두사는 동작에 필요하지 않다.

## 5. VEDA 코드와의 차이

비교 대상은 [VEDA skeleton/sk.c](../../../VEDA/리눅스커널/수업코드/skeleton/sk.c) 전체다. 읽기 확인한 참고 저장소 커밋은 `c2f2ace95af528c6f806c8fc273f02bdb10120c6`이다. 기존 코드를 복사한 뒤 이름만 바꾼 것이 아니라, 같은 `register_chrdev()` 흐름에서 이번 단계의 범위를 좁혔다.

| 확인한 차이 | 현재 구현과 이유 |
| --- | --- |
| 수업은 register_chrdev 반환값을 result에 저장하고 init에서 항상 0 반환 | 현재는 음수 반환을 검사하고 로드 실패로 전달. 등록 실패를 성공으로 숨기지 않음 |
| 수업 연산 표에는 .owner가 없음 | THIS_MODULE 추가로 파일 사용 중 모듈 참조 유지 |
| 수업의 sk_fops와 콜백은 static/const 제한 없이 선언 | 내부 콜백은 static, 표는 static const로 범위·변경 의도를 명시. 기존 형식도 그 자체로 잘못된 것은 아님 |
| 수업은 read/write/ioctl도 등록 | 먼저 open/release 검증 후 현재 write까지 확장. 현재 read 고정 응답까지 추가, ioctl은 이후 단계 |
| 수업에는 사용자 복사용 uaccess·ioctl 관련 헤더가 있음 | 초기 단계에서 제외, write 확장 시 linux/uaccess.h 추가 |
| 수업의 printk와 현재 pr_info/pr_err, SK와 char_drv 등 이름 차이 | 로그 수준·식별 목적과 코드 범위를 드러내는 표현. 등록 방식 자체는 유지 |

수업 read/write에는 고정 크기 버퍼에 전달받은 count로 복사하고 복사 결과를 검사하지 않는 부분이 있다. 데이터 전달을 추가할 때 버퍼 길이, 사용자 복사 결과, 읽기 위치와 반환값을 별도로 검토한다. 현재는 아래 길이·복사 검사를 포함한 write를 구현했고 read는 고정 응답과 파일 위치를 처리하도록 추가했다.

사용자 앱 비교 대상은 같은 참고 커밋의 [skeleton/sk_app.c](../../../VEDA/리눅스커널/수업코드/skeleton/sk_app.c)다. 수업과 동일하게 main에서 open·fd 오류 검사·close를 사용한다. 수업의 ioctl 메뉴·scanf·관련 헤더는 현재 드라이버에 ioctl이 없어 제외했고, 경로를 `/dev/char_drv`로 맞췄다. 수업은 close 반환값을 검사하지 않지만 현재 앱은 검사한다. open 실패 시 `exit(-1)` 대신 main에서 1을 반환해 실패를 표시한다. 두 방식 모두 프로그램을 종료하지만 현재 방식은 작은 main의 제어 흐름을 바로 보여준다.

ioctl 비교: 수업의 [sk_ioctl.h](../../../VEDA/리눅스커널/수업코드/skeleton/sk_ioctl.h)는 `_IO` 기반 SPEED_UP/DOWN을 정의하고 sk.c는 switch로 처리한다. 같은 구조를 유지하되 현재 저장 버퍼에 의미 있는 CLEAR로 바꿨다. 공통 헤더가 ioctl 매크로 헤더를 직접 포함하게 했고, 수업의 알 수 없는 명령 기본 반환값 0은 현재 -ENOTTY로 변경했다. 앱은 메뉴 입력 대신 echo 뒤 명령 성공과 EOF를 확인한다.

## 6. 빌드·실행·검증

### 선행 조건

[커널 빌드](../../docs/kernel-build.md)와 [hello 모듈](../hello_module/kernel-module.md) 실습 환경을 사용한다. WSL에서 소스 커밋 `b3495ea401d709af0701a85babcba5e3d788a602`의 커널 전체 빌드가 완료되어 `out/kernel-rpi4`에 설정·생성 헤더·Module.symvers가 남아 있어야 한다. 설정은 `bcm2711_defconfig`, `CONFIG_LOCALVERSION="-v8-bsp-study01"`, `CONFIG_LOCALVERSION_AUTO=n`이며 각 make에 `LOCALVERSION=`을 전달한다.

WSL의 ARM64 GCC 11.4.0·Make·file·modinfo(kmod)를 사용한다. Pi는 `6.18.59-v8-bsp-study01`로 부팅한 상태이고 현재 접속은 사용자 지정 SSH 별칭 `pi`를 사용한다. 과거 직접 접속 주소는 `nakta@192.168.45.224`다. 별칭의 실제 SSH 설정은 별도 열람하지 않았다. 아래 명령은 사용자가 단계별로 수행할 절차이며 이번 문서 작업에서 실행하지 않았다.

### 먼저 WSL 빌드 결과 확인

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 위 커널 빌드 환경 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make
echo $?
file char_drv.ko
modinfo -F vermagic ./char_drv.ko
```

성공 기준은 make 직후 종료 상태 0, ARM aarch64 relocatable 모듈, vermagic의 커널 버전 부분이 `6.18.59-v8-bsp-study01`인 것이다. 버전 문자열뿐 아니라 같은 소스·설정·Module.symvers를 사용해야 하며 실제 로드도 별도로 확인해야 한다.

### Pi 전송·등록 확인

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 빌드와 메타데이터 확인 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
ssh pi 'mkdir -p ~/bsp/modules'
scp char_drv.ko pi:bsp/modules/char_drv.ko
ssh pi
```

**Pi · 작업 디렉터리: `~/bsp/modules` · 전송 완료 후**

```bash
cd ~/bsp/modules
uname -r
modinfo -F vermagic ./char_drv.ko
```

대상 커널 버전이 일치하는지 확인한 뒤 다음 등록 확인으로 넘어간다. 이미 로드했다면 중복 insmod하지 않는다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 대상 버전 일치 확인 후**

```bash
cd ~/bsp/modules
sudo insmod ./char_drv.ko
lsmod | grep '^char_drv '
grep -w char_drv /proc/devices
sudo dmesg | grep 'char_drv:'
```

모듈 목록, `/proc/devices`의 major와 이름, `loaded, major=...` 로그를 확인한다. 이 결과는 **등록 성공**의 근거이며 open/release는 다음 절차로 따로 확인한다. 실제 사용자가 실행한 Pi 경로는 `~/bsp/modules/char_drv.ko`다. 위 전송 명령은 그 경로에 맞춘 재현 안내이며 당시 scp 명령 출력은 받지 않았다.

### 노드 생성과 셸 open/close

**Pi · 작업 디렉터리: `~/bsp/modules` · 모듈 등록과 실제 major 확인 후**

```bash
cd ~/bsp/modules
ls -l /dev/char_drv
```

노드가 없을 때만 생성한다. 아래 236은 이번에 실제 관측한 major다. 다시 로드할 때에는 `/proc/devices`와 로그를 확인하고 현재 값으로 바꾼다. 기존 노드가 있다면 종류·major/minor를 먼저 대조하며 덮어쓰지 않는다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 노드 부재와 현재 major가 236임을 확인한 경우**

```bash
cd ~/bsp/modules
sudo mknod /dev/char_drv c 236 0
ls -l /dev/char_drv
```

`c`는 문자 장치, 236은 major, 0은 minor다. 관측된 `crw-r--r-- ... root root 236, 0`에서 맨 앞 c와 두 번호로 장치 노드를 확인했다. 노드 생성 자체는 open 콜백을 호출하지 않는다. 일반 사용자에게 쓰기 권한이 없는 상태이므로 이번 읽기·쓰기 모드 open은 sudo로 실행한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 등록과 노드 번호 일치 확인 후**

```bash
cd ~/bsp/modules
sudo bash -c 'exec 3<> /dev/char_drv; exec 3>&-'
echo $?
sudo dmesg | grep 'char_drv:'
```

`exec 3<>`는 Bash에서 읽기·쓰기 모드로 장치를 열어 fd 3에 연결한다. `exec 3>&-`는 그 fd를 닫는다. 데이터 read/write는 수행하지 않는다. 이번에는 종료 상태 0과 `opened`, `released` 로그를 함께 확인해 콜백 실행을 검증했다. 이 복합 명령의 종료 상태만으로 앞선 open 성공을 단정하지 않고 로그도 대조한다. C 앱의 open/close와 같은 커널 파일 연산 경로를 셸에서 먼저 확인한 것이다.

### 등록 해제와 노드 삭제

**Pi · 작업 디렉터리: `~/bsp/modules` · 모듈 로드 상태이며 열린 파일 참조가 없을 때**

```bash
cd ~/bsp/modules
sudo rmmod char_drv
lsmod | grep '^char_drv '
grep -w char_drv /proc/devices
sudo dmesg | grep 'char_drv:'
ls -l /dev/char_drv
```

실제로 모듈·등록 항목은 사라지고 `unloaded` 로그가 추가됐지만 `/dev/char_drv`는 남았다. 검색 결과가 없을 때 grep 종료 상태 1은 항목을 찾지 못했다는 뜻이다. dmesg는 이력이므로 이전 로그도 남는다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 모듈 해제 후 남은 실습 노드를 확인한 상태**

```bash
cd ~/bsp/modules
sudo rm /dev/char_drv
ls -l /dev/char_drv
```

마지막 ls에서 파일 부재를 확인했다. 이는 의도한 정리 결과이며 장애가 아니다. 재실습 시에는 모듈 로드·현재 major 확인·노드 생성부터 다시 진행한다.

### 현재 확인된 결과와 남은 검증

2026-11-04 사용자 제공 Pi 실측:

```text
uname -r:
6.18.59-v8-bsp-study01
vermagic:
6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64
lsmod:
char_drv               12288  0
/proc/devices:
236 char_drv

[  741.510635] char_drv: loading out-of-tree module taints kernel.
[  741.512438] char_drv: loaded, major=236
[  941.540220] char_drv: opened
[  941.540329] char_drv: released
[ 1084.810500] char_drv: unloaded
```

- **등록·사용:** major 236/minor 0의 수동 노드 생성, 셸 open/close 종료 상태 0과 두 콜백 로그 확인.
- **정리:** rmmod 후 lsmod와 /proc/devices 검색 결과 없음. 노드는 여전히 `236, 0`으로 존재했고 별도 rm 후 파일 부재 확인.
- **읽기 확인:** 대상 커널의 `include/linux/fs.h`와 `fs/char_dev.c`를 대조했다. 코드 발췌·소스/obj-m 이름·문서 참조도 정적으로 검토했다.
- **남은 범위:** WSL make 종료 상태·file 출력 자체는 미수신. 초기 C 앱의 Pi 실행은 아래처럼 확인했다. write 정상 전달은 뒤의 실측으로 확인했다. 길이 경계·복사 실패·read 확장 버전·ioctl·열린 상태에서 rmmod 거부 시험은 미검증이며 이 단계의 결과에 포함하지 않는다. hello 모듈 이름 변경 후 재검증도 별개다.

외부 모듈 taint는 [hello 모듈 설명](../hello_module/kernel-module.md#검증-결과)과 같은 표시이며 이번 로드 실패가 아니다. 모듈 기능 장애는 관측되지 않았다.

### 초기 open/close 앱의 빌드·전송·실행

이하 명령은 재현 안내다. 사용자 제공 앱 출력·커널 로그는 뒤의 실측 항목에 따로 기록한다. WSL 빌드는 사용자가 확인했다고 보고했으며 원시 빌드 출력 자체는 미수신이다. Pi의 실습 산출물은 앞으로 `~/bsp/` 아래에 모은다. `.ko`는 기존 `~/bsp/modules/`, 앱은 `~/bsp/apps/`를 사용한다. `/dev` 노드는 커널 접근 경로이므로 이 배포 디렉터리와 별개다. 커널·initramfs 등 시스템 설치 경로도 기존대로 사용한다.

**WSL · 작업 디렉터리: 문자 디바이스 예제 · ARM64 크로스 컴파일러 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make app
echo $?
file char_app
```

종료 상태 0과 ARM aarch64 사용자 실행 파일 형식인지 확인한다. 모듈 .ko의 relocatable 형식과 구분한다. 앱은 libc를 사용하는 실행 파일이며 vermagic 검사 대상이 아니다. Pi 로더·glibc 호환성의 배경은 [크로스 컴파일 실습](../hello/cross-compile.md)에 있다.

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 앱 빌드 확인 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
ssh pi 'mkdir -p ~/bsp/apps'
scp char_app pi:bsp/apps/char_app
ssh pi
```

Pi에서는 앞의 등록 절차에 따라 `~/bsp/modules/char_drv.ko`를 로드하고 현재 major를 확인한다. 이미 로드돼 있다면 중복 로드하지 않는다. 이전 실습은 모듈 해제·노드 삭제까지 마쳤으므로 그대로 앱만 실행하면 장치를 열 수 없다. 현재 major와 minor 0으로 노드를 다시 준비한다. 과거 값 236을 확인 없이 재사용하지 않는다.

**Pi · 작업 디렉터리: `~/bsp/apps` · 대상 커널·모듈 등록·노드 번호 일치, 앱 전송 완료 후**

```bash
cd ~/bsp/apps
sudo ./char_app
echo $?
sudo dmesg | grep 'char_drv:'
```

write 추가 전 앱의 예상 출력은 `opened /dev/char_drv: fd=<실제 fd>`와 `closed /dev/char_drv`, 종료 상태는 0이다. 현재 앱에는 그 사이 wrote 출력도 추가된다. fd가 꼭 3이라고 가정하지 않는다. 커널 로그에는 이번 실행 시점의 opened/released 한 쌍이 추가되는지 이전 기록과 구분해 확인한다. 앱 검증 후에는 앞의 등록 해제·노드 삭제 절차로 정리한다. 이번 사용자 실측으로 C 앱과 드라이버 사이의 open/release 연결을 확인했다. C 앱 검증 이후 모듈·노드 재정리 결과는 아직 받지 않았다.

### C 앱 실제 검증 결과

2026-11-04 write 추가 전 사용자 제공 open/close 앱 출력이다. 현재 write 추가 버전의 성공 결과와 구분한다.

```text
opened /dev/char_drv: fd=3
closed /dev/char_drv
0

[ 3480.821015] char_drv: loaded, major=236
[ 3697.620848] char_drv: opened
[ 3697.620976] char_drv: released
```

앱에서 받은 fd는 3이었다. 앱 출력·종료 상태와 당시 실행 시점의 커널 콜백 로그를 대조해 open/close를 확인했다. 과거 셸 시험의 941초 로그와 이번 C 앱 시험의 3697초 로그를 구분한다.

### write 확장 버전의 빌드·배포·검증

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 기존 커널 전체 빌드 환경 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make
echo $?
file char_drv.ko char_app
modinfo -F vermagic ./char_drv.ko
```

make는 모듈과 앱을 모두 빌드한다. 모듈과 앱의 ARM64 형식, 종료 상태 0, 대상 커널과 vermagic 일치를 확인한 뒤 전송한다.

**WSL · 작업 디렉터리: 문자 디바이스 예제 · SSH 별칭 pi와 scp 사용 가능, 빌드 확인 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make
make deploy
ssh pi
```

현재 make deploy는 빌드된 두 파일만 전송한다. make로 먼저 최신 산출물을 준비해야 한다. 아직 사용자가 실행한 결과는 받지 않았다. 디스크의 .ko 파일을 덮어써도 이미 로드된 커널 코드는 바뀌지 않는다. 앱이 종료된 상태에서 기존 모듈을 해제하고 새 모듈을 로드한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 기존 char_drv가 로드되어 있고 열린 파일이 없을 때**

```bash
cd ~/bsp/modules
sudo rmmod char_drv
sudo insmod ./char_drv.ko
grep -w char_drv /proc/devices
ls -l /dev/char_drv
```

이미 해제한 상태라면 rmmod를 생략한다. 현재 major를 확인해 노드의 번호와 대조한다. 노드가 없으면 앞의 mknod 절차로 만들고, 번호가 달라졌다면 기존 실습 노드를 삭제한 후 현재 major/minor 0으로 다시 만든다. 확인 없이 과거 major 236을 고정하지 않는다.

**Pi · 작업 디렉터리: `~/bsp/apps` · 새 모듈과 앱 배포·노드 대조 완료 후**

```bash
cd ~/bsp/apps
sudo ./char_app
echo $?
sudo dmesg | grep 'char_drv:'
```

예상 앱 출력은 opened, `wrote 14 bytes`, closed와 종료 상태 0이다. 새 실행 시점의 커널 로그에서 opened → `received 14 bytes: Hello, driver!` → released 순서를 확인한다. 이전 로그와 구분한다. 정상 write는 아래 실측으로 확인했다. 이번 실행 직후 echo $? 출력은 받지 않아 종료 상태 0을 실측으로 기록하지 않는다. 초과 길이 거부·복사 실패 경로는 아직 미검증이다. 길이 경계 검증은 미수행으로 남기고 read로 진행한다.

### write 정상 전달 실측

2026-11-04 사용자 제공 Pi 출력이다. 기존 모듈을 해제하고 새 모듈을 로드한 뒤 major 236과 노드 번호를 대조했다. 이번에는 노드를 다시 생성했지만, 번호가 같은 기존 노드는 재생성 없이 사용할 수 있다.

**실제 실행 위치: Pi · `~/bsp/modules`**, 앱은 `sudo ../apps/char_app`으로 실행했다.

```text
opened /dev/char_drv: fd=3
wrote 14 bytes
closed /dev/char_drv

[ 4638.850499] char_drv: unloaded
[ 4646.388443] char_drv: loaded, major=236
[ 4745.027807] char_drv: opened
[ 4745.028071] char_drv: received 14 bytes: Hello, driver!
[ 4745.028115] char_drv: released
```

앱에서 14바이트 반환을 확인했고 커널 로그의 길이·문자열도 일치했다. 사용자 메모리 → copy_from_user → 커널 버퍼 → 로그 → 처리 길이 반환의 정상 경로를 검증한 결과다. 이 당시 결과는 write 단독 단계이며 이후 고정 응답 read 확인과 현재 echo 구현 상태는 별도로 구분한다. 단순 명령·이름 오타는 트러블슈팅 사례로 기록하지 않는다.

### 이전 고정 응답 read 버전의 검증 절차

모듈과 앱이 모두 바뀌었으므로 기존 write 버전의 로드된 코드를 새 read 버전으로 교체해야 한다.

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 커널 빌드 환경·SSH 별칭 pi 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make
make deploy
ssh pi
```

**Pi · 작업 디렉터리: `~/bsp/modules` · 기존 모듈 로드 상태이며 앱 실행이 끝난 경우**

```bash
cd ~/bsp/modules
sudo rmmod char_drv
sudo insmod ./char_drv.ko
grep -w char_drv /proc/devices
ls -l /dev/char_drv
```

이미 해제했다면 rmmod는 생략한다. 현재 major와 노드 번호를 대조하고, 없거나 다르면 앞의 노드 준비 절차로 현재 major/minor 0을 맞춘다.

**Pi · 작업 디렉터리: `~/bsp/apps` · 새 모듈 로드·노드 대조 완료 후**

```bash
cd ~/bsp/apps
sudo ./char_app
echo $?
sudo dmesg | grep 'char_drv:'
```

예상 앱 출력은 opened → wrote 14 bytes → `read 19 bytes: Hello from driver!` → `read EOF` → closed이며 종료 상태 0이다. fd 번호는 달라질 수 있다. 새 커널 로그에서 opened → received 14 bytes → sent 19 bytes → released를 확인한다. EOF 호출은 sent 로그를 추가하지 않는다. 사용자 앱 출력으로 19바이트 응답과 EOF를 확인했다. 이 실행의 커널 로그·종료 상태 출력은 별도 전달받지 않았으며 현재 echo 버전의 결과와 구분한다.

### 현재 echo 버전 검증 절차

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 기존 커널 빌드 환경과 SSH 별칭 pi 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make
make deploy
ssh pi
```

**Pi · 작업 디렉터리: `~/bsp/modules` · 기존 모듈이 로드돼 있고 앱이 종료된 상태**

```bash
cd ~/bsp/modules
sudo rmmod char_drv
sudo insmod ./char_drv.ko
grep -w char_drv /proc/devices
ls -l /dev/char_drv
```

기존 모듈이 없다면 rmmod는 생략한다. 노드가 없거나 현재 major와 다르면 앞의 노드 준비 절차로 맞춘다. 종류·major·minor가 같으면 기존 노드를 유지한다. 새 모듈 로드 시 저장 버퍼도 초기 상태로 돌아간다.

**Pi · 작업 디렉터리: `~/bsp/apps` · 새 모듈 로드와 노드 확인 후, 다른 앱이 쓰지 않는 상태**

```bash
cd ~/bsp/apps
sudo ./char_app
echo $?
sudo dmesg | grep 'char_drv:'
```

예상 앱 출력은 다음과 같다. fd는 실제 할당값으로 달라질 수 있다.

```text
opened /dev/char_drv: fd=3
wrote 14 bytes
read 14 bytes
read EOF
reply: Hello, driver!
echo matched: 14 bytes
closed /dev/char_drv
```

종료 상태 0, 새 커널 로그의 received 14 → sent 14와 콜백 순서를 확인한다. 사용자 앱 출력으로 echo 길이·내용 일치는 확인했다. 해당 실행의 종료 상태·커널 로그는 별도 미수신이며 동시성 검증은 미수행이다. 길이 경계 검증은 사용자가 생략하기로 했으며 다음 진행 조건으로 요구하지 않는다.

### echo 실측 결과

사용자 제공 앱 출력에서 wrote 14 → read 14 → EOF → `reply: Hello, driver!` → `echo matched: 14 bytes` → closed를 확인했다. 정상 경로에서 왕복 데이터 길이·바이트 내용이 일치했다. 해당 결과는 스타일 정리 전 실행 결과이며 mutex의 동시 접근 보호 시험까지 수행한 것으로 기록하지 않는다.

### ioctl CLEAR 버전의 빌드·검증

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 커널 빌드 환경과 SSH 별칭 pi 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make
make deploy
ssh pi
```

make가 성공한 경우에만 make deploy로 넘어간다. 현재 deploy는 전송 전용이다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 기존 char_drv가 로드돼 있고 앱이 종료된 상태**

```bash
cd ~/bsp/modules
sudo rmmod char_drv
sudo insmod ./char_drv.ko
grep -w char_drv /proc/devices
ls -l /dev/char_drv
```

이미 해제했다면 rmmod는 생략한다. 노드 종류·major·minor가 일치하면 재사용하고, 없거나 다를 때만 현재 번호로 준비한다.

**Pi · 작업 디렉터리: `~/bsp/apps` · 새 모듈과 노드 확인 후, 다른 앱이 쓰지 않는 상태**

```bash
cd ~/bsp/apps
sudo ./char_app
echo $?
sudo dmesg | grep 'char_drv:'
```

예상 앱 출력은 기존 `echo matched: 14 bytes` 뒤 `buffer cleared`, `read after clear: EOF`, closed다. 종료 상태 0과 새 커널 로그의 `buffer cleared`를 함께 확인한다. 사용자 앱 출력으로 ioctl 명령 성공과 초기화 후 EOF를 확인했다. 빌드 원시 출력·이번 실행의 종료 상태·커널 로그·미지원 명령 경로는 별도 미확인이다.

### ioctl CLEAR 실측 결과

사용자가 Pi의 `~/bsp/modules`에서 `sudo ../apps/char_app`을 실행한 출력이다.

```text
opened /dev/char_drv: fd=3
wrote 14 bytes
read 14 bytes
read EOF
reply: Hello, driver!
echo matched: 14 bytes
buffer cleared
read after clear: EOF
closed /dev/char_drv
```

앱이 write/read의 길이·내용 일치를 확인한 뒤 ioctl 성공 경로를 통과했고, CLEAR가 현재 파일 위치를 0으로 되돌린 이후에도 read가 0을 반환했다. 따라서 단일 앱의 정상 데이터 왕복과 버퍼 초기화 제어를 확인했다. 이 출력만으로 종료 상태 0·커널 로그·미지원 명령 오류·동시성 시험까지 확인했다고 기록하지 않는다.

### 현재 기능별 로그 형식 (실행 전)

앱은 기능별 제목과 빈 줄, 커널 로그는 각 줄의 태그로 구분한다. 오류에도 기능 태그를 붙이고 색상 코드는 사용하지 않는다. 데이터 처리·반환값·호출 순서는 유지한다. 위 실측 로그는 출력 형식 변경 전 결과로 보존한다.

현재 앱 정상 경로의 예상 출력:

```text
=== OPEN ===
device: /dev/char_drv
fd: 3

=== WRITE ===
sent: 14 bytes

=== READ ===
received: 14 bytes
EOF
reply: Hello, driver!

=== VERIFY ===
echo matched: 14 bytes

=== IOCTL: CLEAR ===
buffer cleared
read after clear: EOF

=== CLOSE ===
device closed
```

현재 커널 로그 정상 경로의 예상 형식(시간 접두사 생략):

```text
char_drv: [MODULE] loaded, major=236
char_drv: [OPEN] opened
char_drv: [WRITE] received 14 bytes: Hello, driver!
char_drv: [READ] sent 14 bytes
char_drv: [IOCTL] buffer cleared
char_drv: [CLOSE] released
```

fd·major는 실제 할당값에 따라 달라진다. EOF는 기존처럼 별도 커널 로그를 추가하지 않는다. 모듈 해제 시에는 `[MODULE] unloaded`가 출력된다. 전체 검색은 기존 `sudo dmesg | grep 'char_drv:'`로 가능하다. 특정 기능 검색에서 대괄호를 문자 그대로 찾으려면 `grep -F`를 사용한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 새 버전 모듈·앱 배포, 모듈 교체·노드 확인과 앱 실행 후:**

```bash
cd ~/bsp/modules
sudo dmesg | grep -F 'char_drv: [WRITE]'
```

기능별 출력 형식은 아직 실행 전이다. 빌드와 전송은 기존대로 WSL에서 make 성공 후 make deploy로 진행한다. 로드된 모듈을 교체해야 새 커널 태그가 적용되며 이번 수정만을 확인하기 위한 자동 실행은 하지 않았다.
