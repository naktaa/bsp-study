# 14-3 INMP441 코덱 드라이버 — Codex 지시서

작업 노트: [14-3-mic-codec.md](14-3-mic-codec.md)

## 지시 (Claude)
- 변경 파일:
  - 새 파일 `examples/mic_codec/inmp441_codec.c`
  - 새 파일 `examples/mic_codec/inmp441-study-overlay.dts`
  - 새 파일 `examples/mic_codec/Makefile`
  - `.gitignore`: `/examples/mic_codec/*.dtbo` 한 줄 추가
- 요구사항 — `inmp441_codec.c` (외부 모듈, 대상 커널 `sound/soc/codecs/ics43432.c` 구조를 참고하되 아래 값으로):
  - `struct snd_soc_dai_driver` 하나, `.name = "inmp441-hifi"`, `.capture`만 정의
    - `.stream_name = "Capture"`, `.channels_min = 1`, `.channels_max = 2`
    - `.rate_min = 7800`, `.rate_max = 50000`, `.rates = SNDRV_PCM_RATE_CONTINUOUS`
    - `.formats = SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE`
    - 레이트 숫자는 매크로로 두고 "INMP441 데이터시트 WS 7.8~50kHz" 출처를 한글 주석 한 줄로(`docs/hardware.md` INMP441 항목)
  - `struct snd_soc_component_driver`: `ics43432`의 `idle_bias_on`·`use_pmdown_time`·`endianness` 각각을 `include/sound/soc-component.h`와 사용처로 확인하고, 이 마이크에 필요한 것만 넣는다. 넣고 뺀 이유를 구현 보고에 적는다
  - probe: `devm_snd_soc_register_component(&pdev->dev, ...)` 호출. 실패하면 `dev_err_probe()`로 반환, 성공하면 `dev_info(&pdev->dev, "[CODEC] registered: 7800-50000 Hz, S24_LE/S32_LE\n")`
  - remove 함수는 두지 않는다(devm 해제). 이유를 구현 보고에 적는다
  - `of_device_id`: `{ .compatible = "nakta,inmp441" }`, `MODULE_DEVICE_TABLE(of, ...)`
  - `struct platform_driver`, `.driver.name = "inmp441-codec"`, `module_platform_driver()`
  - `MODULE_LICENSE("GPL")`, `MODULE_DESCRIPTION`, `MODULE_AUTHOR`
  - AGENTS.md 코드 가독성(커널 스타일, 처리 단위 사이 빈 줄, 주석은 한글로 필요한 곳만)
- 요구사항 — `inmp441-study-overlay.dts`:
  - `examples/mic/inmp441-overlay.dts`를 복사하고 두 곳만 바꾼다: 코덱 노드 `compatible = "nakta,inmp441"`, `simple-audio-card,name = "inmp441study"`
  - 노드 라벨·fragment 구조·`__overrides__`는 그대로
- 요구사항 — `Makefile`: `examples/mpu6050_iio/Makefile` 형식(앱 부분 제외)
  - `obj-m := inmp441_codec.o`, `KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)`, `ARCH ?= arm64`, `CROSS_COMPILE ?= aarch64-linux-gnu-`, `DTC ?= $(KDIR)/scripts/dtc/dtc`
  - `all`(modules + overlay), `modules`, `overlay`(`inmp441-study.dtbo`), `deploy`(`inmp441_codec.ko` → `pi:bsp/modules/`, `inmp441-study.dtbo` → `pi:bsp/overlays/`), `clean`, `.PHONY`
