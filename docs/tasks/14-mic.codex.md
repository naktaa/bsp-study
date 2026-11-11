# 14 I2S 마이크 INMP441 — Codex 지시서

작업 노트: [14-mic.md](14-mic.md)

## 지시 (Claude)
- 변경 파일:
  - 새 파일 `examples/mic/inmp441-overlay.dts`
  - 새 파일 `examples/mic/Makefile`
  - `.gitignore`: `/examples/mic/*.dtbo` 한 줄 추가
- 요구사항:
  - overlay는 대상 커널의 `arch/arm/boot/dts/overlays/adau7002-simple-overlay.dts` 구조를 따른다. 차이는 코덱 노드뿐이다.
    - fragment@0: `target = <&i2s_clk_producer>`, `status = "okay"`
    - fragment@1: `target-path = "/"`에 코덱 노드 `inmp441_codec: inmp441-codec`, `#sound-dai-cells = <0>`, `compatible = "invensense,ics43432"`, `status = "okay"`
    - fragment@2: `target = <&sound>`에 `compatible = "simple-audio-card"`, `simple-audio-card,format = "i2s"`, `simple-audio-card,name = "inmp441"`, `bitclock-slave`·`frame-slave`는 코덱 dailink(Pi가 클럭 마스터), cpu `sound-dai = <&i2s_clk_producer>`, codec `sound-dai = <&inmp441_codec>`
    - `__overrides__`에 `card-name` (adau7002-simple과 같은 방식)
    - `compatible = "brcm,bcm2835"` 최상위(기존 rpi overlay와 같게)
    - 주석은 한글로, 클럭 마스터 방향처럼 코드만으로 알기 어려운 것에만
  - Makefile은 `examples/mpu6050_iio/Makefile`의 overlay·deploy 부분과 같은 형식으로 쓴다.
    - `KDIR ?= $(abspath $(CURDIR)/../../out/kernel-rpi4)`, `DTC ?= $(KDIR)/scripts/dtc/dtc`
    - `all`/`overlay`: `inmp441.dtbo`를 `"$(DTC)" -@ -I dts -O dtb`로 생성(출력 이름은 `config.txt`의 `dtoverlay=inmp441`에 맞춰 `inmp441.dtbo`)
    - `deploy`: `scp inmp441.dtbo pi:bsp/overlays/`
    - `clean`, `.PHONY`
- 하지 말 것: 코덱·machine 드라이버 작성, 커널 config fragment 변경, `adau7002-simple` 원본 수정, 핀(GPIO18~21) 재배치 파라미터 추가, 재생(DAC) 설정, 보드 조작
- 빌드 확인 명령:

```bash
# [WSL] ~/bspworkspace/bsp-study/examples/mic
make
file inmp441.dtbo
"$HOME/bspworkspace/bsp-study/out/kernel-rpi4/scripts/dtc/dtc" -I dtb -O dts inmp441.dtbo 2>/dev/null | grep -E 'compatible|sound-dai|format|name|__fixups__|i2s_clk_producer|sound ='
git check-ignore -v inmp441.dtbo
```

`__fixups__`에 `i2s_clk_producer`와 `sound`가 있어야 한다. dtc 경고가 있으면 구현 보고에 그대로 적는다.

## 구현 보고 (Codex)
- 변경 요약:
  - `examples/mic/inmp441-overlay.dts`: `i2s_clk_producer` 활성화, `invensense,ics43432` 코덱과 CPU DAI의 simple-audio-card 연결, 카드 이름 `inmp441`, `card-name` override 작성.
  - `examples/mic/Makefile`: `all`·`overlay`로 `inmp441.dtbo` 생성, `deploy`·`clean` 타깃 작성. `.gitignore`에 `/examples/mic/*.dtbo` 추가.
