# Device Tree와 platform 드라이버 — 등록과 장치 매칭

센서 도착 전에도 확인할 수 있는 Device Tree·커널 장치·드라이버 연결을 학습한다. 기존 Pi I2C 컨트롤러의 연결을 읽은 뒤, [platform_drv.c](platform_drv.c)로 실습용 드라이버를 등록한다. 기존 [문자 디바이스](../char_device/char-device.md)의 등록 방식은 유지한다.

목차:

- [1. 구현 범위와 목적](#1-구현-범위와-목적)
- [2. 전체 실행 흐름](#2-전체-실행-흐름)
- [3. 현재 코드 해설](#3-현재-코드-해설)
- [4. 기존 I2C 연결에서 확인한 개념](#4-기존-i2c-연결에서-확인한-개념)
- [5. 수업 자료와 기존 드라이버 비교](#5-수업-자료와-기존-드라이버-비교)
- [6. 빌드·전송·등록 확인과 검증](#6-빌드전송등록-확인과-검증)

## 1. 구현 범위와 목적

현재 파일은 platform 드라이버, [platform-overlay.dts](platform-overlay.dts), 모듈·overlay 빌드·전송 Makefile이다. 하드웨어 레지스터·GPIO·IRQ·I2C 전송은 하지 않는다. probe는 DT 문자열·정수를 장치별 상태로 보관하고 읽기 전용 sysfs 속성 label·study_value를 공개한다. remove는 해제 로그를 남기며 관리 자원은 devm 경로에서 정리된다. 실습용 compatible은 `nakta,platform-demo`, 드라이버 이름은 `platform-demo`, 모듈 파일은 `platform_drv.ko`다. 이 문자열은 독립적인 로컬 학습 장치를 위한 것으로 기존 컨트롤러의 식별자를 대신 사용하지 않는다.

드라이버 자체는 장치를 만들지 않고 overlay 적용이 독립적인 DT 노드를 추가한다. `/dev` 문자 장치 노드는 만들지 않는다. 목표는 “드라이버 등록”과 “장치 생성 후 probe 호출”을 구분하는 것이다. 확인된 결과와 남은 범위는 마지막 검증 현황에 모았다.

## 2. 전체 실행 흐름

현재 코드의 흐름:

```text
WSL make → ARM64 platform_drv.ko와 platform-overlay.dtbo 생성
  → make deploy로 Pi에 두 파일 전송
Pi insmod platform_drv.ko
  → module_platform_driver가 생성한 초기화 경로
  → platform 드라이버 등록
  → 매칭되는 장치가 있으면 커널이 platform_probe(pdev) 호출
     매칭 장치가 없으면 드라이버 등록 상태로 대기
Pi dtoverlay로 실습 overlay 적용
  → /platform-demo 노드 추가 → platform 장치 생성
  → compatible 매칭 → platform_probe(pdev) 호출
  → 장치별 메모리 준비 → label·nakta,study-value 읽기·문자열 복사
  → drvdata 연결 → sysfs 속성 공개 → 성공 로그 또는 음수 오류 반환
Pi cat /sys/bus/platform/devices/platform-demo/label 또는 study_value
  → 커널이 해당 show 콜백 호출 → 저장된 값을 텍스트로 반환
Pi dtoverlay로 해당 overlay 제거
  → 장치 제거·바인딩 해제 → platform_remove(pdev) 호출
  → devm 속성 그룹 제거·문자열·장치별 상태 메모리 해제
Pi rmmod platform_drv
  → platform 드라이버 등록 해제
```

runtime overlay는 실행 중인 트리에 적용한다. 부팅 설정 편집이나 재부팅 없이 추가·제거를 확인하며 기존 I2C 장치는 유지한다. 재부팅 후에도 자동 적용되는 설정을 추가하는 것은 아니다.

## 3. 현재 코드 해설

### 헤더·장치별 상태

```c
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

struct platform_state {
	const char *label;
	u32 study_value;
};
```

device.h는 장치와 sysfs 속성 선언, errno.h는 오류 코드, module.h는 모듈 메타데이터, of.h는 DT API, platform_device.h는 platform 장치 API를 제공한다. slab.h는 메모리 할당 관련 선언·플래그, sysfs.h는 sysfs_emit 선언을 위해 사용한다.

platform_state는 한 장치가 유지할 label 문자열 포인터와 u32 값을 묶는다. probe의 지역 변수는 함수 반환 뒤 사용할 수 없으므로 상태 객체는 동적으로 할당한다. 같은 드라이버가 장치 여러 개에 연결되면 각 probe가 별도 객체를 만들며 문자 디바이스 예제의 전역 공유 버퍼와 구분한다. u32는 32비트 부호 없는 정수다.

### sysfs show: cat 요청을 텍스트로 변환

```c
static ssize_t label_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	const struct platform_state *state = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", state->label);
}
static DEVICE_ATTR_RO(label);

static ssize_t study_value_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	const struct platform_state *state = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", state->study_value);
}
static DEVICE_ATTR_RO(study_value);
```

cat이 장치 속성을 읽으면 커널이 해당 show 콜백에 struct device, 속성 정보, 커널 출력 버퍼를 전달한다. attr은 콜백 형식에 맞춰 받지만 현재 직접 사용하지 않는다. dev_get_drvdata(dev)는 probe에서 연결한 상태 포인터를 가져온다. C의 void 포인터에서 해당 구조체 포인터로 받으며 상태 값은 const 포인터로 읽는다.

sysfs_emit는 sysfs 출력 버퍼의 크기 규약에 맞춰 문자열을 쓰고 출력 바이트 수를 반환한다. 이 결과를 ssize_t 반환값으로 전달한다. 줄바꿈을 포함해 문자열 또는 정수 하나를 텍스트로 보여준다. buf는 사용자 포인터가 아니라 커널이 준비한 sysfs 버퍼라 copy_to_user를 직접 호출하지 않는다.

DEVICE_ATTR_RO(label)는 이름 label·권한 0444·콜백 label_show인 dev_attr_label 객체를 정의한다. study_value도 같은 규칙이다. 이 매크로만 선언해서 파일이 생성되는 것은 아니며 속성 그룹을 장치에 등록해야 한다. store 콜백은 없어서 쓰기 인터페이스를 제공하지 않는다.

### 속성 그룹과 공개 시점

```c
static struct attribute *platform_attrs[] = {
	&dev_attr_label.attr,
	&dev_attr_study_value.attr,
	NULL,
};

static const struct attribute_group platform_group = {
	.attrs = platform_attrs,
};
```

각 device_attribute 안의 공통 attribute 멤버 주소를 배열에 넣고 NULL로 종료한다. group에 name을 지정하지 않았으므로 새 하위 디렉터리 없이 장치 디렉터리에 label·study_value 파일이 직접 생긴다. 이 파일들은 DT 노드의 원시 속성 파일과 별개인 드라이버 인터페이스다.

### probe: DT 읽기·장치 데이터 연결·자원 관리

```c
static int platform_probe(struct platform_device *pdev)
{
	struct platform_state *state;
	const char *label;
	int ret;

	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	ret = of_property_read_string(pdev->dev.of_node, "label", &label);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "[PROBE] failed to read label\n");

	ret = of_property_read_u32(pdev->dev.of_node, "nakta,study-value",
				   &state->study_value);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "[PROBE] failed to read nakta,study-value\n");

	/* sysfs 조회에 사용할 문자열을 장치 소유 메모리에 보관한다. */
	state->label = devm_kstrdup(&pdev->dev, label, GFP_KERNEL);
	if (!state->label)
		return -ENOMEM;

	platform_set_drvdata(pdev, state);

	/* 값과 조회 문맥을 준비한 뒤 속성을 공개한다. */
	ret = devm_device_add_group(&pdev->dev, &platform_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "[PROBE] failed to create sysfs attributes\n");

	dev_info(&pdev->dev, "[PROBE] label=%s, value=%u\n",
		 state->label, state->study_value);
	return 0;
}
```

pdev는 커널이 매칭한 platform 장치 포인터이고 &pdev->dev는 그 안의 공통 struct device 주소다. dev_info·dev_err_probe는 장치 문맥을 포함한 로그를 남긴다. probe의 0은 바인딩 성공, 음수는 실패다. 나중에도 호출될 수 있으므로 __init를 붙이지 않는다.

처리 순서를 따라 읽는다.

1. devm_kzalloc은 sizeof(*state)만큼 0으로 채운 장치 관리 메모리를 할당한다. GFP_KERNEL은 잠들 수 있는 일반 커널 문맥용 플래그다. 실패하면 NULL이므로 -ENOMEM을 반환한다. sizeof(*state)는 포인터 크기가 아닌 실제 상태 구조체 크기다.
2. pdev->dev.of_node에서 필수 label과 nakta,study-value를 읽는다. API 반환값 0은 성공이고 실제 데이터는 출력 인자를 통해 받는다. 실패하면 dev_err_probe로 원인을 남기고 같은 음수 오류를 반환한다.
3. of_property_read_string이 제공한 label은 DT 메모리를 가리키므로 devm_kstrdup으로 복사해 장치 소유 문자열로 보관한다. label은 문자열을 가리키는 const char *이고 &label은 포인터 변수의 주소인 const char **다. u32 API에는 &state->study_value로 구조체 멤버의 저장 위치를 전달한다. 빅 엔디언 변환은 API가 수행한다.
4. platform_set_drvdata(pdev, state)로 장치와 상태 포인터를 연결한다. 상태를 복사하는 함수가 아니라 포인터를 보관하며 show에서 dev_get_drvdata로 같은 객체를 찾는다.
5. 마지막에 devm_device_add_group으로 속성을 공개한다. 준비 전 cat 요청이 들어오는 일을 피하려고 값·drvdata를 먼저 완성한다. 그룹 생성이 실패해도 probe는 성공 로그 없이 오류를 반환한다.

필수 속성이 없어도 DT 노드는 생성될 수 있지만 probe가 실패하면 정상 바인딩과 사용자 속성 공개는 완료되지 않는다. 노드 존재·driver 링크·sysfs 속성·로그를 구분한다. API로 읽을 수 있는 데이터인지 검사하며 값 자체의 추가 범위 제한은 이번 코드에 없다.

### remove와 devm 정리 순서

```c
static void platform_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "[REMOVE] detached\n");
}
```

remove는 바인딩 해제 시 호출되는 void 콜백이다. 현재 함수는 로그만 남긴다. devm 자원은 probe 실패 또는 드라이버 바인딩 해제에 맞춰 커널이 정리하므로 여기서 kfree나 sysfs_remove_group을 중복 호출하지 않는다. 정상 해제에서는 remove 호출 후 관리 자원이 정리된다.

현재 등록 순서는 상태 메모리 → 복사 문자열 → sysfs 그룹이다. 관리 자원은 역순으로 해제돼 먼저 파일을 제거하고 진행 중인 속성 접근을 정리한 뒤 문자열·상태 메모리를 해제한다. 모든 자원이 devm이라는 이유만으로 모든 동시성 문제가 자동 해결되는 것은 아니지만, 이 예제는 값을 공개한 뒤 변경하지 않고 읽기만 하므로 데이터 갱신용 mutex를 추가하지 않는다.

sysfs 출력은 DT를 매번 다시 읽는 것이 아니라 probe에서 보관한 값이다. 값을 바꾸려면 overlay 소스를 바꿔 다시 빌드·재적용하고 새 probe를 거친다. 이번 구현에는 런타임 값 변경·store 콜백·주기적 측정이 없다.

### compatible 매칭 표

```c
static const struct of_device_id platform_match[] = {
	{ .compatible = "nakta,platform-demo" },
	{},
};
MODULE_DEVICE_TABLE(of, platform_match);
```

`struct of_device_id` 배열의 compatible은 장치 노드의 compatible과 비교할 문자열이다. `.compatible = ...`은 구조체 멤버 지정 초기화이고 마지막 빈 항목은 표의 종료를 나타낸다. const는 표를 수정하지 않겠다는 의미다.

MODULE_DEVICE_TABLE은 이 표를 모듈의 OF 별칭 정보로 제공해 사용자 공간의 모듈 선택에 사용할 수 있게 한다. 이 선언이 장치를 생성하거나 모든 환경에서 모듈 자동 로드를 보장하는 것은 아니다. 이번에는 명시적으로 insmod해서 드라이버를 먼저 준비한다.

### 드라이버 등록과 모듈 수명

```c
static struct platform_driver platform_driver = {
	.probe = platform_probe,
	.remove = platform_remove,
	.driver = {
		.name = "platform-demo",
		.of_match_table = platform_match,
	},
};
module_platform_driver(platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nakta");
MODULE_DESCRIPTION("Device Tree platform driver matching practice");
```

probe/remove 멤버에 함수 이름을 넣는 것은 지금 함수를 호출하는 것이 아니라 주소를 저장하는 것이다. `.driver.name`은 sysfs에서 드라이버를 식별할 이름이고 `.of_match_table`은 앞의 배열과 연결한다. DT 기반 매칭은 모듈 파일명과 노드 이름의 일치가 아닌 compatible 표를 기준으로 한다.

`module_platform_driver(platform_driver)`는 platform_driver_register·unregister를 호출하는 모듈 초기화·종료 경로를 생성하는 편의 매크로다. hello의 module_init/module_exit와 목적이 연결되며, 여기에 중복된 모듈 초기화 매크로를 추가하지 않는다. 장치가 없으면 probe 로그가 없을 수 있으므로 sysfs의 드라이버 등록과 별도로 판정한다. 커널이 출력하는 out-of-tree 표시는 초기화 로그와 구분한다.

### Makefile

```make
ifneq ($(KERNELRELEASE),)
obj-m := platform_drv.o
else
KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
DTC ?= $(KDIR)/scripts/dtc/dtc
PI_HOST := pi
PI_MODULE_DIR := bsp/modules
PI_OVERLAY_DIR := bsp/overlays

.PHONY: all modules overlay deploy clean

all: modules overlay

modules:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= modules

overlay: platform-overlay.dtbo

platform-overlay.dtbo: platform-overlay.dts
	"$(DTC)" -@ -I dts -O dtb -o $@ $<

deploy:
	scp platform_drv.ko $(PI_HOST):$(PI_MODULE_DIR)/
	scp platform-overlay.dtbo $(PI_HOST):$(PI_OVERLAY_DIR)/

clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" \
		ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" LOCALVERSION= clean
	$(RM) platform-overlay.dtbo
endif
```

기존 모듈과 같은 KERNELRELEASE 분기·KDIR·ARM64 크로스 도구·LOCALVERSION= 조건이다. obj-m은 platform_drv.o로 지정해 platform_drv.ko를 만든다. 자세한 Make 문법은 [hello 모듈 Makefile 해설](../hello_module/kernel-module.md#makefile의-핵심)을 참고한다.

인자 없는 make는 modules와 overlay를 모두 만든다. make modules는 모듈만, make overlay는 overlay만 만든다. make deploy는 전송만 수행하며 빌드나 원격 디렉터리 생성을 자동 수행하지 않는다. clean은 Kbuild로 모듈 산출물을 정리하고 실습 dtbo도 삭제한다. dtbo는 .gitignore로 제외한다.

DTC는 KDIR/scripts/dtc/dtc를 기본값으로 사용한다. 이 도구는 기존 커널 빌드 중 WSL 호스트용으로 생성됐으므로 WSL에서 실행한다. ARM64 모듈 컴파일러와 구분한다. `-I dts -O dtb`는 텍스트 입력과 바이너리 출력을 지정하며 overlay도 DT 바이너리 형식이지만 .dtbo 이름으로 구분한다. `-@`는 overlay의 심볼·참조 처리에 사용하는 정보를 생성하게 한다. 현재 예제는 외부 phandle 대신 target-path를 사용한다. `$<`는 입력 소스, `$@`는 출력 dtbo다. 소스가 바뀐 경우에만 해당 파일을 다시 만드는 Make 의존성을 사용한다.

### overlay: 장치를 추가하는 범위와 매칭

```dts
/dts-v1/;
/plugin/;

/ {
	compatible = "brcm,bcm2711";

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
};
```

`/dts-v1/;`은 DTS 문법 선언이고 `/plugin/;`은 overlay임을 표시한다. overlay 최상위 compatible은 대상 플랫폼 계열 brcm,bcm2711을 나타내며, 새 장치의 nakta,platform-demo와 역할이 다르다.

fragment@0은 적용할 변경 묶음이다. `target-path = "/"`로 실행 중인 트리의 루트를 대상으로 정하고, `__overlay__` 안의 platform-demo를 그 아래 추가한다. 이를 루트 전체 교체나 기존 soc 노드 수정으로 해석하지 않는다. 이 노드는 레지스터 영역이 없는 논리적 실습 장치라 reg와 @주소를 넣지 않았다.

label은 끝 NUL을 가진 문자열이고 `nakta,study-value = <42>`는 32비트 셀 하나다. 런타임 바이트 출력은 00 00 00 2a이며 텍스트 숫자 "42"나 ASCII 바이트와 다르다. 이 사용자 정의 속성은 실제 센서 설정이 아닌 값 전달 학습용이다. 문자열·정수를 바꾸면 DTS를 다시 빌드하고 overlay를 제거·재적용해야 새 속성이 전달된다.

노드의 compatible이 platform_match와 일치하고 status=okay이므로 커널이 사용 가능한 장치로 처리할 수 있다. 대상 커널 drivers/of/platform.c의 of_platform_notify는 루트 또는 platform bus 아래 동적으로 추가된 노드에 대해 platform 장치를 생성하고, 제거 때 장치를 정리하는 경로를 갖는다. 이 예제가 센서 없이도 probe/remove를 확인할 수 있는 근거다.

루트 아래 노드 이름 platform-demo, driver.name의 platform-demo, 파일명 platform-overlay는 각각 다른 역할이다. 앞의 둘은 보기 쉽게 같게 정했지만 DT 드라이버 매칭의 핵심은 compatible이다. dtoverlay에는 생성한 platform-overlay.dtbo를 찾을 디렉터리와 overlay 이름을 전달한다.

## 4. 기존 I2C 연결에서 확인한 개념

### DTS에서 실행 중인 장치까지

```text
DTS/DTSI: 하드웨어 노드와 속성을 기술
  → dtbs 빌드로 DTB 생성
  → Pi 부팅 과정에서 선택·속성 및 overlay 반영
  → 커널이 실행 중인 트리를 사용
  → platform 장치 생성·드라이버 매칭
  → I2C 컨트롤러 probe가 I2C adapter 등록
```

소스의 루트 model은 사람이 읽을 보드 설명이고 compatible은 소프트웨어가 하드웨어를 식별하는 문자열 목록이다. 루트의 `raspberrypi,4-model-b`, `brcm,bcm2711`과 개별 I2C 노드의 compatible은 서로 다른 계층이다. 실행 중 model에는 소스의 기본 문자열 외에 보드 리비전 정보도 반영돼 있었다.

`/proc/device-tree`와 `/sys/firmware/devicetree/base`는 커널이 사용하는 트리를 읽는 경로다. 폴더는 노드이고 파일은 속성이며, 이것을 읽는 것으로 새 노드를 등록하지 않는다. 문자열 목록은 NUL로 구분되므로 tr로 줄바꿈 변환해 보고 숫자 속성은 바이너리로 해석한다. DTB 파일명 자체의 출처를 문자열 비교만으로 입증하는 것은 아니다.

I2C 컨트롤러의 reg 출력은 `7e 80 40 00 00 00 10 00`이었다. 이 노드의 부모 표현 기준에서 32비트 빅 엔디언 셀 하나는 주소 0x7e804000, 다음 셀은 크기 0x1000(4096바이트)이다. 일반적인 reg 해석은 부모의 #address-cells·#size-cells를 따라야 하며 모든 노드가 이 형식인 것은 아니다. I2C 센서의 버스 주소와 컨트롤러 레지스터 주소도 구분한다.

선택 소스의 soc/ranges는 0x7e000000 버스 영역을 CPU 물리 주소 0xfe000000으로 매핑한다. 컨트롤러 offset 0x00804000을 더하면 0xfe804000이다. DT 경로의 i2c@7e804000과 platform 장치 fe804000.i2c가 다르게 보인 이유다.

### DT 노드·platform 장치·드라이버·adapter

```text
/soc/i2c@7e804000
  → /sys/bus/platform/devices/fe804000.i2c
  → driver: /sys/bus/platform/drivers/i2c-bcm2835
  → adapter: /sys/bus/i2c/devices/i2c-1
```

platform 장치는 Pi 내부 컨트롤러를 커널 장치 모델에 표현하고, I2C adapter는 그 컨트롤러를 통해 I2C 전송을 수행할 버스다. 센서가 없어도 버스는 준비될 수 있다. 버스 번호 1은 센서 주소나 GPIO 핀 번호가 아니다.

of_node는 커널 장치를 원래 DT 노드로 연결하고 driver는 현재 담당 드라이버로 연결한다. adapter의 실제 경로를 통해 컨트롤러 아래 생성된 자식임을 확인한다. compatible 표의 일치 가능성과 실제 바인딩 성공은 구분하고, status=okay도 센서 존재나 모든 기능 정상 동작까지 뜻하지 않는다.

readlink -f는 마지막 요소가 없어도 경로를 출력할 수 있으므로 존재 확인이 필요할 때는 -e를 사용한다. adapter를 찾을 때는 `/sys/bus/i2c/devices`를 사용한다. 없는 glob이 문자 그대로 처리되는 일을 막으려면 반복문에서 디렉터리 여부를 검사한다. 새 실습의 명령과 중복하지 않도록 기존 컨트롤러 확인 결과는 아래 표·대표 출력으로 보존한다.

## 5. 수업 자료와 기존 드라이버 비교

주요 수업 자료는 [임베디드리눅스1.pdf](../../../VEDA/임베디드리눅스/강의자료/임베디드리눅스1.pdf) 23~27쪽이다. 장치 트리 목적·노드·속성·compatible·reg·셀 크기를 현재 소스와 함께 읽는다. 22쪽은 커널에 전달하는 부팅 정보, 29쪽은 Raspberry Pi overlay 사용 예다. PDF 뷰어의 1부터 시작하는 페이지 번호 기준이다.

선택 커널의 비교 대상은 [i2c-bcm2835.c](../../external/linux/drivers/i2c/busses/i2c-bcm2835.c) 523행의 probe와 631행 이후 매칭·드라이버 표다. 참고한 Raspberry Pi 소스 커밋은 b3495ea401d709af0701a85babcba5e3d788a602다.

| 기존 I2C 컨트롤러 드라이버 | 이번 실습 |
| --- | --- |
| brcm 호환 문자열·실제 SoC 컨트롤러 | 로컬 학습용 compatible·자원 없는 장치 |
| probe가 레지스터 매핑·클럭·IRQ 준비 | 필수 DT 속성 읽기·장치별 상태·읽기 전용 sysfs와 성공/실패 반환 구현 |
| i2c_add_adapter로 버스 등록 | adapter를 등록하지 않음 |
| platform_driver와 OF 표로 매칭 | 같은 등록·매칭 구조 사용 |

이 실습은 VEDA GPIO 예제를 그대로 옮긴 구현이 아니다. 수업의 DT 개념을 작은 platform 드라이버로 연결하며, 직접 대응하는 VEDA platform C 예제와의 차이를 확인한 것처럼 기록하지 않는다.

## 6. 빌드·전송·등록 확인과 검증

### 선행 조건

WSL은 기존 ARM64 크로스 GCC·Make·file·modinfo·ssh·scp를 사용한다. KDIR는 커밋 b3495ea401d709af0701a85babcba5e3d788a602의 전체 커널 빌드 출력 out/kernel-rpi4다. 설정은 bcm2711_defconfig·LOCALVERSION=-v8-bsp-study01·LOCALVERSION_AUTO=n이며 명령의 LOCALVERSION=을 유지한다. KDIR/scripts/dtc/dtc가 호스트용 도구로 존재해야 한다.

Pi는 6.18.59-v8-bsp-study01로 부팅하고 SSH 별칭 pi와 dtoverlay를 사용한다. 모듈은 ~/bsp/modules, overlay는 ~/bsp/overlays에 둔다. 부팅 파일을 덮어쓰거나 config.txt에 dtoverlay 항목을 추가하지 않는다.

### 1) WSL 빌드와 전송

**WSL · 작업 디렉터리: platform 예제 · 위 커널 빌드 환경 준비 후**

```bash
cd ~/bspworkspace/bsp-study/examples/platform_device
make
echo $?
file platform_drv.ko platform-overlay.dtbo
modinfo -F vermagic ./platform_drv.ko
modinfo -F alias ./platform_drv.ko
```

종료 상태 0, ARM aarch64 모듈과 DT 바이너리 형식, Pi 커널과 같은 vermagic, nakta,platform-demo 문자열을 포함한 OF 별칭을 확인한다. 빌드 성공 후에만 전송한다.

**WSL · 작업 디렉터리: platform 예제 · 빌드 확인 후**

```bash
cd ~/bspworkspace/bsp-study/examples/platform_device
ssh pi 'mkdir -p ~/bsp/modules ~/bsp/overlays'
make deploy
ssh pi
```

### 2) 드라이버 등록과 적용 전 상태

**Pi · 작업 디렉터리: ~/bsp/modules · 전송 완료 후**

```bash
cd ~/bsp/modules
uname -r
modinfo -F vermagic ./platform_drv.ko
lsmod | grep '^platform_drv '
```

대상 커널 버전을 확인한다. 이미 platform_drv가 로드돼 있고 C 코드를 바꾸지 않았다면 다시 로드하지 않는다. 미로드 상태일 때만 다음을 실행한다. C 코드를 수정해 교체해야 할 때는 overlay를 먼저 제거하고 모듈을 해제·재로드하는 순서로 진행한다.

**Pi · 작업 디렉터리: ~/bsp/modules · platform_drv 미로드, 버전 일치 확인 후에만**

```bash
cd ~/bsp/modules
sudo insmod ./platform_drv.ko
```

**Pi · 작업 디렉터리: ~/bsp/modules · 드라이버 로드 상태**

```bash
cd ~/bsp/modules
ls -ld /sys/bus/platform/drivers/platform-demo
readlink -e /sys/bus/platform/drivers/platform-demo/module
sudo dtoverlay -l
ls -ld /proc/device-tree/platform-demo
sudo dmesg | grep -F 'platform-demo'
```

처음에는 모듈·드라이버 디렉터리만 존재하고 실습 노드·PROBE 로그는 없는 상태를 확인한다. dtoverlay 목록에 이미 같은 실습 overlay가 있다면 중복 적용하지 않는다. 이 목록은 runtime overlay 관리 목록으로, config.txt의 전체 부팅 overlay 목록과 같다고 보지 않는다.

### 3) overlay 적용과 probe·바인딩 확인

**Pi · 작업 디렉터리: ~/bsp/overlays · 드라이버 등록·실습 overlay 미적용 확인 후**

```bash
cd ~/bsp/overlays
sudo dtoverlay -d "$PWD" platform-overlay
sudo dtoverlay -l
tr '\0' '\n' < /proc/device-tree/platform-demo/compatible
tr '\0' '\n' < /proc/device-tree/platform-demo/label
od -An -tx1 /proc/device-tree/platform-demo/nakta,study-value
readlink -e /sys/bus/platform/devices/platform-demo/of_node
readlink -e /sys/bus/platform/devices/platform-demo/driver
sudo dmesg | grep -F 'platform-demo'
```

-d는 overlay 파일 탐색 디렉터리다. PWD는 cd한 사용자의 셸에서 절대 경로로 확장되므로 시스템 기본 overlay 디렉터리에 복사할 필요가 없다. platform-overlay는 생성 파일에서 .dtbo를 제외한 이름이다.

예상 결과는 runtime 목록에 실습 overlay 추가, 노드 compatible이 nakta,platform-demo, of_node가 /sys/firmware/devicetree/base/platform-demo, driver가 /sys/bus/platform/drivers/platform-demo로 연결되는 것이다. 문자열은 platform study, 정수 바이트는 00 00 00 2a이며 새 시간대의 `[PROBE] label=platform study, value=42` 로그를 함께 확인한다. dev_info의 장치 접두사가 붙으므로 수동으로 로그 전체 형식을 고정하지 않는다.

### sysfs 값 조회

**Pi · 작업 디렉터리: ~/bsp/overlays · 새 버전 모듈·overlay 적용과 probe 성공 확인 후**

```bash
cd ~/bsp/overlays
ls -l /sys/bus/platform/devices/platform-demo/label /sys/bus/platform/devices/platform-demo/study_value
cat /sys/bus/platform/devices/platform-demo/label
cat /sys/bus/platform/devices/platform-demo/study_value
```

파일의 읽기 전용 권한과 각각 platform study, 42 출력을 확인한다. root 권한으로 파일 내용을 변경하는 실습은 하지 않는다. DT 원시 숫자는 바이너리지만 이 sysfs 정수는 show 콜백이 출력한 십진수 텍스트다. /dev 파일을 만들 필요는 없다.

### 4) overlay 제거와 remove 확인

**Pi · 작업 디렉터리: ~/bsp/overlays · 이 실습의 platform-overlay가 적용된 상태**

```bash
cd ~/bsp/overlays
sudo dtoverlay -l
sudo dtoverlay -r platform-overlay
sudo dtoverlay -l
ls -ld /proc/device-tree/platform-demo
ls -ld /sys/bus/platform/devices/platform-demo
ls -l /sys/bus/platform/devices/platform-demo/label /sys/bus/platform/devices/platform-demo/study_value
ls -ld /sys/bus/platform/drivers/platform-demo
sudo dmesg | grep -F 'platform-demo'
```

목록에서 이름이 platform-overlay인지 확인하고 이 overlay만 제거한다. 다른 이름·예상하지 못한 상태라면 결과를 먼저 검토한다. 이름 없는 -r이나 -R로 다른 runtime overlay까지 제거하지 않는다.

노드·platform 장치·두 sysfs 속성은 사라지고 `[REMOVE] detached` 로그가 추가되며 드라이버 디렉터리는 남는 것이 기대 동작이다. 장치를 제거한 것과 드라이버 등록 해제는 별개이기 때문이다. ls의 노드 부재 오류는 이 정리 상태를 확인하는 결과다.

### 5) 드라이버 정리

**Pi · 작업 디렉터리: ~/bsp/modules · 실습 overlay 제거 확인 후**

```bash
cd ~/bsp/modules
sudo rmmod platform_drv
lsmod | grep '^platform_drv '
ls -ld /sys/bus/platform/drivers/platform-demo
```

모듈·드라이버 디렉터리가 사라지는지 확인한다. 모듈 제거와 장치 노드 제거를 각각 관측한 것으로 정리한다. 이 예제에는 문자 장치 노드가 없으므로 mknod나 /dev 파일 삭제는 하지 않는다.

### 검증 현황

기존 컨트롤러는 사용자 제공 Pi 출력으로 다음 연결을 확인했다.

| 관측 | 결과 |
| --- | --- |
| 루트 model·compatible | Raspberry Pi 4 Model B Rev 1.5, raspberrypi,4-model-b와 brcm,bcm2711 |
| 컨트롤러 속성 | brcm,bcm2711-i2c·brcm,bcm2835-i2c, status okay, reg 주소·크기 |
| platform of_node | fe804000.i2c에서 soc/i2c@7e804000으로 연결 |
| platform driver | i2c-bcm2835에 바인딩 |
| I2C adapter | i2c-1 이름 bcm2835 (i2c@7e804000) |

대표 사용자 출력:

```text
/sys/bus/platform/drivers/i2c-bcm2835
/sys/devices/platform/soc/fe804000.i2c/i2c-1
/sys/firmware/devicetree/base/soc/i2c@7e804000
```

새 실습 드라이버는 2026-11-05 사용자 출력으로 모듈 정보·Pi 로드·드라이버 등록을 확인했다.

```text
vermagic:
6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64
OF aliases:
of:N*T*Cnakta,platform-demoC*
of:N*T*Cnakta,platform-demo

platform_drv           12288  0
/sys/bus/platform/drivers/platform-demo
/sys/module/platform_drv
```

모듈은 등록됐고 매칭 장치 생성 전이라 platform-demo 로그는 없었다. `/usr/bin/dtoverlay`와 사용법 출력에서 -d 경로 지정·-l 목록·-r 제거 기능과 root 권한 요구도 확인했다.

2026-11-05 runtime overlay 적용·제거와 최종 정리 실측:

| 단계 | 사용자 출력으로 확인한 결과 |
| --- | --- |
| overlay 적용 | 목록 0: platform-overlay, 노드 compatible nakta,platform-demo, driver 링크 platform-demo |
| 장치 매칭 | 10764초대 PROBE 로그 |
| overlay 제거 | No overlays loaded, DT 노드·platform 장치 부재, 드라이버 디렉터리 유지 |
| 바인딩 해제 | 11026초대 REMOVE 로그 |
| 모듈 해제 | lsmod 검색 결과 없음, 드라이버 디렉터리 부재 |

대표 로그:

```text
[10764.272796] platform-demo platform-demo: [PROBE] matched
[11026.800909] platform-demo platform-demo: [REMOVE] detached
```

장치가 없는 상태에서 드라이버를 먼저 등록하고 overlay로 장치를 추가했을 때 probe가 호출됐다. overlay를 제거하면 장치와 바인딩은 정리되지만 드라이버는 남았고, 마지막 rmmod로 드라이버도 정리됐다. 같은 compatible을 사용하는 실제 장치 수명과 모듈 수명을 구분해 관측한 결과다.

sysfs 추가 전 DT 속성 읽기 버전의 실측(2026-11-05):

| 항목 | 결과 |
| --- | --- |
| label 원시 문자열 | platform study |
| nakta,study-value 원시 바이트 | 00 00 00 2a |
| probe 해석·바인딩 | label=platform study, value=42, driver 링크 platform-demo |
| 정리 | REMOVE 로그 추가, No overlays loaded, lsmod 검색 결과 없음, 드라이버 디렉터리 부재 |

```text
[12000.749503] platform-demo platform-demo: [PROBE] label=platform study, value=42
[12172.283822] platform-demo platform-demo: [REMOVE] detached
```

DT의 문자열·32비트 빅 엔디언 셀을 probe가 읽어 출력하는 정상 경로와 이후 해제를 확인했다. 앞의 matched 로그는 속성 읽기 추가 전 결과이고, 12000초대 로그가 sysfs 추가 전 속성 읽기 결과다. remove 로그는 커널 로그에 남으며 정리 명령 자체가 터미널에 별도 성공 메시지를 출력해야 하는 것은 아니다.

현재 sysfs 버전 실측(2026-11-05):

| 항목 | 사용자 제공 결과 |
| --- | --- |
| label 조회 | platform study |
| study_value 조회 | 42 |
| 파일 권한 | 두 속성 모두 -r--r--r-- (0444) |
| overlay 제거 | label·study_value 파일 부재와 새 REMOVE 로그 |
| 모듈 정리 | sudo rmmod platform_drv가 오류 출력 없이 완료 |

```text
[13085.150473] platform-demo platform-demo: [PROBE] label=platform study, value=42
[13285.313547] platform-demo platform-demo: [REMOVE] detached
```

cat이 show 콜백에서 장치별 저장값을 읽는 경로와 바인딩 해제 후 속성 파일이 제거되는 결과를 확인했다. ls에 표시된 4096은 실제 문자열 길이가 아닌 sysfs 속성의 보고 크기다. 파일 부재만으로 메모리 해제까지 직접 관측한 것으로 보지는 않는다.

- 에이전트는 대상 커널 platform_device.h·of/platform.c, 기존 i2c-bcm2835 드라이버와 gpio-led overlay 형식을 읽기 대조했다. WSL의 기존 .config에서 CONFIG_OF_DYNAMIC=y·CONFIG_OF_OVERLAY=y, 커널 출력의 호스트 dtc 파일 존재를 확인했다.
- 사용자 출력으로 runtime overlay 적용·노드 compatible·driver 바인딩·probe, overlay 제거·remove·장치 부재·드라이버 유지, 최종 모듈 해제·드라이버 부재까지 확인했다. 이 결과는 속성 읽기 추가 전 버전이며 부팅 설정 변경·재부팅 없이 수행한 기록이다.
- sysfs 추가 전 속성 읽기 버전은 사용자 출력으로 문자열·정수 바이트·probe 로그·바인딩과 최종 overlay/모듈 정리를 확인했다. 필수 속성 누락·형식 오류의 probe 실패 경로는 아직 미검증이다.
- 현재 sysfs 버전은 사용자 출력으로 두 값·읽기 전용 권한·overlay 제거 후 속성 부재와 REMOVE 로그를 확인했고 rmmod가 오류 출력 없이 완료됐다. 이 버전의 해제 후 lsmod·드라이버 링크 재확인과 메모리 해제 추적은 별도 수행하지 않았다. 메모리 자동 정리는 devm API 구현을 읽기 대조한 동작 설명이다.
- 기존 모듈 vermagic·별칭과 Pi 로드 출력은 받았지만 make 종료 상태·file 원시 출력과 deploy 자체 실행 출력은 별도 미수신이다.
- 앞선 잘못된 /sys/class/i2c-adapter 안내와 존재하지 않는 device 말단에 대한 readlink -f 출력은 등록 실패 근거로 사용하지 않는다. 대상 소스의 I2C sysfs 문서와 실제 경로로 확인을 바로잡았다.
