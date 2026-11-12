# 16 초음파 IIO srf04 — Codex 지시서

작업 노트: [16-ultrasonic.md](16-ultrasonic.md)

## 지시 (Claude)
- 변경 파일:
  - 새 파일 `configs/bsp-study.config`
  - 새 파일 `examples/ultrasonic/srf04-overlay.dts`
  - 새 파일 `examples/ultrasonic/Makefile`
  - `.gitignore`: `/examples/ultrasonic/*.dtbo` 한 줄 추가
- 요구사항 — `configs/bsp-study.config`:
  - `CONFIG_SRF04=m` 한 줄과, 이 파일이 `merge_config.sh`로 `out/kernel-rpi4/.config`에 합치는 fragment라는 한글 주석 한 줄
  - 다른 설정은 넣지 않는다
- 요구사항 — `srf04-overlay.dts`:
  - 대상 커널 `Documentation/devicetree/bindings/iio/proximity/devantech-srf04.yaml`과 `drivers/iio/proximity/srf04.c`를 기준으로 한다
  - `compatible = "brcm,bcm2835"` 최상위, fragment 하나: `target-path = "/"`에 노드 `proximity`(라벨 `srf04`)
    - `compatible = "devantech,srf04"`
    - `trig-gpios = <&gpio 23 0>`(GPIO_ACTIVE_HIGH), `echo-gpios = <&gpio 24 0>`
    - `status = "okay"`
  - `#include` 없이 숫자로 쓴다(Makefile이 cpp 없이 dtc를 직접 부름). 플래그 0이 ACTIVE_HIGH라는 것만 한글 주석으로
  - `power-gpios`, `startup-time-ms`, pinctrl·풀 설정, `__overrides__`는 넣지 않는다
- 요구사항 — `Makefile`: `examples/mic/Makefile` 형식
  - `KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)`, `DTC ?= $(KDIR)/scripts/dtc/dtc`
  - `all`/`overlay`: `srf04.dtbo`를 `"$(DTC)" -@ -I dts -O dtb`로 생성(출력 이름은 `dtoverlay=srf04`에 맞춤)
  - `deploy`: `scp srf04.dtbo pi:bsp/overlays/`
  - `clean`, `.PHONY`
