# 문자 디바이스 — 등록·데이터 왕복·ioctl

이 문서는 현재 [char_drv.c](char_drv.c), [char_app.c](char_app.c), [char_ioctl.h](char_ioctl.h), [Makefile](Makefile)을 함께 읽기 위한 해설이다. 개념은 등록 → 파일 연산 연결 → 데이터 전달 → 제어 요청 순서로 설명한다. 실행 결과와 확인 범위는 [검증 현황](#검증-현황)에 모았다.

목차:

- [1. 구현 기능과 범위](#1-구현-기능과-범위)
- [2. 전체 실행 흐름](#2-전체-실행-흐름)
- [3. 현재 코드 해설](#3-현재-코드-해설)
- [4. 헷갈리기 쉬운 개념과 질문](#4-헷갈리기-쉬운-개념과-질문)
- [5. VEDA 코드와의 차이](#5-veda-코드와의-차이)
- [6. 빌드·배포·실행·정리와 검증](#6-빌드배포실행정리와-검증)

## 1. 구현 기능과 범위

사용자 앱이 보낸 짧은 메시지를 커널에 저장하고, 다시 읽어 길이·내용을 비교한 뒤 ioctl로 저장 내용을 초기화한다. 실제 센서나 GPIO 대신 메모리 버퍼를 사용해 사용자/커널 경계를 학습한다.

| 파일 | 역할 |
| --- | --- |
| `char_drv.c` | 문자 장치 등록, open/write/read/ioctl/release, 공유 버퍼와 mutex |
| `char_app.c` | open → write → read/EOF → echo 비교 → CLEAR → read/EOF → close |
| `char_ioctl.h` | 앱·드라이버가 함께 사용하는 CLEAR 명령 번호 |
| `Makefile` | WSL의 ARM64 모듈·앱 빌드와 Pi 전송 |

저장 용량은 64바이트이며, 문자열 끝 NUL을 남겨 한 번에 최대 63바이트를 받는다. 각 write는 이전 메시지를 대체한다. 버퍼는 모듈 전체에서 하나이고 읽기 위치는 열린 파일별로 관리한다. 출력과 비교는 짧은 텍스트를 기준으로 한다.

범위 밖 기능은 자동 장치 노드 생성, 데이터 이어붙이기·큐·앱별 저장 공간, 하드웨어 제어, ioctl의 데이터 인자 전달·32비트 앱 호환 처리다.

## 2. 전체 실행 흐름

```text
Pi에서 insmod char_drv.ko
  → 커널이 char_init() 호출
  → register_chrdev(0, "char_drv", &char_fops)
  → 동적 major 할당, 장치 연산 등록

현재 major와 minor 0의 /dev/char_drv 노드 준비 (별도 작업)

앱 main()
  → open("/dev/char_drv", O_RDWR)
      → 커널이 노드 번호에 연결된 char_open() 호출
      → 성공하면 앱에 fd 반환
  → write(fd, message, count)
      → char_write(): 사용자 데이터 복사 → 공유 버퍼·길이 저장
      → 이 파일의 위치 0으로 초기화 → 처리 길이 반환
  → read 반복
      → char_read(): 저장 데이터 복사 → 읽은 만큼 위치 증가
      → 끝에 도달한 다음 read는 0(EOF) 반환
  → 앱이 보낸 데이터와 받은 데이터의 길이·내용 비교
  → ioctl(fd, CHAR_CLEAR)
      → char_ioctl(): 버퍼·길이 초기화, 이 파일 위치 0
  → read(): 처음 위치에서도 데이터가 없으므로 EOF
  → close(fd)
      → 마지막 열린 파일 참조 해제 시 char_release() 호출

앱 종료 후 rmmod char_drv
  → 커널이 char_exit() 호출
  → unregister_chrdev(): 등록 해제
  → 별도 rm으로 /dev/char_drv 노드 정리
```

앱이 드라이버 함수를 직접 호출하는 것은 아니다. 커널이 파일 연산을 처리하다가 등록된 함수 포인터를 호출한다. 모듈 로드 자체는 장치 open을 수행하지 않으며 앱 종료도 모듈 해제를 의미하지 않는다.

## 3. 현재 코드 해설

### 헤더·이름·공유 상태

```c
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "char_ioctl.h"

#define DEVICE_NAME "char_drv"
#define DRIVER_AUTHOR "Nakta"
#define DRIVER_DESC "Character device read and write practice"

static int major;
static char saved_data[64];
static size_t saved_length;
static DEFINE_MUTEX(data_lock);
```

`fs.h`는 파일 연산 구조체와 문자 장치 등록 API, `module.h`·`init.h`는 모듈 메타데이터와 초기화·종료 관련 선언을 제공한다. 오류 코드는 `errno.h`, 로그는 `printk.h`, 사용자 메모리 복사는 `uaccess.h`, 잠금은 `mutex.h`, 커널 버퍼 처리는 `string.h`를 사용한다. 공통 명령은 `char_ioctl.h`에서 가져온다.

`DEVICE_NAME`은 등록·해제에 사용할 이름이다. 작성자·설명 매크로는 마지막 `MODULE_*` 메타데이터에 들어간다. 이름에 붙는 `bsp_`는 필수 문법이 아니므로 현재는 장치·역할에 맞는 짧은 이름을 사용한다.

파일 범위의 `major`·`saved_data`·`saved_length`는 함수가 끝나도 유지되며 모듈이 존재하는 동안 사용할 수 있다. 명시적 초기값이 없는 저장 배열·길이는 0으로 초기화된다. `static`은 이 이름들의 직접 참조 범위를 해당 C 파일로 제한한다. `DEFINE_MUTEX`는 사용할 수 있는 상태로 mutex를 선언·초기화한다. 잠금이 필요한 이유와 보호 범위는 [mutex 설명](#mutex)에 모았다.

### file_operations: 사용자 연산과 콜백 연결

```c
static const struct file_operations char_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_release,
	.write = char_write,
	.read = char_read,
	.unlocked_ioctl = char_ioctl,
};
```

`struct file_operations`는 파일 연산별 호출 대상을 담는 표다. 현재는 open·release·write·read·ioctl을 모두 연결했다. `.open = char_open`처럼 멤버 이름을 지정하는 것은 C의 지정 초기화 문법이다.

`char_open`은 여기서 함수 포인터로 변환돼 저장된다. `&char_open`도 가능하지만 `char_open()`은 지금 함수를 호출하는 표현이므로 다르다. `static` 함수도 이 표에 주소를 전달하면 커널이 호출할 수 있다. 표의 `const`는 초기화 후 표를 바꾸지 않겠다는 뜻이며 콜백 내부 상태까지 불변으로 만들지는 않는다.

`.owner = THIS_MODULE`은 이 연산 표의 소유 모듈이다. 커널이 파일 연산을 사용하는 동안 모듈 참조를 유지해 사용 중인 콜백 코드가 일반적인 rmmod로 사라지는 일을 막는다. 선언하지 않은 다른 연산 멤버는 0으로 초기화된다.

### 모듈 등록과 해제

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

`module_init()`·`module_exit()`는 커널이 로드·해제 때 호출할 함수를 연결한다. 이 줄에서 함수를 즉시 호출하는 것은 아니다. `__init`·`__exit`의 섹션 의미, `MODULE_*`, 로그 수준은 [hello 모듈 해설](../hello_module/kernel-module.md#c-코드의-핵심)에 설명했다. `pr_info()`는 INFO이고 `printk(KERN_INFO ...)`에 대응한다. NOTICE 수준의 짧은 표현은 `pr_notice()`다.

등록 인자를 구분해서 읽는다.

| 인자 | 의미 |
| --- | --- |
| `0` | major를 고정하지 않고 동적 할당 요청 |
| `DEVICE_NAME` | `/proc/devices`에서 식별할 등록 이름 |
| `&char_fops` | 연산 표 객체의 주소 전달 |

`&char_fops`는 구조체 객체 포인터, 표 안의 `char_open`은 함수 포인터다. 가리키는 대상이 다르다.

이 동적 등록 호출은 성공 시 양수 major, 실패 시 음수 오류를 반환한다. 고정 major 요청의 성공 반환값 0과 혼동하지 않는다. 실패하면 init에서도 같은 음수 오류를 반환해 로드를 실패시키고, 성공한 경우에만 loaded 로그와 init 성공 값 0을 반환한다. 즉 **register_chrdev의 성공 값과 init의 성공 값은 다르다.**

대상 커널의 register_chrdev는 minor 0~255 범위를 등록한다. 현재는 minor 0만 사용하고 minor별 동작을 구분하지 않는다. 할당된 major는 종료 시 unregister_chrdev에 다시 전달한다. 이 해제 API의 반환형은 void다.

init 실패 후 exit가 자동으로 정리해 주지는 않는다. 현재는 등록 후 추가로 실패할 작업이 없지만, 자원 확보 단계를 확장하면 실패 지점까지 확보한 자원을 init 내부에서 되돌려야 한다. 커널 등록과 노드의 관계는 [장치 등록과 접근 경로](#device-registration)를 참고한다.

### open과 release

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

`inode`는 파일 자체의 커널 정보, `filp`는 열린 파일 상태를 가리킨다. 현재 콜백은 로그만 남겨 인자를 사용하지 않지만 file_operations의 콜백 형식에 맞게 받는다.

`char_open()`의 0은 열기 성공을 커널에 알린다. 앱이 받는 fd 번호를 이 함수가 직접 반환하는 것은 아니다. 오류를 반환하려면 음수 오류 코드를 사용하며, 커널이 나머지 open 처리를 마치면 앱에 fd를 전달한다.

`char_release()`는 열린 파일의 마지막 참조가 정리될 때 호출된다. 단순 open/close에서는 close 뒤 호출되지만 dup·fork 등으로 참조를 공유하면 각 close마다 반드시 호출되지는 않는다. 현재 정리할 개별 파일 자원이 없어 0을 반환한다. release의 반환값을 앱 close의 오류 전달 수단으로 삼지는 않는다.

### write: 사용자 데이터 복사와 저장

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
```

인자는 `filp`(열린 파일 상태), `buf`(사용자 주소), `count`(요청 바이트 수), `f_pos`(파일 위치 포인터)다. 현재 filp 자체는 사용하지 않는다. `__user`는 사용자 주소임을 나타내는 커널 표기로, 자동 복사를 수행하지 않는다. 사용자 포인터를 일반 커널 문자열처럼 직접 역참조하지 않고 copy_from_user로 옮긴다.

`size_t`는 크기를 표현하는 부호 없는 형이고 `ssize_t`는 처리 길이와 음수 오류를 함께 나타내는 부호 있는 반환형이다. `loff_t *`는 파일 위치를 가리킨다.

처리 단위를 따라 읽으면 다음과 같다.

1. 0바이트는 상태 변경 없이 0 반환. 버퍼 크기 이상이면 `-EMSGSIZE`로 거부한다. 긴 요청을 조용히 잘라서 성공시키지 않는 실습 규약이다.
2. 임시 배열 data에 사용자 데이터를 복사한다. copy_from_user의 반환값은 **복사하지 못한 바이트 수**이므로 0만 완전 성공이다. 실패 시 `-EFAULT`로 끝내고 기존 저장 내용은 건드리지 않는다. 복사 API가 대상 배열 크기 검사를 대신하지 않는다.
3. 임시 배열에 NUL을 붙인 뒤 mutex 안에서 저장 배열과 길이를 함께 교체한다. count + 1은 종료 NUL까지 복사하기 위함이고 saved_length에는 실제 payload 길이만 저장한다. 커널 배열끼리 복사할 때는 memcpy를 사용한다.
4. 같은 열린 파일에서 새 메시지를 처음부터 읽도록 `*f_pos = 0`으로 둔다. 일반 파일의 write 위치 규칙을 그대로 따르는 것이 아니라 이 장치의 동작 정의다. 다른 열린 파일의 위치는 초기화하지 않는다.
5. 잠금을 풀고 처리 길이 count를 반환한다. 커널의 음수 오류는 앱 write에서 -1과 errno로 나타난다.

함수 내부 data는 write가 끝나면 수명이 끝난다. 이후 read에 사용할 수 있도록 파일 범위의 saved_data에 복사한 이유다. 중간 NUL을 포함한 이진 데이터는 길이만큼 저장할 수 있지만 `%s` 로그는 첫 NUL에서 끊기므로 현재 출력 예제는 텍스트에 한정한다. `%zu`는 size_t 출력 형식이다.

### read: 저장 데이터와 파일 위치·EOF

```c
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

read는 `buf`라는 사용자 버퍼에 최대 count바이트를 돌려준다. write와 반대 방향이므로 copy_to_user를 사용한다. 처음에는 위치 0에서 읽고, 다음 호출은 갱신된 위치부터 이어 읽는다.

음수 위치는 `-EINVAL`, 0바이트 요청은 0을 반환한다. 잠금 안에서는 현재 위치가 저장 길이 이상이면 result의 초기값 0으로 끝내 EOF를 알린다. 길이를 loff_t로 변환해 위치와 같은 부호 있는 형으로 비교한다. 현재 저장 용량은 해당 형에 안전하게 들어간다.

요청이 남은 데이터보다 크면 남은 길이로 제한한다. `saved_data + *f_pos`는 배열 시작 주소에서 현재 읽기 위치만큼 이동한 주소다. 예를 들어 5바이트를 읽었다면 다음 호출은 offset 5부터 읽는다.

copy_to_user도 복사하지 못한 바이트 수를 반환한다. 실패하면 `-EFAULT`로 정리하고 위치·성공 로그를 갱신하지 않는다. 일부 복사 후 실패라면 앱 메모리 일부는 이미 변경될 수 있으므로 오류 반환이 복사 취소를 의미하지는 않는다.

성공한 경우 `*f_pos += count`로 포인터가 가리키는 파일 위치를 갱신한다. 포인터 자체를 옮기는 `f_pos += count`와 다르다. 다음 read에서 끝 위치를 만나면 EOF가 된다. 별도의 open으로 다시 열면 새 위치 0부터 읽는다. 명시적인 llseek 구현은 추가하지 않았다. 공통 정리 지점 out의 의미는 [goto 설명](#goto-out)에 모았다.

### ioctl: 공통 명령과 버퍼 초기화

```c
#ifndef CHAR_IOCTL_H
#define CHAR_IOCTL_H

#include <linux/ioctl.h>

#define CHAR_IOC_MAGIC 'C'
#define CHAR_CLEAR _IO(CHAR_IOC_MAGIC, 1)

#endif
```

헤더 가드는 같은 헤더의 중복 처리를 막는다. 앱·드라이버가 동일한 CHAR_CLEAR 정의를 사용해야 하므로 공통 헤더에 둔다. linux/ioctl.h의 매크로는 사용자 공간에서도 사용할 수 있는 정의다. 앱의 ioctl 호출 선언은 별도로 sys/ioctl.h에서 가져온다.

`_IO(type, nr)`는 별도 데이터를 주고받지 않는 제어 명령 번호를 만든다. 여기서는 문자 C와 번호 1을 사용하며 전역 고유 번호를 확보했다는 뜻은 아니다. 요청은 해당 fd의 드라이버가 해석한다. read/write는 payload를 전달하고, 이 ioctl은 저장 내용 초기화를 요청한다.

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

`filp`는 열린 파일, `cmd`는 명령 번호, `arg`는 명령별 부가 인자를 담는 unsigned long이다. 현재 _IO 명령은 arg를 사용하지 않는다. 반환형 long은 unlocked_ioctl 콜백 형식이다. 성공은 0, 지원하지 않는 명령은 `-ENOTTY`다.

CLEAR는 write/read와 같은 mutex로 배열과 길이를 초기화한다. memset은 배열을 0으로 채우고 saved_length도 0으로 만든다. `filp->f_pos = 0`은 이 파일의 위치를 처음으로 되돌린다. 따라서 뒤의 read가 EOF인 이유는 예전 위치가 끝에 남아 있어서가 아니라 처음 위치에서도 데이터가 없기 때문이다. 다른 열린 파일의 위치까지 되돌리지는 않는다.

`.unlocked_ioctl`의 unlocked는 잠금을 사용하지 말라는 의미가 아니다. 공유 상태에 필요한 잠금은 드라이버가 직접 적용해야 한다.

### 사용자 앱: open과 write

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "char_ioctl.h"

#define DEVICE_PATH "/dev/char_drv"

int main(void)
{
    const char message[] = "Hello, driver!";
    size_t count = sizeof(message) - 1;
    int status = 0;

    printf("=== OPEN ===\n");

    int fd = open(DEVICE_PATH, O_RDWR);

    if (fd == -1) {
        perror("[OPEN] " DEVICE_PATH);
        return 1;
    }

    printf("device: %s\nfd: %d\n", DEVICE_PATH, fd);

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

앱은 커널 내부 fs.h 대신 libc·시스템 호출 선언을 제공하는 사용자 헤더를 사용한다. DEVICE_PATH는 앱이 찾을 노드 경로이며 커널 등록 문자열과 별개다. O_RDWR은 읽기·쓰기 모드로 열겠다는 요청이고 open 자체가 데이터를 전송하지는 않는다.

앱 open은 성공하면 0 이상의 fd, 실패하면 -1을 반환한다. fd 0도 유효하므로 `fd <= 0`으로 검사하면 안 된다. perror는 설명과 errno에 해당하는 메시지를 표준 오류로 출력한다. `"[OPEN] " DEVICE_PATH`는 매크로 확장 후 인접 문자열 리터럴이 합쳐지는 C 문법이다. main의 반환값 1은 셸에 실패를 알린다.

message는 포인터가 아니라 배열이므로 sizeof에 끝 NUL도 포함된다. 1을 빼서 실제 텍스트 14바이트만 보낸다. 포인터에 sizeof를 적용해 문자열 길이를 구하는 방법으로 일반화하면 안 된다.

write가 -1인지 먼저 확인한 뒤에만 size_t로 변환해 길이를 비교한다. 성공 반환값은 실제 처리 길이이며 요청보다 작을 수도 있다. 이번 앱은 짧은 전송을 실패로 기록한다. 일반 스트림 앱에서는 남은 데이터를 이어 보내는 설계가 필요할 수 있다. `%zd`는 ssize_t, `%zu`는 size_t 출력 형식이다. 실패 상태를 기록해도 이후 close는 수행한다.

### 사용자 앱: read 누적과 echo 비교

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

read 한 번이 전체 데이터를 반환한다고 가정하지 않고 chunk 결과를 reply에 누적한다. total은 현재 누적 길이다. 복사 전 남은 공간을 검사하고 문자열 출력용 NUL 한 칸을 남긴다. `reply + total`은 다음 저장 주소다. chunk는 문자열로 직접 출력하지 않으므로 그 배열에는 NUL이 필요하지 않다.

while 조건은 반환값을 received에 저장하고 양수인지 검사한다. 양수면 데이터를 누적하고 다시 읽는다. 0이면 EOF라 반복을 정상 종료하고 -1이면 오류다. 복사 후 마지막에 reply[total]에 NUL을 붙여 `%s`로 출력한다. 범위를 넘으면 실패 상태로 반복을 끝내고 정리 경로로 진행한다.

길이 total과 count를 먼저 비교하고 같을 때만 memcmp로 payload를 비교한다. `||`는 왼쪽이 참이면 오른쪽을 평가하지 않는 단락 평가를 하므로 길이가 다를 때 내용 비교는 생략된다. memcmp의 0은 내용 일치다. string.h는 memcpy·memcmp 선언을 제공한다. 이전 오류가 없고 길이·내용이 모두 같아야 echo matched를 출력한다.

### 사용자 앱: CLEAR 확인과 close

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

    printf("\n=== CLOSE ===\n");

    if (close(fd) == -1) {
        perror("[CLOSE] " DEVICE_PATH);
        return 1;
    }

    printf("device closed\n");

    return status;
}
```

echo까지 성공했을 때만 ioctl로 넘어간다. 실패하면 상태 1을 남기고, 성공하면 read를 다시 호출한다. 초기화 후 read의 -1은 오류, 양수는 예상 밖 데이터, 0은 정상 EOF다. 이 검증에서는 다른 앱이 중간에 데이터를 쓰지 않는 조건을 사용한다.

close는 성공 시 0, 실패 시 -1이다. 실패하면 오류를 보고하고 종료하며 같은 fd로 무조건 재시도하지 않는다. Linux에서는 오류가 나도 fd가 이미 해제됐을 수 있기 때문이다. 마지막 return status는 중간 write/read/비교/ioctl 오류가 최종 종료 상태에 반영되게 한다. 앱의 printf·perror는 터미널 출력이고 드라이버의 pr_info는 커널 로그이므로 두 경로를 구분한다.

### Makefile: 모듈·앱·배포의 구분

```make
ifneq ($(KERNELRELEASE),)
obj-m := char_drv.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
APP_CC ?= $(CROSS_COMPILE)gcc
APP_CFLAGS ?= -Wall -Wextra -O0 -g
PI_HOST := pi
PI_MODULE_DIR := bsp/modules
PI_APP_DIR := bsp/apps

.PHONY: all modules app deploy clean

all: modules app

modules:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

app: char_app

char_app: char_app.c char_ioctl.h
	$(APP_CC) $(APP_CFLAGS) $< -o $@

deploy:
	scp char_drv.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp char_app $(PI_HOST):$(PI_APP_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) char_app
endif
```

| 요소 | 현재 의미 |
| --- | --- |
| `ifneq ($(KERNELRELEASE),)` | Kbuild가 읽는 모듈 대상 선언과 사용자의 편의 타깃을 분리 |
| `obj-m := char_drv.o` | char_drv.c를 기반으로 char_drv.ko 생성 |
| `KDIR` | 예제에서 두 단계 위의 Pi 커널 빌드 출력 out/kernel-rpi4 선택 |
| `ARCH`, `CROSS_COMPILE`, `LOCALVERSION=` | 기존 Pi 커널과 같은 ARM64·버전 조건 유지 |
| `APP_CC` | Make의 내장 CC 기본값 대신 ARM64 크로스 GCC를 명시 |
| `APP_CFLAGS` | 앱의 경고·최적화·디버깅 옵션. Kbuild 옵션과 구분 |
| `all`, `modules`, `app` | 기본 make는 둘 다, 나머지는 모듈 또는 앱만 빌드 |
| `char_app.c char_ioctl.h` | 소스 또는 공통 헤더가 바뀌면 앱 재빌드 |
| `$<`, `$@` | 첫 선행 파일 char_app.c와 현재 타깃 char_app |
| `deploy` | 이미 빌드된 두 산출물을 Pi에 전송. 자동 빌드·디렉터리 생성·모듈 교체는 수행하지 않음 |
| `clean` | Kbuild로 외부 모듈 산출물을 정리한 뒤 앱 실행 파일 삭제 |

앱은 작은 단일 C 파일이라 한 레시피에서 컴파일과 링크를 함께 한다. .PHONY의 app은 작업 타깃이고 실제 char_app 파일의 의존성 판단은 유지되므로 무조건 재컴파일하지 않는다. deploy는 매번 전송하며 전송 실패 시 다음 레시피 줄을 실행하지 않지만 이미 전송한 파일은 되돌리지 않는다.

`:=`는 읽는 시점에 값을 계산해 저장하고 `?=`는 정의되지 않은 변수의 기본값을 준다. 빈 값으로 이미 정의된 변수도 ?=가 덮어쓰지 않는다. 일반적으로 명령줄 변수는 Makefile의 보통 := 대입보다 우선한다. 레시피는 탭으로 시작한다. all과 수업의 default는 모두 일반 타깃 이름이며 첫 일반 타깃이 기본 선택되는 원리다. WSL의 uname -r은 Pi 커널이 아니므로 KDIR 계산에 쓰지 않는다.

더 자세한 Kbuild 분기·산출물·clean·증분 빌드는 [hello 모듈의 Makefile 해설](../hello_module/kernel-module.md#makefile의-핵심)과 [증분 빌드 설명](../hello_module/kernel-module.md#2-산출물과-증분-빌드)을 참고한다.

## 4. 헷갈리기 쉬운 개념과 질문

<a id="device-registration"></a>

### 장치 등록·/proc/devices·/dev의 차이

```text
사용자 앱의 경로 검색: /dev/char_drv
  → 문자 장치 노드의 (major, minor)
  → 커널에 등록된 file_operations
  → open/read/write/ioctl/release 콜백

/proc/devices: 커널 등록의 major와 이름을 보여주는 정보 창
```

`/proc/devices`는 procfs의 가상 정보 파일이다. 읽으면 커널 내부 등록 목록을 텍스트로 보여준다. 드라이버가 이 파일에 문자열을 쓰거나 `/proc/char_drv`를 생성한 것이 아니다. 대상 커널의 fs/proc/devices.c는 chrdev_show를 호출하고 fs/char_dev.c는 등록 목록을 순회해 표시한다. `/proc` 전체가 장치 목록인 것도 아니며 프로세스 등 여러 커널 정보를 제공한다.

`/dev/char_drv`는 앱이 open할 문자 장치 특수 파일이다. mknod는 파일시스템에 종류·major·minor·권한을 가진 노드를 만든다. 노드 안에 센서 데이터나 드라이버 코드가 저장돼 있는 것은 아니다. 커널은 노드의 번호로 장치 동작을 찾는다. 등록 이름과 노드 이름을 같게 정한 것은 사람이 알아보기 쉽게 하려는 선택이다.

앱은 먼저 경로로 노드를 찾아야 하고, 찾은 다음에 번호로 드라이버에 연결된다. 따라서 다른 이름의 노드에 같은 번호가 있어도 앱이 지정한 경로 자체가 없으면 열 수 없다. 또한 `/proc/devices`의 한 줄만으로 노드 존재·minor별 동작·하드웨어 연결·open 성공을 모두 확인할 수는 없다.

| 작업 | 커널 등록·/proc/devices | /dev 노드 |
| --- | --- | --- |
| insmod | 등록되고 번호·이름 표시 | 이 구현에서는 자동 생성 안 됨 |
| mknod | 기존 등록 유지 | 지정한 종류·번호로 생성 |
| rmmod | 등록 해제·표시 사라짐 | 그대로 남음 |
| rm | 기존 등록 상태 유지 | 노드 삭제 |

새 .ko를 전송해도 로드된 코드는 바뀌지 않는다. 앱을 종료한 뒤 rmmod → insmod로 교체한다. 새 major와 기존 노드의 문자 장치 종류·major·minor가 같으면 노드는 재사용할 수 있다. 노드를 매번 지울 필요는 없지만, 번호가 달라졌다면 다시 만들어야 한다. 오래된 번호가 다른 장치에 재할당될 수 있으므로 현재 등록을 대조한다. 장치 모델·devtmpfs·udev를 이용한 자동 생성은 이 실습에 포함하지 않는다.

### fd·inode·struct file과 모듈 수명

| 구분 | 의미 |
| --- | --- |
| 앱의 fd | 프로세스가 사용하는 정수 식별자 |
| `struct inode *` | 파일 자체의 커널 정보 |
| `struct file *` | 열린 파일의 상태, 연산 표와 현재 위치 등 |
| 모듈 로드/해제 | 코드와 장치 등록을 준비·정리 |
| 파일 open/close | 이미 등록된 장치를 사용하는 열린 파일 참조 생성·정리 |

같은 노드를 별도로 열면 같은 inode에 별도의 struct file을 사용할 수 있고, dup·fork로 여러 fd가 하나의 struct file을 공유할 수도 있다. 그래서 파일 위치·release 시점을 앱의 fd 개수와 단순히 일치시킬 수는 없다. owner의 모듈 참조도 mutex에 의한 데이터 보호와는 다른 역할이다.

<a id="mutex"></a>

### mutex가 필요한 이유와 보호 범위

saved_data와 saved_length는 모든 앱이 함께 사용한다. 잠금이 없으면 read가 길이를 확인한 뒤 다른 write가 내용을 바꿔 서로 다른 시점의 상태를 읽을 수 있다. write끼리의 복사·길이 변경도 겹칠 수 있으므로 데이터와 길이를 같은 잠금으로 보호한다. static 선언 자체는 동시 접근을 막지 않는다.

write는 사용자 복사를 임시 배열에 완료한 후 공유 상태를 갱신한다. read는 길이 확인부터 사용자 복사까지 잠금을 유지해 그 사이 write가 내용을 바꾸지 못하게 한다. copy_to_user는 잠들 수 있으므로 이 경로는 sleep 가능한 mutex로 보호하고 spinlock으로 감싸지 않는다.

mutex는 한 콜백 내부의 일관성을 지킨다. 앱 A의 write와 다음 read 사이에 앱 B가 write하는 것까지 막지는 않으며, 여러 번에 나눠 읽는 동안 내용이 스냅샷으로 고정되지도 않는다. 저장 데이터는 모듈 전체에서 공유하지만 파일 위치는 각 열린 파일에 속한다. 마지막 write가 전체 메시지를 대체하며 새 모듈을 로드하면 버퍼는 초기 상태로 돌아간다.

<a id="goto-out"></a>

### goto out을 사용하는 이유

out은 예약어가 아닌 C 레이블이고 goto out은 같은 함수의 그 위치로 이동한다. EOF·복사 실패·정상 반환 모두 mutex를 풀어야 하므로 char_read는 공통 정리 지점에서 unlock과 return을 수행한다. 잠금 획득 뒤 바로 return해 잠금을 남기는 실수를 줄이는 구조다. 정상 경로도 아래로 진행하다 out에 도달한다.

커널에서도 자원 정리를 한곳으로 모으는 goto는 사용한다. 대상 소스 Documentation/process/coding-style.rst의 “Centralized exiting of functions”에서도 설명한다. 정리할 자원이 없으면 직접 return해도 되고, 모든 함수에 goto가 필요한 것은 아니다. out_unlock처럼 정리 의도가 드러나는 이름도 가능하지만 현재 코드는 out을 사용한다.

### grep 옵션과 기능별 로그

`grep -w char_drv`는 char_drv가 완전한 단어로 나타나는 줄을 찾는다. 밑줄도 단어 문자라 char_drv_test 같은 이름은 제외된다. 행 전체가 같아야 한다는 뜻은 아니다. `grep -F`는 `[WRITE]` 같은 대괄호를 정규식이 아닌 문자 그대로 검색한다. 실시간 파이프에서 `--line-buffered`는 grep이 줄별로 출력을 넘기게 한다.

현재 앱은 기능별 제목과 빈 줄, 커널은 `[MODULE]`, `[OPEN]`, `[WRITE]`, `[READ]`, `[IOCTL]`, `[CLOSE]` 태그로 구분한다. 오류에도 앱 기능 태그를 붙인다. 색상 코드는 사용하지 않는다. 커널 EOF 경로에는 별도의 데이터 전송 로그를 추가하지 않았다. 실제 조회 명령은 아래 재현 절차에 모았다.

## 5. VEDA 코드와의 차이

비교 대상은 참고 저장소 커밋 `c2f2ace95af528c6f806c8fc273f02bdb10120c6`의 [sk.c](../../../VEDA/리눅스커널/수업코드/skeleton/sk.c), [sk_app.c](../../../VEDA/리눅스커널/수업코드/skeleton/sk_app.c), [sk_ioctl.h](../../../VEDA/리눅스커널/수업코드/skeleton/sk_ioctl.h)다. 이 세 파일을 읽고 비교했다. 수업과 동일하게 register_chrdev·file_operations·사용자 앱 흐름을 사용한다.

| 확인한 수업 코드 | 현재 코드와 변경 이유 |
| --- | --- |
| 등록 결과를 result에 저장하고 init은 항상 0 반환 | 음수 오류 검사 후 전달. 실패한 등록을 성공으로 숨기지 않음 |
| 연산 표에 .owner가 없고 콜백·표가 외부 참조 가능한 선언 | THIS_MODULE로 모듈 참조 유지, static·const로 범위와 변경 의도 명시 |
| write의 지역 배열 data[11]에 count만큼 복사, 결과·종료 문자 검사 없음 | 길이와 복사 결과 검사, NUL 공간 확보, 임시 복사 성공 뒤 저장 상태 갱신 |
| read는 지역 배열의 고정 문자열을 count만큼 복사하고 같은 길이를 반환 | 저장 메시지를 요청 크기·남은 길이만큼 반환하고 위치 갱신·EOF 처리 |
| write와 read가 같은 저장 버퍼를 공유하지 않음 | 데이터 왕복을 위해 공유 배열·길이 추가. 이 변경 때문에 mutex도 추가 |
| `_IO` 기반 SPEED_UP/DOWN, switch의 미지원 명령도 0 반환 | 같은 명령 구조로 CLEAR 제공, 미지원 명령은 -ENOTTY |
| 공통 헤더가 _IO 정의 헤더를 직접 포함하지 않음 | 공통 헤더에서 linux/ioctl.h를 포함해 의존성 명시 |
| 앱은 메뉴·scanf로 ioctl 선택, close 반환값 검사 없음 | echo → CLEAR → EOF를 순서대로 확인하고 close 오류도 검사 |
| 앱 open 실패 시 exit(-1) | main에서 1 반환. 작은 앱의 실패 흐름을 직접 표현 |
| printk와 SK 이름 | pr_info/pr_err와 기능 태그·char_drv 이름으로 로그 구분 |

원래 register_chrdev 방식과 콜백 연결 자체는 여전히 유효하다. 이번 변경은 현재 학습 범위의 데이터 왕복·오류 전달·공유 상태 보호를 위한 것이며, 드라이버라면 무조건 수업에 없는 mutex나 echo 버퍼가 필요하다는 뜻은 아니다.

## 6. 빌드·배포·실행·정리와 검증

### 선행 조건

- WSL: Ubuntu 22.04.5, ARM64 GCC 11.4.0·Make·file·modinfo(kmod)·ssh·scp. 저장소는 `~/bspworkspace/bsp-study`이며 SSH 별칭 pi가 설정돼 있어야 한다.
- 커널: 소스 `external/linux`, Raspberry Pi 커밋 `b3495ea401d709af0701a85babcba5e3d788a602`. `bcm2711_defconfig`, `CONFIG_LOCALVERSION="-v8-bsp-study01"`, `CONFIG_LOCALVERSION_AUTO=n`. 전체 빌드 출력 `out/kernel-rpi4`의 .config·생성 헤더·Module.symvers가 필요하다. 각 Kbuild 호출의 `LOCALVERSION=`을 유지한다.
- Pi: aarch64, 실행 커널 `6.18.59-v8-bsp-study01`, sudo 사용 가능. `.ko`는 `~/bsp/modules/`, 앱은 `~/bsp/apps/`로 보낸다. libc·로더는 앱 실행에 사용되며 [크로스 컴파일 실습](../hello/cross-compile.md)에 배경을 설명했다.

커널 자체의 준비·빌드는 [커널 구축 문서](../../docs/kernel-build.md)를 따른다. 버전 문자열뿐 아니라 같은 소스·설정·심볼 정보로 만든 모듈을 사용한다. 아래는 현재 코드용 재현 절차이며 예상 출력과 과거 결과는 뒤에서 구분한다.

### 1) WSL에서 빌드와 산출물 확인

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 위 커널 빌드 환경 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make
echo $?
file char_drv.ko char_app
modinfo -F vermagic ./char_drv.ko
```

make 종료 상태 0을 확인한다. 둘 다 ARM aarch64이며 .ko는 relocatable, 앱은 사용자 실행 파일이어야 한다. 모듈 vermagic의 버전 부분은 Pi 커널과 같아야 한다. 앱은 vermagic 검사 대상이 아니다. 빌드가 실패하면 다음 전송으로 넘어가지 않는다.

### 2) 디렉터리 준비와 전송

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 빌드 확인 후, SSH 별칭 pi 사용 가능**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
ssh pi 'mkdir -p ~/bsp/modules ~/bsp/apps'
make deploy
ssh pi
```

deploy는 현재 파일 전송만 수행한다. .ko를 덮어써도 이미 로드된 모듈 코드는 교체되지 않는다.

### 3) Pi에서 모듈 교체 또는 최초 로드

기존 앱이 종료돼 열린 파일 참조가 없는 상태에서 작업한다. 먼저 로드 여부를 본다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 전송 완료 후**

```bash
cd ~/bsp/modules
uname -r
modinfo -F vermagic ./char_drv.ko
lsmod | grep '^char_drv '
```

대상 버전 일치를 확인한 뒤, 기존 char_drv가 보일 때만 다음 해제를 수행한다. 최초 로드라면 해제 명령은 생략한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 기존 모듈이 로드돼 있고 사용 중인 앱이 없을 때만**

```bash
cd ~/bsp/modules
sudo rmmod char_drv
```

**Pi · 작업 디렉터리: `~/bsp/modules` · 기존 모듈 해제 또는 최초 미로드 상태**

```bash
cd ~/bsp/modules
sudo insmod ./char_drv.ko
lsmod | grep '^char_drv '
grep -w char_drv /proc/devices
ls -l /dev/char_drv
```

노드가 없다는 ls 결과는 이 구현에서 준비해야 할 상태다. 등록 줄의 major와 기존 노드의 종류·major·minor를 대조한다.

### 4) 현재 번호로 노드 준비

문자 노드이며 현재 major·minor 0과 일치하면 이 단계의 생성·삭제는 생략한다. 번호가 다른 기존 실습 노드라면 먼저 삭제한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · /dev/char_drv가 이번 실습 노드이며 번호가 다름을 확인한 경우만**

```bash
cd ~/bsp/modules
sudo rm /dev/char_drv
```

노드가 없을 때 생성한다. 새 터미널에서도 현재 major를 다시 읽도록 같은 블록에서 변수를 정의한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · char_drv 등록 확인·노드 부재 상태**

```bash
cd ~/bsp/modules
char_major=$(awk '$2 == "char_drv" {print $1}' /proc/devices)
printf 'current major: %s\n' "$char_major"
```

출력이 등록 때 확인한 하나의 major인지 확인한 후 같은 터미널에서 생성한다. 비어 있거나 예상하지 못한 출력이면 등록 상태부터 확인한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 바로 위에서 char_major를 정의·확인한 같은 터미널**

```bash
cd ~/bsp/modules
sudo mknod /dev/char_drv c "$char_major" 0
ls -l /dev/char_drv
```

`c`는 문자 장치, 마지막 0은 minor다. 과거 할당값 236을 현재 값으로 가정하지 않는다. 노드 소유자·권한도 표시되며, 현재 앱은 O_RDWR로 열기 때문에 아래 실행에서 sudo를 사용한다.

### 5) 앱과 커널 로그 확인

**Pi · 작업 디렉터리: `~/bsp/apps` · 새 모듈·노드 번호 확인 완료, 다른 앱이 쓰지 않는 상태**

```bash
cd ~/bsp/apps
sudo ./char_app
echo $?
sudo dmesg | grep 'char_drv:'
```

현재 코드의 정상 경로 **예상 앱 출력**이다. fd는 실제 할당값에 따라 달라질 수 있다.

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

종료 상태 0과 같은 실행 시점의 다음 **예상 커널 로그**를 대조한다. major는 예시이며 시간 접두사는 생략했다.

```text
char_drv: [MODULE] loaded, major=236
char_drv: [OPEN] opened
char_drv: [WRITE] received 14 bytes: Hello, driver!
char_drv: [READ] sent 14 bytes
char_drv: [IOCTL] buffer cleared
char_drv: [CLOSE] released
```

dmesg는 과거 이력도 보여주므로 새 시간대의 로그를 확인한다. 필요하면 기능 하나만 검색하거나 다른 터미널에서 실시간으로 본다.

**Pi · 작업 디렉터리: `~/bsp/apps` · 해당 모듈의 로그 조회**

```bash
cd ~/bsp/apps
sudo dmesg | grep -F 'char_drv: [WRITE]'
sudo dmesg -w | grep --line-buffered 'char_drv:'
```

두 번째 명령을 켠 동안 다른 Pi 터미널에서 앱을 실행하고 Ctrl+C로 로그 조회를 끝낸다.

### 6) 모듈과 노드 정리

**Pi · 작업 디렉터리: `~/bsp/modules` · 앱이 종료되고 열린 파일 참조가 없는 상태**

```bash
cd ~/bsp/modules
sudo rmmod char_drv
lsmod | grep '^char_drv '
grep -w char_drv /proc/devices
sudo dmesg | grep 'char_drv:'
ls -l /dev/char_drv
```

모듈·등록 항목은 사라지고 `[MODULE] unloaded`가 추가돼야 한다. 검색 결과가 없을 때 grep 종료 상태 1은 항목을 찾지 못했다는 뜻이다. 노드는 남아 있으므로 확인한 실습 노드를 별도로 삭제한다.

**Pi · 작업 디렉터리: `~/bsp/modules` · 모듈 해제 후 남은 실습 노드 확인 상태**

```bash
cd ~/bsp/modules
sudo rm /dev/char_drv
ls -l /dev/char_drv
```

마지막 파일 부재는 의도한 정리 결과다. 로컬 산출물을 정리하려면 다음을 사용한다.

**WSL · 작업 디렉터리: 문자 디바이스 예제 · 기존 KDIR 빌드 환경 유지 상태**

```bash
cd ~/bspworkspace/bsp-study/examples/char_device
make clean
```

이 clean은 외부 모듈 산출물과 앱을 지우며 커널 전체 빌드 출력은 지우지 않는다.

### 검증 현황

아래는 2026-11-04 대화에서 사용자가 제공한 출력에 근거한 기록이다. 현재 코드의 예상 출력과 구분한다.

| 당시 단계 | 확인된 근거·결과 |
| --- | --- |
| 최초 등록·셸 open/close | Pi 커널·vermagic 일치, lsmod 등록, major 236. minor 0 노드로 셸 open/release 로그 확인. rmmod 뒤 등록은 사라지고 노드는 남았으며 별도 rm 후 부재 확인 |
| C 앱 open/close | fd=3, closed, 종료 상태 0과 3697초대 open/release 로그 |
| write 단독 | 앱 wrote 14, 커널 received 14 bytes와 Hello, driver! 일치 |
| 고정 응답 read | 앱 read 19 bytes: Hello from driver!, EOF. 당시 read는 write 데이터와 독립적인 응답 |
| 공유 버퍼 echo | 앱 write 14·read 14·EOF·echo matched 14로 길이·내용 일치 |
| ioctl CLEAR 추가 | echo matched 뒤 buffer cleared·read after clear: EOF·closed 출력 |

과거 write 단계의 대표 커널 로그:

```text
[ 4745.027807] char_drv: opened
[ 4745.028071] char_drv: received 14 bytes: Hello, driver!
[ 4745.028115] char_drv: released
```

기능별 출력 정리 **이전** ioctl 단계의 대표 앱 출력:

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

이전 지역 버퍼·고정 응답 코드를 본문에 남기지 않은 이유는 현재 구현과 상태가 다르기 때문이다. 학습상 중요한 변화는 지역 배열의 수명 → 모듈 공유 저장, 고정 문자열 read → 저장 데이터 read, 독립 read/write → 왕복 비교, 단순 제어 로그 → 실제 버퍼 초기화다. mutex는 공유 저장 도입에 따라 필요해졌다.

확인 범위와 남은 항목은 다음과 같다.

- 에이전트는 대상 커널의 fs.h·char_dev.c·proc/devices.c·코딩 지침과 VEDA 세 파일을 읽기 대조했다. 이번 문서 검증은 코드 발췌·링크·명령 구문의 정적 검토로 수행했다.
- WSL 빌드는 사용자가 확인했다고 보고했으나 원시 make/file 출력과 make deploy 자체 실행 출력은 별도로 받지 않았다. Pi에서 해당 기능이 동작한 출력과 이 사실은 구분한다.
- 기능별 제목·태그를 적용한 최신 출력 형식은 실행 결과가 전달되지 않았다. 위 기능 동작 근거는 출력 정리 전 결과이며 최신 출력이 검증됐다고 표현하지 않는다.
- C 앱의 최초 open/close 종료 상태 0은 확인했다. 이후 echo·ioctl 실행의 종료 상태 출력과 해당 커널 로그는 별도 미수신이다.
- 길이 경계 시험은 사용자 선택으로 생략했다. 복사 실패·미지원 ioctl·사용 중 모듈 해제 거부·동시성 시험도 수행 근거가 없으며 다음 단계 진행 조건으로 요구하지 않는다.
- 단순 명령·이름 오타는 트러블슈팅 사례로 기록하지 않는다. 현재 별도 장애 사례를 추가할 내용은 없다.