- 하지 말 것: CPU DAI·DMA·machine 드라이버 작성, `/lib/modules` 설치나 modules_install, 커널 config fragment 변경, `examples/mic/` 기존 파일 수정, DAPM 위젯·컨트롤 추가, 보드 조작
- 빌드 확인 명령:

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic_codec
make
file inmp441_codec.ko inmp441-study.dtbo
modinfo -F vermagic ./inmp441_codec.ko
modinfo -F alias ./inmp441_codec.ko                 # of:N*T*Cnakta,inmp441 포함
modinfo -F depends ./inmp441_codec.ko
"$HOME/bspworkspace/bsp-study/out/kernel-rpi4/scripts/dtc/dtc" -I dtb -O dts inmp441-study.dtbo | grep -E 'compatible|name ='
../../external/linux/scripts/checkpatch.pl --no-tree -f inmp441_codec.c
git check-ignore -v inmp441-study.dtbo inmp441_codec.ko
```

## 구현 보고 (Codex)
- 변경 요약:
  - `examples/mic_codec/inmp441_codec.c`: 캡처 전용 `inmp441-hifi` DAI(1~2채널, 연속 7800~50000Hz, S24_LE·S32_LE), `nakta,inmp441` OF 매칭, devm component 등록과 성공·실패 로그를 구현했다. 레이트는 매크로로 두고 `docs/hardware.md` INMP441 항목을 출처 주석으로 남겼다.
  - `examples/mic_codec/inmp441-study-overlay.dts`: 기존 `examples/mic/inmp441-overlay.dts`와 비교해 코덱 compatible과 카드 이름 두 곳만 변경했다. 라벨·fragment·`__overrides__`는 유지했다.
  - `examples/mic_codec/Makefile`: 지정된 Kbuild·크로스 빌드 변수와 `all`, `modules`, `overlay`, `deploy`, `clean` 타깃을 작성했다. 배포 대상은 `pi:bsp/modules/`, `pi:bsp/overlays/`다.
  - `.gitignore`: `/examples/mic_codec/*.dtbo` 한 줄을 추가했다.
- 빌드 결과:
  - 실행 위치: WSL `examples/mic_codec`. `make` 종료 코드 0, 컴파일러·DTC 경고 없이 `inmp441_codec.ko`, `inmp441-study.dtbo` 생성.
  - `file inmp441_codec.ko inmp441-study.dtbo`: 모듈은 ELF 64-bit LSB relocatable, ARM aarch64. DTBO는 Device Tree Blob version 17, 1475바이트.
  - `modinfo -F vermagic ./inmp441_codec.ko`: `6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64`.
  - `modinfo -F alias ./inmp441_codec.ko`: `of:N*T*Cnakta,inmp441C*`, `of:N*T*Cnakta,inmp441`.
  - `modinfo -F depends ./inmp441_codec.ko`: `snd-soc-core`.
  - `../../out/kernel-rpi4/scripts/dtc/dtc -I dtb -O dts inmp441-study.dtbo | rg 'compatible|name ='`: 코덱 `nakta,inmp441`, 카드 compatible `simple-audio-card`, 카드 이름 `inmp441study` 확인. 역변환 경고 없음.
  - 저장소 루트에서 `diff -u examples/mic/inmp441-overlay.dts examples/mic_codec/inmp441-study-overlay.dts`: 지정된 두 문자열만 차이남을 확인.
  - `git check-ignore -v inmp441-study.dtbo inmp441_codec.ko`: DTBO는 새 경로 규칙, 모듈은 기존 `*.ko` 규칙으로 무시됨을 확인.
  - `git diff --check`: 오류 없음.
  - 미검증: 보드 배포·적재·probe 지연·카드 등록·녹음·레이트 협상·모듈 해제. `deploy`, 전체 커널 빌드, 설치와 보드 조작은 실행하지 않았다.
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`):
  - 저장소 루트에서 `external/linux/scripts/checkpatch.pl --no-tree -f examples/mic_codec/inmp441_codec.c` 실행, 종료 코드 0.
  - `total: 0 errors, 0 warnings, 61 lines checked`.
- 판단이 필요했던 부분:
  - 대상 커널 `external/linux/sound/soc/codecs/ics43432.c`의 DAI·component·platform 등록 구조를 참고했다. component 세 필드는 `external/linux/include/sound/soc-component.h`의 정의와 아래 사용처를 확인해 모두 생략했다(기본값 0).
  - `idle_bias_on`: `sound/soc/soc-dapm.c`의 `snd_soc_dapm_init()`이 DAPM의 `idle_bias`로 전달하고 유휴 bias 수준 결정에 사용한다. 이 드라이버는 bias 콜백과 DAPM 위젯·전원 제어가 없어 유휴 bias 유지 설정이 필요하지 않다.
  - `use_pmdown_time`: `sound/soc/soc-pcm.c`의 `snd_soc_runtime_ignore_pmdown_time()`이 확인하며, `sound/soc/soc-dapm.c`의 `snd_soc_dapm_stream_stop()`에서 재생 스트림의 지연 정지에 사용한다. 캡처 스트림은 즉시 정지하므로 캡처 전용 마이크에는 필요하지 않다.
  - `endianness`: `sound/soc/soc-core.c`의 `snd_soc_add_component()`가 이 값이 켜져 있으면 `convert_endianness_formats()`로 LE·BE 형식을 모두 추가한다. 요구한 S24_LE·S32_LE만 광고하도록 생략했다.
  - remove 함수: `sound/soc/soc-devres.c`의 `devm_snd_soc_register_component()`가 해제 콜백 `devm_component_release()`를 등록하고, 해제 시 `snd_soc_unregister_component_by_driver()`를 호출한다. 별도로 해제할 자원이 없어 remove를 두지 않았다.
  - 지시서 변경 파일 목록 밖의 구현 파일 수정은 필요하지 않았다. 기존 문서 변경분과 작업 노트는 수정하지 않았고, 이 지시서에서는 구현 보고 섹션만 작성했다.

## 리뷰 (Claude)
- 결과: 통과
- 범위: 새 파일 3개(`examples/mic_codec/`)와 `.gitignore` 한 줄. `examples/mic/` 기존 파일과 문서는 바뀌지 않음(`git status`). overlay는 `diff` 결과 코덱 `compatible`과 카드 이름 두 줄만 다름
- 드라이버: `.capture` 값(1~2채널, 7800~50000Hz 연속, S24_LE·S32_LE)이 지시와 같음(`inmp441_codec.c` L11~22). probe는 등록 한 번과 `dev_err_probe` 반환이라 probe 지연(`-EPROBE_DEFER`)도 조용히 전달됨(L31~36). 해제할 자원이 없어 remove가 없는 판단이 맞음(devm)
- component 필드 생략 판단을 소스로 확인:
  - `endianness`: `snd_soc_add_component()`가 켜져 있으면 capture 형식에 BE 형식까지 추가한다(`soc-core.c` L2889~2893). 생략하면 S24_LE·S32_LE만 알리므로 지시와 맞음
  - `use_pmdown_time`: `snd_soc_dapm_stream_stop()`에서 재생 스트림에만 쓰이고 캡처는 바로 정지한다(`soc-dapm.c` L4579~4595). 캡처 전용 코덱에 필요 없음
  - `idle_bias_on`: bias 콜백·DAPM 위젯이 없어 영향 없음
- 스타일: checkpatch 0 errors 0 warnings, 커널 스타일·한글 주석 규칙 준수. `MODULE_LICENSE("GPL")`은 SPDX `GPL-2.0-only`와 함께 쓰는 커널 관례(`license-rules.rst` L441)
- 참고(수정 요구 아님): `dev_info` 문자열의 `7800-50000`은 매크로와 별도로 적힌 숫자라, 매크로를 바꾸면 함께 고쳐야 함
- Makefile: `mpu6050_iio` 형식과 같고 deploy 대상이 지시와 같음. 빌드·`modinfo`·역변환만 실행, 보드 조작 없음
- 수정 지시: 없음