- 하지 말 것: `out/kernel-rpi4/.config` 수정, `merge_config.sh`·`olddefconfig`·커널·모듈 빌드 실행, 드라이버 수정, 사용자 앱, 보드 조작
- 빌드 확인 명령:

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/ultrasonic
make
file srf04.dtbo
"$HOME/bspworkspace/bsp-study/out/kernel-rpi4/scripts/dtc/dtc" -I dtb -O dts srf04.dtbo
git check-ignore -v srf04.dtbo
make -n deploy
cat ../../configs/bsp-study.config
```

역변환 결과에서 `__fixups__`의 `gpio`가 `trig-gpios`·`echo-gpios`를 가리키는지 확인한다. dtc 경고가 있으면 그대로 적는다.

## 구현 보고 (Codex)
- 변경 요약:
  - `configs/bsp-study.config`: fragment 용도를 설명하는 한글 주석과 `CONFIG_SRF04=m`만 추가했다.
  - `examples/ultrasonic/srf04-overlay.dts`: 루트 대상 fragment 하나에 `srf04: proximity`를 만들고, `devantech,srf04`, Trig GPIO23·Echo GPIO24, ACTIVE_HIGH 플래그 0, `status = "okay"`를 지정했다.
  - `examples/ultrasonic/Makefile`: `examples/mic/Makefile` 형식으로 `all`·`overlay`·`deploy`·`clean`을 작성하고 출력 이름을 `srf04.dtbo`로 맞췄다.
  - `.gitignore`: `/examples/ultrasonic/*.dtbo` 한 줄을 추가했다.
- 빌드 결과:
  - `examples/ultrasonic`에서 `make`: 종료 코드 0, 경고 없이 `srf04.dtbo` 생성. 실제 명령은 `"/home/nakta/bspworkspace/bsp-study/out/kernel-rpi4/scripts/dtc/dtc" -@ -I dts -O dtb -o srf04.dtbo srf04-overlay.dts`였다.
  - `file srf04.dtbo`: `Device Tree Blob version 17`, 크기 555바이트.
  - `"$HOME/bspworkspace/bsp-study/out/kernel-rpi4/scripts/dtc/dtc" -I dtb -O dts srf04.dtbo`: 종료 코드 0. `trig-gpios = <0xffffffff 0x17 0x00>`, `echo-gpios = <0xffffffff 0x18 0x00>`와 `__symbols__.srf04`를 확인했다. `__fixups__.gpio`는 `/fragment@0/__overlay__/proximity:trig-gpios:0`와 `/fragment@0/__overlay__/proximity:echo-gpios:0` 두 항목이다.
  - 역변환 경고 원문:

    ```text
    <stdout>: Warning (gpios_property): /fragment@0/__overlay__/proximity:trig-gpios: Could not get phandle node for (cell 1)
    <stdout>: Warning (gpios_property): /fragment@0/__overlay__/proximity:echo-gpios: Could not get phandle node for (cell 1)
    <stdout>: Warning (gpios_property): /__fixups__:gpio: property size (94) is invalid, expected multiple of 4
    ```

  - `git check-ignore -v srf04.dtbo`: `.gitignore:7:/examples/ultrasonic/*.dtbo` 규칙 적용 확인.
  - `make -n deploy`: `scp srf04.dtbo pi:bsp/overlays/` 출력 확인. 실제 배포는 실행하지 않았다.
  - `cat ../../configs/bsp-study.config`: 한글 주석 한 줄과 `CONFIG_SRF04=m` 한 줄 확인.
  - 저장소 루트에서 `git diff --check`: 종료 코드 0, 출력 없음.
  - config 병합·커널/모듈 빌드·설치·부팅·보드 기능 검증은 실행하지 않았다.
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`): C 소스 변경이 없어 해당 없음. 실행하지 않았다.
- 판단이 필요했던 부분:
  - 대상 커널의 `Documentation/devicetree/bindings/iio/proximity/devantech-srf04.yaml`과 `drivers/iio/proximity/srf04.c`에서 compatible, 필수 GPIO 속성, GPIO 취득과 Echo 양쪽 에지 IRQ 처리를 읽기 확인했다. 현재 `out/kernel-rpi4/.config`는 `# CONFIG_SRF04 is not set`, `CONFIG_GPIOLIB=y`, `CONFIG_IIO=m`이며 수정하지 않았다.
  - GPIO phandle이 외부 참조인 단독 DTBO의 역변환에서 위 경고가 발생했다. 요구한 fixup 두 항목은 생성되어 있으며, 경고를 숨기는 옵션이나 추가 속성은 넣지 않았다. 실제 base DT에 적용한 결과와 보드 동작은 미검증이다.

## 리뷰 (Claude)
- 결과: 통과
- 범위: 새 파일 `configs/bsp-study.config`, `examples/ultrasonic/` 2개, `.gitignore` 한 줄. `out/kernel-rpi4/.config`와 드라이버는 바뀌지 않음(`git status`)
- fragment: 주석 한 줄 + `CONFIG_SRF04=m`뿐(`configs/bsp-study.config` L2). 합치기·빌드는 사용자 단계
- overlay: `compatible = "devantech,srf04"`(`srf04.c` L235), 필수 `trig-gpios`·`echo-gpios`가 binding과 맞고 GPIO23·24는 [bringup](../bringup.md#40핀-한눈에-보기) 계획·VEDA wiringPi 4·5와 같음. `&gpio`는 2셀(번호, 플래그)이라 `<&gpio 23 0>` 형식이 맞음. 노드 이름 `proximity`는 binding 예시와 같고 `/proc/interrupts`의 IRQ 이름이 됨(`srf04.c` L306 `pdev->name`)
- 역변환 경고 3줄: 외부 phandle(`&gpio`)을 아직 모르는 단독 DTBO를 역변환할 때 나오는 경고로, 이미 보드에서 동작한 `mpu6050-iio-overlay.dtbo` 역변환에서도 같은 `gpios_property`·`__fixups__` 경고가 나옴. firmware가 base DT에 적용할 때 `__fixups__.gpio` 두 항목으로 해결되므로 문제 아님
- Makefile: `examples/mic/Makefile` 형식과 같음. 빌드·`make -n deploy`만 실행
- 수정 지시: 없음