- 빌드 결과:
  - `examples/mic`에서 `make`: 종료 코드 0. 대상 커널의 `out/kernel-rpi4/scripts/dtc/dtc`로 `inmp441.dtbo` 생성. dtc 경고 없음.
  - `file inmp441.dtbo`: Device Tree Blob version 17, 1471바이트.
  - `"$HOME/bspworkspace/bsp-study/out/kernel-rpi4/scripts/dtc/dtc" -I dtb -O dts inmp441.dtbo`: 종료 코드 0, 경고 없음. stderr를 숨기거나 출력을 필터링하지 않고 전체 역변환 결과를 확인함. `compatible`, `i2s` 형식, `inmp441` 이름, CPU·코덱 `sound-dai`, `card-name` override 확인.
  - `__fixups__`: `i2s_clk_producer`가 fragment@0의 target과 CPU의 sound-dai를, `sound`가 fragment@2의 target을 참조함. 코덱 DAI와 slave 속성의 내부 phandle 연결도 확인.
  - `git check-ignore -v inmp441.dtbo`: `.gitignore:5:/examples/mic/*.dtbo` 규칙 적용 확인.
  - `make -n deploy`: `scp inmp441.dtbo pi:bsp/overlays/` 출력 확인. 실제 deploy는 실행하지 않음.
  - `git diff --check`: 통과. 신규 DTS·Makefile은 내용 직접 확인.
  - 미검증: 보드 배포·부팅, ALSA 카드 등록, 코덱 모듈 적재, 실제 클럭·녹음. 전체 커널 빌드와 보드 조작은 실행하지 않음.
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`): 해당 없음(C 소스 없음)
- 판단이 필요했던 부분:
  - 사전 계획에서 설명하고 사용자 승인받은 대로 원본 overlay의 `simple-audio-card,widgets`와 `simple-audio-card,routing`은 제외함. `PDM_DAT`는 `external/linux/sound/soc/codecs/adau7002.c`의 DAPM 입력이고 `ics43432.c`에는 해당 위젯·경로가 없음.
  - `bitclock-slave`·`frame-slave`는 지시서와 원본 형식을 유지함. 다만 대상 커널의 `simple_util_parse_daifmt()`와 `snd_soc_daifmt_parse_clock_provider_raw()`는 `*-master` 속성을 읽음. 현재 구조에서는 master 속성이 없어 코덱 노드의 legacy 파싱으로 넘어가며, 여기에도 master 속성이 없어 `SND_SOC_DAIFMT_CBC_CFC`(코덱이 두 클럭을 받음)로 결정되는 것을 소스로 확인함. slave 속성 자체가 클럭 방향을 결정하는 것은 아니며, 실측은 미검증임.

## 리뷰 (Claude)
- 결과: 통과
- 범위: 변경은 `examples/mic/inmp441-overlay.dts`, `examples/mic/Makefile`, `.gitignore` 한 줄뿐(`git status`). dtbo는 무시 규칙 적용
- overlay: 구조가 `adau7002-simple`과 같고 코덱만 `invensense,ics43432`(`sound/soc/codecs/ics43432.c` L55). 역변환 결과 `__fixups__`에 `i2s_clk_producer`(fragment@0 target, cpu sound-dai)와 `sound`(fragment@2 target)가 있음. DAPM 위젯·routing을 뺀 판단은 맞음(`PDM_DAT`는 adau7002 전용)
- 클럭 방향: 구현 보고의 지적이 맞다. `sound/soc/` 전체에 `bitclock-slave`·`frame-slave`를 읽는 코드가 없고, `snd_soc_daifmt_parse_clock_provider_raw()`(`soc-core.c` L3433)는 `*-master`만 본다. master 속성이 없으므로 코덱이 두 클럭을 받는 쪽(CBC_CFC), 곧 Pi가 BCLK·WS를 공급하는 결과는 같다. slave 두 줄과 그 위 주석은 동작에 영향이 없고 원본 rpi overlay 형식을 따른 것으로 둔다(학습 문서에 설명)
- Makefile: `mpu6050_iio` 형식과 같음. 빌드·`make -n deploy`만 실행하고 보드 조작 없음
- 수정 지시: 없음
