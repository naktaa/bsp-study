# Device Tree와 platform 드라이버: DT 노드 → 매칭 → probe/remove → sysfs

## 1. 한눈에 보기
- **목적:** DT 노드가 커널 장치가 되고, compatible로 드라이버와 짝지어져 probe가 불리기까지의 경로를 센서 없이 확인한다. 이후 I2C 센서([06](06-mpu6050.md))·카메라·마이크가 모두 같은 "DT 노드 → 매칭 → probe" 구조를 쓴다.
- **기능:** runtime overlay가 루트에 `platform-demo` 노드를 추가하면, 미리 등록해 둔 드라이버가 매칭돼 probe에서 DT의 문자열·정수를 읽고 읽기 전용 sysfs 파일 `label`·`study_value`로 공개한다. overlay를 제거하면 remove가 불리고 devm 자원이 정리된다.
- **직접 작성 / 가져다 쓴 것:** platform 드라이버·overlay·Makefile은 직접 작성했다. platform bus·OF 매칭·`of_property_read_*`·devm·sysfs·`dtoverlay`는 커널과 Pi OS 것을 쓴다.
- **결과(실측):** overlay 적용 후 `cat label` → `platform study`, `cat study_value` → `42`, 제거 후 속성 파일이 사라지고 `[REMOVE] detached`. 상세는 [6장](#6-트러블슈팅과-한계).
- **범위 밖:** 레지스터·GPIO·IRQ·I2C 전송, `/dev` 노드, store 콜백(값 변경), 부팅 `config.txt`에 overlay 고정.
- **관련 파일:** [platform_drv.c](../../examples/platform_device/platform_drv.c), [platform-overlay.dts](../../examples/platform_device/platform-overlay.dts), [Makefile](../../examples/platform_device/Makefile)

## 2. 구조

**장치 수명 흐름:** `insmod`(드라이버 등록, 대기) → `dtoverlay` 노드 추가 → platform 장치 생성 → compatible 매칭 → `platform_probe` → sysfs 공개 → `cat` → `dtoverlay -r` → `platform_remove` → devm 정리 → `rmmod`

```text
 +---------------+  dtoverlay  +------------------+  of_match  +------------------+  sysfs  +--------------+
 | overlay.dtbo  |------------>| /platform-demo   |----------->| platform_probe   |-------->| label        |
 | fragment "/"  |             | platform_device  |            | of_property_read |         | study_value  |
 +---------------+             +------------------+            +------------------+         +--------------+
```

드라이버를 먼저 올려도 장치가 없으면 probe는 불리지 않는다. 장치(노드)와 드라이버가 둘 다 있고 compatible이 맞을 때 커널이 probe를 부른다.

| 계층 | 구성 요소 | 하는 일 |
| --- | --- | --- |
| DT | `platform-overlay.dts` | 루트 아래 `compatible = "nakta,platform-demo"`, `label`, `nakta,study-value = <42>` 노드 추가 |
| 커널 OF | `of_platform_notify`(drivers/of/platform.c) | 동적으로 추가된 루트 노드에 platform 장치 생성·제거 |
| 커널 버스 | platform bus | 장치와 드라이버의 `of_match_table` 비교 → probe/remove 호출 |
| 직접 작성 드라이버 | `platform_drv.ko` | DT 속성 읽기, 장치별 상태 보관, sysfs 속성 공개 |
| 사용자 공간 | `cat`, `/proc/device-tree` | 드라이버가 낸 값과 DT 원시 값 확인 |

## 3. 알아야 할 개념

### 왜 compatible로 매칭하는가
노드 이름·드라이버 이름·파일 이름은 서로 달라도 된다. 커널은 노드의 `compatible`과 드라이버의 `of_device_id` 표만 비교한다([platform_match L86](../../examples/platform_device/platform_drv.c#L86)). `MODULE_DEVICE_TABLE(of, ...)`은 같은 문자열을 모듈 별칭(`of:N*T*Cnakta,platform-demo`)으로 남겨 udev가 모듈을 찾을 수 있게 한다. 다른 드라이버와 문자열이 겹치면 엉뚱한 드라이버가 잡으므로 vendor 접두사(`nakta,`)를 붙였다.

### 왜 overlay만으로 platform 장치가 생기는가
`target-path = "/"`로 루트 바로 아래에 노드를 넣었기 때문이다. 커널(`CONFIG_OF_DYNAMIC=y`·`CONFIG_OF_OVERLAY=y`)의 `of_platform_notify`가 루트나 platform bus 아래에 동적으로 추가된 노드에 platform 장치를 만들고, 제거될 때 지운다. 그래서 센서 없이도 probe/remove를 관측할 수 있다. 레지스터가 없는 논리 장치라 `reg`와 `@주소`는 넣지 않았다([overlay L11](../../examples/platform_device/platform-overlay.dts#L11)).

### 왜 DT 문자열을 복사해 두는가
`of_property_read_string`은 DT 메모리 안의 문자열 주소를 돌려준다. 드라이버가 오래 쓸 값은 `devm_kstrdup`으로 장치 소유 메모리에 복사해 둔다([L64](../../examples/platform_device/platform_drv.c#L64)). 정수는 `of_property_read_u32`가 big-endian 셀(`00 00 00 2a`)을 CPU 값 42로 바꿔 준다.

### 왜 remove에서 아무것도 해제하지 않는가
상태 메모리·복사 문자열·sysfs 그룹을 모두 `devm_*`으로 얻었기 때문이다. 커널이 바인딩이 풀릴 때 등록의 역순(sysfs 그룹 → 문자열 → 상태)으로 정리하고, probe가 실패해도 그때까지 얻은 것을 정리한다. 그래서 remove는 로그만 남기고([L81](../../examples/platform_device/platform_drv.c#L81)) `kfree`·`sysfs_remove_group`을 다시 부르지 않는다.

### 왜 sysfs 그룹을 probe 마지막에 공개하는가
속성 파일이 생기는 순간 사용자가 `cat`할 수 있다. 그 전에 값과 `platform_set_drvdata`가 준비돼 있어야 show 콜백이 빈 상태를 읽지 않는다. 그래서 값 읽기 → 복사 → drvdata 연결 → `devm_device_add_group` 순서로 둔다([L68](../../examples/platform_device/platform_drv.c#L68)).

## 4. 구현 흐름

### 4-1. overlay: 루트에 노드 하나 추가

```dts
	fragment@0 {
		target-path = "/";
		__overlay__ {
			platform-demo {
				compatible = "nakta,platform-demo";
				status = "okay";
				label = "platform study";
				nakta,study-value = <42>;
			};
		};
	};
```

- 최상위 `compatible = "brcm,bcm2711"`은 대상 플랫폼 계열이고, 새 노드의 `nakta,platform-demo`와 역할이 다르다.
- `fragment@0`은 변경 묶음이고, `__overlay__` 안의 노드가 루트 아래에 **추가**된다. 루트 교체가 아니다.
- `<42>`는 32비트 셀 하나라 `/proc/device-tree`에서 `00 00 00 2a`로 보인다. 값을 바꾸려면 DTS를 다시 빌드하고 overlay를 제거·재적용한다.

### 4-2. 드라이버 등록: 장치를 기다린다
`module_platform_driver()`는 `platform_driver_register`·`unregister`를 부르는 init·exit를 만들어 주는 매크로다([L100](../../examples/platform_device/platform_drv.c#L100)). `insmod` 직후에는 `/sys/bus/platform/drivers/platform-demo`만 생기고, 장치가 없으므로 PROBE 로그는 없다.

### 4-3. probe: 읽기 → 복사 → 연결 → 공개

| 순서 | 하는 일 | 코드 |
| --- | --- | --- |
| 1 | `devm_kzalloc`으로 장치별 상태 할당, 실패하면 `-ENOMEM` | [L48](../../examples/platform_device/platform_drv.c#L48) |
| 2 | 필수 속성 `label`·`nakta,study-value` 읽기, 실패하면 `dev_err_probe`로 원인을 남기고 같은 오류 반환 | [L52](../../examples/platform_device/platform_drv.c#L52) |
| 3 | `label`을 `devm_kstrdup`으로 복사 | [L64](../../examples/platform_device/platform_drv.c#L64) |
| 4 | `platform_set_drvdata(pdev, state)`: show 콜백이 `dev_get_drvdata`로 같은 상태를 찾게 한다 | [L68](../../examples/platform_device/platform_drv.c#L68) |
| 5 | `devm_device_add_group`으로 `label`·`study_value` 공개, `[PROBE] label=..., value=42` 로그 | [L71](../../examples/platform_device/platform_drv.c#L71) |

필수 속성이 없어도 DT 노드와 platform 장치는 생긴다. 그러나 probe가 실패하면 `driver` 링크와 sysfs 속성은 생기지 않는다. 그래서 확인할 때 노드·`driver` 링크·속성 파일·로그를 따로 본다.

### 4-4. sysfs show: 보관한 값을 텍스트로
`DEVICE_ATTR_RO(label)`은 읽기 전용(0444) 속성과 `label_show`를 잇는다. show는 DT를 다시 읽지 않고 probe에서 보관한 값을 `sysfs_emit`으로 출력한다([label_show L14](../../examples/platform_device/platform_drv.c#L14)). 그룹에 이름이 없어서 파일이 장치 디렉터리 바로 아래 생긴다.

### 4-5. remove와 모듈 해제는 따로다
overlay를 제거하면 장치가 사라지고 바인딩이 풀려 remove가 불린다. 드라이버 디렉터리는 남는다. 드라이버를 지우는 것은 `rmmod`다. 장치 수명과 모듈 수명이 별개라는 것이 이 실습의 핵심 관측이다.

### 4-6. 기존 I2C 컨트롤러와 비교
같은 구조를 실제 장치에서 읽어 확인했다. `/soc/i2c@7e804000` 노드 → platform 장치 `fe804000.i2c` → 드라이버 `i2c-bcm2835` → adapter `i2c-1`.

| 기존 `i2c-bcm2835` (external/linux/drivers/i2c/busses/i2c-bcm2835.c) | 이번 실습 |
| --- | --- |
| SoC 컨트롤러 compatible `brcm,bcm2711-i2c` | 학습용 `nakta,platform-demo`, 자원 없음 |
| probe가 레지스터 매핑·클럭·IRQ 준비 | DT 속성 읽기·상태 보관·sysfs 공개 |
| `i2c_add_adapter`로 버스 등록 | 버스를 등록하지 않음 |
| `platform_driver` + OF 표 | 같은 등록·매칭 구조 |

DT 경로는 `@7e804000`인데 장치 이름이 `fe804000`인 이유는 `soc`의 `ranges`가 버스 주소 0x7e000000을 CPU 물리 주소 0xfe000000으로 옮기기 때문이다.

## 5. 재현

<details><summary>빌드 → 배포 → 드라이버 등록 → overlay 적용 → 확인 → 정리 (펼치기)</summary>

선행 조건: [07 커널 빌드](07-kernel-build.md)의 `out/kernel-rpi4`(호스트 `scripts/dtc/dtc` 포함), SSH 별칭 `pi`, Pi 커널 `6.18.59-v8-bsp-study01`. 부팅 파일과 `config.txt`는 건드리지 않는다.

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/platform_device — 빌드·전송
make
modinfo -F vermagic ./platform_drv.ko      # 6.18.59-v8-bsp-study01 ...
modinfo -F alias ./platform_drv.ko         # of:N*T*Cnakta,platform-demo...
ssh pi 'mkdir -p ~/bsp/modules ~/bsp/overlays' && make deploy
```

```bash
# [Pi] 드라이버 먼저 등록 — 장치가 없어 PROBE 로그 없음
cd ~/bsp/modules && sudo insmod ./platform_drv.ko
ls -ld /sys/bus/platform/drivers/platform-demo
sudo dmesg | grep -F 'platform-demo'          # 출력 없음
```

```bash
# [Pi] ~/bsp/overlays — overlay 적용 → 매칭·probe → sysfs
cd ~/bsp/overlays && sudo dtoverlay -d "$PWD" platform-overlay
od -An -tx1 /proc/device-tree/platform-demo/nakta,study-value   # 00 00 00 2a
readlink -e /sys/bus/platform/devices/platform-demo/driver      # .../drivers/platform-demo
cat /sys/bus/platform/devices/platform-demo/label               # platform study
cat /sys/bus/platform/devices/platform-demo/study_value         # 42
sudo dmesg | grep -F 'platform-demo' | tail -1                  # [PROBE] label=platform study, value=42
```

```bash
# [Pi] 정리 — 이 overlay만 이름으로 제거(-R이나 이름 없는 -r 금지) → remove → rmmod
sudo dtoverlay -r platform-overlay && sudo dtoverlay -l         # No overlays loaded
ls /sys/bus/platform/devices/platform-demo                      # 없음, 드라이버 디렉터리는 남음
sudo rmmod platform_drv && ls -ld /sys/bus/platform/drivers/platform-demo   # 없음
```

</details>

## 6. 트러블슈팅과 한계

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| sysfs 속성 `ls -l` 크기가 4096 | sysfs가 보고하는 고정 크기 | 실제 문자열 길이가 아니다. `cat`으로 값을 본다 |

**현재 버전 검증 (2026-11-05, sysfs 버전)**

| 항목 | 결과 |
| --- | --- |
| 드라이버 등록 | vermagic 일치, OF 별칭 `of:N*T*Cnakta,platform-demo`, `lsmod`·드라이버 디렉터리, 장치 없을 때 PROBE 로그 없음 |
| 적용·probe | `[PROBE] label=platform study, value=42`, `driver` 링크 `platform-demo`, DT 바이트 `00 00 00 2a` |
| sysfs | `label` → `platform study`, `study_value` → `42`, 두 파일 모두 `-r--r--r--` |
| 제거·해제 | 속성 파일 부재와 `[REMOVE] detached`, 드라이버 디렉터리 유지, `rmmod` 오류 없음 |
| 기존 컨트롤러 | `soc/i2c@7e804000` → `fe804000.i2c` → `i2c-bcm2835` → `i2c-1` |

- 미검증: 필수 속성 누락·형식 오류일 때의 probe 실패 경로, sysfs 버전 rmmod 뒤 `lsmod`·드라이버 디렉터리 재확인, devm 메모리 해제 추적(소스 읽기로 설명한 동작), `make` 종료 상태·`deploy` 원시 출력.
- 한계: 값은 probe 때 한 번 읽어 보관하므로 런타임 변경이 없다. runtime overlay라 재부팅하면 사라진다.

**더 보기**
- `/proc/device-tree`와 `/sys/firmware/devicetree/base`는 무엇인가 → 커널이 쓰는 실행 중 트리. 폴더는 노드, 파일은 속성이고 문자열 목록은 NUL로 구분된다(`tr '\0' '\n'`)
- `reg = <0x7e804000 0x1000>`은 어떻게 읽는가 → 부모의 `#address-cells`·`#size-cells`만큼 셀을 나눠 주소·크기로 읽는다. 컨트롤러 레지스터 주소와 I2C 센서 주소(0x68)는 다르다
- `dtoverlay -l` 목록이 `config.txt`의 overlay와 같은가 → 아니다. runtime으로 올린 것만 보인다
- 수업 자료는? → VEDA `임베디드리눅스/강의자료/임베디드리눅스1.pdf` 23~27쪽(DT 노드·속성·compatible·reg), 29쪽(Pi overlay). 이 실습은 VEDA GPIO 예제를 옮긴 것이 아니다
- 모듈 빌드·`obj-m`·DTBO 규칙은? → [02 문서](02-kernel-module.md)와 [Makefile](../../examples/platform_device/Makefile)
