# 문자 디바이스: open → write → read → ioctl로 사용자/커널 경계 넘기

## 1. 한눈에 보기
- **목적:** 사용자 앱의 `open`·`write`·`read`·`ioctl`이 커널의 어느 함수로 이어지고, 데이터가 어떻게 경계를 넘는지 이해한다. 이후 센서 드라이버([06](06-mpu6050.md))의 `/dev` 인터페이스가 이 구조 위에 있다.
- **기능:** 앱이 보낸 짧은 메시지를 커널의 64바이트 버퍼에 저장하고, 다시 읽어 길이·내용이 같은지 비교한 뒤 `ioctl(CHAR_CLEAR)`로 버퍼를 비운다. 하드웨어 대신 메모리 버퍼를 쓴다.
- **직접 작성 / 가져다 쓴 것:** 드라이버([char_drv.c](../../examples/char_device/char_drv.c))·앱·공통 ioctl 헤더는 직접 작성했다. 문자 장치 등록(`register_chrdev`)·VFS·`copy_*_user`는 커널 것을 쓴다.
- **결과(실측):** 앱이 14바이트를 쓰고 14바이트를 읽어 `echo matched: 14 bytes`, CLEAR 뒤 read가 EOF. 상세는 [6장](#6-트러블슈팅과-한계).
- **범위 밖:** `/dev` 노드 자동 생성(udev), 데이터 이어붙이기·큐·앱별 저장 공간, ioctl 데이터 인자, 32비트 앱 호환.
- **관련 파일:** [char_drv.c](../../examples/char_device/char_drv.c), [char_app.c](../../examples/char_device/char_app.c), [char_ioctl.h](../../examples/char_device/char_ioctl.h), [Makefile](../../examples/char_device/Makefile)

## 2. 구조

**한 번의 실행 흐름:** `insmod` → `register_chrdev` → `mknod /dev/char_drv` → 앱 `open` → `write` → `read`(EOF까지) → 비교 → `ioctl(CHAR_CLEAR)` → `read`(EOF) → `close` → `rmmod`

```text
 +-----------+  open/write/read/ioctl  +-----------+  (major, minor)  +------------+  callbacks  +-------------+
 | char_app  |------------------------>| /dev node |----------------->| char_fops  |------------>| saved_data  |
 | user      |<------------------------| c 236 0   |                  | VFS lookup |             | 64B + mutex |
 +-----------+  bytes / 0(EOF) / errno +-----------+                  +------------+             +-------------+
```

앱은 드라이버 함수를 직접 부르지 않는다. 앱이 경로로 노드를 열면 커널이 노드의 major로 등록된 `file_operations`를 찾아 콜백을 호출한다.

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| 사용자 공간 | `char_app` | open → write → read 반복 → 비교 → CLEAR → read → close, 실패하면 종료 코드 1 |
| 파일시스템 | `/dev/char_drv`(mknod) | 문자 장치 종류·major·minor만 가진 특수 파일 |
| 커널 VFS | `register_chrdev`, `struct file` | major → `file_operations` 연결, 열린 파일마다 위치(`f_pos`) 보관 |
| 직접 작성 드라이버 | `char_drv.ko` | 64바이트 공유 버퍼·길이를 mutex로 보호, 콜백 5개 |

## 3. 알아야 할 개념

### 왜 insmod만으로는 앱이 열 수 없는가
`register_chrdev`는 커널 안에 "major 236 → `char_fops`"를 등록할 뿐이고(`/proc/devices`에 보임), 앱이 여는 `/dev/char_drv` 파일은 만들지 않는다. 그래서 `mknod /dev/char_drv c <major> 0`으로 노드를 따로 만든다. rmmod해도 노드는 남으므로 따로 지운다. 장치 모델·udev로 노드를 자동으로 만드는 방식은 [06의 misc 장치](06-mpu6050.md#왜-misc-장치로-만들었고-다음은-iio인가)에서 쓴다.

### 왜 사용자 포인터를 바로 읽지 않고 copy_from_user로 옮기는가
`buf`는 `__user`가 붙은 사용자 주소라 커널이 바로 역참조하면 안 된다. `copy_from_user`·`copy_to_user`는 주소를 검사하며 복사하고, **복사하지 못한 바이트 수**를 돌려준다. 그래서 0일 때만 성공으로 보고 나머지는 `-EFAULT`로 돌려준다([L44](../../examples/char_device/char_drv.c#L44), [L81](../../examples/char_device/char_drv.c#L81)).

### 왜 write는 임시 배열에 먼저 복사하는가
사용자 복사가 중간에 실패해도 기존 저장 내용을 지키기 위해서다. 함수 지역 배열 `data`에 복사와 NUL 붙이기를 끝낸 뒤에야 mutex를 잡고 `saved_data`와 `saved_length`를 함께 바꾼다([write L33](../../examples/char_device/char_drv.c#L33)). 지역 배열은 함수가 끝나면 사라지므로 read가 쓸 데이터는 파일 범위 배열에 둔다.

### 왜 spinlock이 아니라 mutex인가
`saved_data`·`saved_length`는 모든 열린 파일이 함께 쓴다. read가 길이를 확인한 뒤 다른 write가 내용을 바꾸면 서로 다른 시점의 상태를 읽게 되므로 길이 확인부터 복사까지 한 잠금으로 묶는다. 그런데 `copy_to_user`는 페이지 폴트로 잠들 수 있어 spinlock 안에서 부르면 안 된다. 그래서 잠들 수 있는 mutex를 쓴다([L19](../../examples/char_device/char_drv.c#L19)).

### 왜 read가 0을 돌려주면 EOF인가
read의 반환값은 "이번에 준 바이트 수"다. 드라이버는 열린 파일의 위치 `*f_pos`를 읽은 만큼 올리고, 위치가 저장 길이에 닿으면 0을 돌려준다([L76](../../examples/char_device/char_drv.c#L76)). 앱은 `read() > 0`인 동안 반복하다 0을 만나면 끝으로 본다([char_app.c L49](../../examples/char_device/char_app.c#L49)). 위치는 `struct file`마다 따로 있어서 다른 fd의 읽기 위치에 영향을 주지 않는다.

## 4. 구현 흐름

### 4-1. 등록: 연산 표와 동적 major
`char_fops`는 사용자 연산과 콜백을 잇는 표다. `.owner = THIS_MODULE`은 파일이 열려 있는 동안 모듈 참조를 잡아, 사용 중인 콜백 코드가 rmmod로 사라지지 않게 한다([L117](../../examples/char_device/char_drv.c#L117)).

| 순서 | 하는 일 | 코드 |
| --- | --- | --- |
| 1 | `register_chrdev(0, ...)`: 0은 동적 major 요청. 성공하면 양수 major, 실패하면 음수 오류 | [init L126](../../examples/char_device/char_drv.c#L126) |
| 2 | 음수면 그대로 반환해 로드 실패. 성공이면 init은 0을 반환 | [L129](../../examples/char_device/char_drv.c#L129) |
| 3 | 해제 때 받은 major로 `unregister_chrdev` | [exit L138](../../examples/char_device/char_drv.c#L138) |

`register_chrdev`의 성공 값(major)과 init의 성공 값(0)이 다르다는 점을 헷갈리지 않는다. 모듈 init·exit 매크로와 `MODULE_*`는 [02 문서](02-kernel-module.md)에서 다룬다.

### 4-2. open·release: 로그만 남긴다
`char_open`의 0은 "열기 성공"을 커널에 알리는 값이고, 앱이 받는 fd 번호는 커널이 정한다([L21](../../examples/char_device/char_drv.c#L21)). `char_release`는 열린 파일의 **마지막 참조**가 사라질 때 불린다. dup·fork로 참조를 나누면 close마다 불리지는 않는다.

### 4-3. write: 검사 → 복사 → 잠금 안에서 교체

```c
	if (count >= sizeof(data))
		return -EMSGSIZE;              /* NUL 자리를 남겨 최대 63B */
	if (copy_from_user(data, buf, count))
		return -EFAULT;
	data[count] = '\0';
	mutex_lock(&data_lock);
	memcpy(saved_data, data, count + 1);
	saved_length = count;
	*f_pos = 0;                        /* 같은 fd에서 새 메시지를 처음부터 읽게 */
	mutex_unlock(&data_lock);
```

- 0바이트 요청은 상태를 바꾸지 않고 0을 반환한다([L38](../../examples/char_device/char_drv.c#L38)).
- 긴 요청은 잘라서 성공시키지 않고 `-EMSGSIZE`로 거부한다. 앱에서는 `-1`과 `errno`로 보인다.
- `*f_pos = 0`은 일반 파일 규칙이 아니라 이 장치의 약속이다. 다른 열린 파일의 위치는 건드리지 않는다.

### 4-4. read: 남은 만큼만 주고 위치를 올린다

```c
	mutex_lock(&data_lock);
	if (*f_pos >= (loff_t)saved_length)
		goto out;                      /* result = 0 → EOF */
	if (count > saved_length - *f_pos)
		count = saved_length - *f_pos;
	if (copy_to_user(buf, saved_data + *f_pos, count)) {
		result = -EFAULT;
		goto out;
	}
	*f_pos += count;
	result = count;
out:
	mutex_unlock(&data_lock);
```

EOF·복사 실패·정상 경로가 모두 mutex를 풀어야 하므로 `out:` 한 곳에서 풀고 반환한다([read L64](../../examples/char_device/char_drv.c#L64)). 복사에 실패하면 위치를 올리지 않는다.

### 4-5. ioctl: 데이터 없는 제어 명령
`CHAR_CLEAR`는 공통 헤더에서 `_IO('C', 1)`로 만든다([char_ioctl.h L7](../../examples/char_device/char_ioctl.h#L7)). 앱과 드라이버가 같은 헤더를 써야 번호가 맞는다. read·write가 payload를 옮긴다면 ioctl은 "버퍼를 비워라" 같은 요청을 전한다.

드라이버는 잠금 안에서 버퍼·길이를 0으로 만들고 `filp->f_pos = 0`으로 둔다. 그래서 앱이 바로 다시 read하면 처음 위치에서도 데이터가 없어 EOF가 된다([ioctl L96](../../examples/char_device/char_drv.c#L96)). 모르는 명령은 `-ENOTTY`("이 장치에 맞지 않는 ioctl")로 돌려준다([L113](../../examples/char_device/char_drv.c#L113)).

### 4-6. VEDA 수업 코드와의 차이
비교 대상은 VEDA 저장소 `c2f2ace95a`의 `리눅스커널/수업코드/skeleton/sk.c`·`sk_app.c`·`sk_ioctl.h`다(저장소 밖이라 텍스트로 적음). `register_chrdev`·`file_operations`·앱 흐름은 수업과 같다.

| 수업 코드 | 현재 코드 | 이유 |
| --- | --- | --- |
| 등록 결과를 저장만 하고 init은 항상 0 | 음수면 그대로 반환 | 실패한 등록을 성공으로 숨기지 않는다 |
| `.owner` 없음, 콜백이 외부 공개 | `THIS_MODULE`, `static`·`const` | 사용 중 해제 방지, 범위 명시 |
| write가 `data[11]`에 count만큼 복사, 결과 검사 없음 | 길이·복사 결과 검사, NUL 자리 확보 | 오버플로·부분 복사 방지 |
| read가 고정 문자열 반환, write와 버퍼 공유 없음 | 저장 메시지를 남은 길이만큼, 위치·EOF 처리 | 왕복 비교를 하려고 공유 버퍼를 두었고, 그래서 mutex가 필요해졌다 |
| 미지원 ioctl도 0 반환 | `-ENOTTY` | 잘못된 요청을 성공으로 보이지 않게 |

## 5. 재현

<details><summary>빌드 → 배포 → 등록·노드 → 실행 → 정리 (펼치기)</summary>

선행 조건: [07 커널 빌드](07-kernel-build.md)의 `out/kernel-rpi4`(같은 소스·설정·`Module.symvers`), SSH 별칭 `pi`, Pi 커널 `6.18.59-v8-bsp-study01`.

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/char_device — 빌드·전송
make
file char_drv.ko char_app                 # 둘 다 ARM aarch64
modinfo -F vermagic ./char_drv.ko         # Pi uname -r과 같아야 함
ssh pi 'mkdir -p ~/bsp/modules ~/bsp/apps' && make deploy
```

이미 로드돼 있으면 앱을 끝내고 `sudo rmmod char_drv`부터, 번호가 다른 옛 노드가 있으면 `sudo rm /dev/char_drv`부터 한다.

```bash
# [Pi] ~/bsp/modules — 등록 → 현재 major로 노드 만들기
cd ~/bsp/modules && sudo insmod ./char_drv.ko
grep -w char_drv /proc/devices            # 예: 236 char_drv
char_major=$(awk '$2 == "char_drv" {print $1}' /proc/devices)
sudo mknod /dev/char_drv c "$char_major" 0 && ls -l /dev/char_drv   # crw... 236, 0
```

```bash
# [Pi] ~/bsp/apps — 실행과 커널 로그
cd ~/bsp/apps && sudo ./char_app; echo $?  # echo matched: 14 bytes, read after clear: EOF, 0
sudo dmesg | grep 'char_drv:' | tail -6    # [OPEN] [WRITE] [READ] [IOCTL] [CLOSE]
# 정리 — rmmod 뒤에도 노드는 남으므로 따로 지운다
sudo rmmod char_drv && grep -w char_drv /proc/devices   # 출력 없음
sudo rm /dev/char_drv
```

</details>

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| 모듈을 다시 올린 뒤 앱 open 실패 | 동적 major가 바뀌었는데 옛 번호의 노드가 남음 | `/proc/devices`의 major와 `ls -l /dev/char_drv`를 대조하고 다르면 지우고 다시 mknod |
| 새 `.ko`를 보냈는데 동작이 그대로 | 파일을 덮어써도 로드된 코드는 바뀌지 않음 | 앱을 끝내고 rmmod → insmod |

**현재 버전 검증 (2026-11-04)**

| 항목 | 결과 |
| --- | --- |
| 등록·노드 | vermagic 일치, `lsmod` 등록, major 236, rmmod 뒤 등록은 사라지고 노드는 남음 |
| open·close | fd=3, 종료 상태 0, open·release 커널 로그 |
| 데이터 왕복 | write 14 → read 14 → EOF → `echo matched: 14 bytes` |
| ioctl | `buffer cleared`, `read after clear: EOF` |

- 미검증: 현재 출력 형식(기능별 제목·`[TAG]`)으로 바꾼 뒤의 실행 출력, 63바이트 경계와 `-EMSGSIZE`, 복사 실패(`-EFAULT`)·미지원 ioctl(`-ENOTTY`), 사용 중 rmmod 거부, 여러 앱 동시 write.
- 한계: 버퍼가 모듈 전체에 하나라 마지막 write가 앞 메시지를 덮어쓴다. mutex는 콜백 하나 안의 일관성만 지키고, A의 write와 read 사이에 B가 write하는 것은 막지 않는다.

**더 보기**
- fd·`struct inode`·`struct file`은 어떻게 다른가 → fd는 프로세스의 번호, inode는 파일 자체, `struct file`은 열린 파일 상태(위치 포함). dup·fork는 `struct file`을 공유한다 ([open L21](../../examples/char_device/char_drv.c#L21))
- 커널에서 `goto out`을 왜 쓰는가 → 잠금 해제·자원 정리를 한곳에 모아 빠뜨리지 않기 위해. 커널 `Documentation/process/coding-style.rst`의 "Centralized exiting of functions" ([L90](../../examples/char_device/char_drv.c#L90))
- `register_chrdev`는 minor를 몇 개 잡는가 → 0~255를 등록한다. 이 실습은 minor 0만 쓰고 구분하지 않는다
- `/proc/devices`는 무엇인가 → 커널 등록 목록을 텍스트로 보여 주는 procfs 가상 파일. 드라이버가 쓰는 파일이 아니다
- Makefile의 `obj-m`·`KDIR`·`M=`은? → [02 문서](02-kernel-module.md)
