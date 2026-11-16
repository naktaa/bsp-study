# 13-3 최소 IMX219 센서 드라이버 — Codex 지시서

작업 노트: [13-3-camera-sensor.md](13-3-camera-sensor.md)

## 지시 (Claude)
- 변경 파일(모두 새 파일): `examples/camera_sensor/imx219_study.c`, `examples/camera_sensor/imx219-study-overlay.dts`, `examples/camera_sensor/Makefile`
- 참고(읽기 전용): `external/linux/drivers/media/i2c/imx219.c`(6.18 mainline), `external/linux/arch/arm/boot/dts/overlays/imx219-overlay.dts`·`imx219.dtsi`, `external/linux/drivers/media/platform/bcm2835/bcm2835-unicam.c`, 기존 Makefile 형식 `examples/mpu6050_iio/Makefile`·`examples/ultrasonic/Makefile`
- 요구사항:
  1. **드라이버 식별:** i2c 드라이버 이름 `imx219-study`, `of_match` compatible `nakta,imx219-study`, `MODULE_DEVICE_TABLE(of)`. 모듈 이름은 `imx219_study`.
  2. **probe 순서:**
     - endpoint 파싱(`v4l2_fwnode_endpoint_alloc_parse`, CSI2 DPHY)으로 `data-lanes` 2개와 `link-frequencies`에 456000000이 있는지 확인한다. 아니면 `-EINVAL`.
     - `devm_cci_regmap_init_i2c(client, 16)`, `devm_v4l2_sensor_clk_get(dev, NULL)` 후 rate가 24000000인지 확인한다.
     - `devm_regulator_bulk_get` 3개(`VANA`·`VDIG`·`VDDL`).
     - 전원 on → chip ID(`CCI_REG16(0x0000)`) == `0x0219` 확인 후 `dev_info`로 한 줄 출력 → 전원 off.
     - 컨트롤 초기화 → `V4L2_SUBDEV_FL_HAS_DEVNODE`, `MEDIA_ENT_F_CAM_SENSOR`, source pad 1개 `media_entity_pads_init` → `v4l2_async_register_subdev_sensor`.
     - 오류 경로는 역순으로 해제한다(컨트롤 handler free, entity cleanup). chip ID 실패 때도 전원을 끈다.
  3. **전원:**
     - on: `regulator_bulk_enable` → `clk_prepare_enable` → `usleep_range(6200, 7200)`. 실패 시 켠 것만 되돌린다.
     - off: `clk_disable_unprepare` → `regulator_bulk_disable`.
     - reset GPIO, runtime PM, `dev_pm_ops`는 쓰지 않는다.
  4. **고정 모드:** 3280×2464, `MEDIA_BUS_FMT_SRGGB10_1X10`, `V4L2_FIELD_NONE`, `V4L2_COLORSPACE_RAW`, 나머지 필드는 mainline `imx219_update_pad_format()`(L800)과 같은 값.
     - 레지스터 고정 테이블은 `cci_reg_sequence` 배열 하나로 둔다. 배열 위 주석에 출처(6.18 `imx219.c`의 `imx219_common_regs`·`imx219_2lane_regs`·`imx219_set_framefmt()` 계산값)를 적는다.

     | 레지스터 | 값 | 출처 |
     | --- | --- | --- |
     | `CCI_REG8(0x0128)` DPHY_CTRL | 0 | common |
     | `CCI_REG16(0x012a)` EXCK_FREQ | 24 × 256 | common |
     | `CCI_REG8(0x0301)` `0x0303` `0x0304` `0x0305` | 5, 1, 3, 3 | 2lane |
     | `CCI_REG16(0x0306)` PLL_VT_MPY | 57 | 2lane |
     | `CCI_REG8(0x030b)` / `CCI_REG16(0x030c)` | 1 / 114 | 2lane |
     | `CCI_REG8(0x0114)` CSI_LANE_MODE | 1 | 2lane |
     | `CCI_REG16(0x0164)` `0x0166` `0x0168` `0x016a` | 0, 3279, 0, 2463 | set_framefmt |
     | `CCI_REG8(0x0174)` `0x0175` binning | 0, 0 | set_framefmt |
     | `CCI_REG16(0x016c)` `0x016e` 출력 크기 | 3280, 2464 | set_framefmt |
     | `CCI_REG16(0x0624)` `0x0626` TP 창 | 3280, 2464 | set_framefmt |
     | `CCI_REG16(0x018c)` CSI_DATA_FORMAT | `0x0a0a` | set_framefmt |
     | `CCI_REG8(0x0309)` OPPXCK_DIV | 10 | set_framefmt |
     | `CCI_REG16(0x0162)` LINE_LENGTH | 3448 | `IMX219_LLP_MIN` (hblank 168) |

  5. **pad ops:** `enum_mbus_code`(index 0만), `enum_frame_size`(index 0, 위 코드만), `get_fmt`, `set_fmt`(요청을 무시하고 고정 포맷을 `fmt->format`에 채워 반환). 포맷은 state에 저장하지 않는다. `init_state`, `v4l2_subdev_init_finalize`, `enable_streams`/`disable_streams`, `get_selection`은 쓰지 않는다.
  6. **컨트롤:** handler 하나로 만든다.
     - `V4L2_CID_EXPOSURE`: 최소 4, 최대 `3526 − 4`, 기본 1600
     - `V4L2_CID_ANALOGUE_GAIN`: 0~232, 기본 0
     - `V4L2_CID_VBLANK`: 32 ~ `0xfffe − 2464`, 기본 `3526 − 2464`
     - `V4L2_CID_HBLANK`: 168 고정, `READ_ONLY`
     - `V4L2_CID_PIXEL_RATE`: 182400000 고정(PIXEL_RATE는 기본이 읽기 전용)
     - `V4L2_CID_LINK_FREQ`: int menu {456000000}, `READ_ONLY`
     - `V4L2_CID_TEST_PATTERN`: 메뉴 {"Disabled", "Color Bars"} → 레지스터 `CCI_REG16(0x0600)` 값 {0, 2}
     - VBLANK가 바뀌면 같은 `s_ctrl` 안에서 `__v4l2_ctrl_modify_range`로 exposure 최대를 `2464 + vblank − 4`로 갱신한다(mainline L463과 같은 계산).
     - 레지스터 매핑: exposure → `0x015a`(16), gain → `0x0157`(8), vblank → `0x0160`(16)에 `2464 + val`, test_pattern → `0x0600`(16). hblank·pixel_rate·link_freq는 쓰지 않는다.
  7. **잠금:** 드라이버 lock은 컨트롤 handler lock(`ctrl_handler.lock`) 하나뿐이다. `bool streaming` 플래그를 둔다.
     - `.s_stream(1)`: handler lock 안에서 전원 on → 고정 테이블 쓰기(`cci_multi_reg_write`) → `__v4l2_ctrl_handler_setup` → `MODE_SELECT`(`CCI_REG8(0x0100)`) = 1 → `streaming = true` 순서로 한다. 중간 실패 시 전원 off 후 오류를 반환한다.
     - `.s_stream(0)`: 같은 lock 안에서 `MODE_SELECT` = 0 → 전원 off → `streaming = false`.
     - `s_ctrl`: 코어가 같은 handler lock을 잡고 호출한다. 범위 갱신 후 `streaming`이 false면 레지스터를 쓰지 않고 0을 반환한다. true일 때만 `cci_write`한다.
     - 추가 mutex·spinlock·atomic은 넣지 않는다.
  8. **remove:** `v4l2_async_unregister_subdev` → `media_entity_cleanup` → `v4l2_ctrl_handler_free`. 스트리밍 중이면 전원을 끈다(정상 경로에서는 unicam이 먼저 내려가 발생하지 않음).
  9. **LP-11 토글:** probe에서 mainline L1236의 streaming → standby 토글은 넣지 않는다. 보드 A2에서 첫 스트림이 실패하면 리뷰에서 추가를 지시한다.
  10. **overlay** `imx219-study-overlay.dts`:
      - stock `imx219-overlay.dts`의 fragment@0(`i2c0if`), @1(`cam1_clk`, 24MHz), @2(`i2c0mux`), @100(`i2c_csi_dsi`), @101(`csi1` endpoint)만 남긴다.
      - @100 안에 `imx219.dtsi` 노드를 `#include` 없이 직접 펼친다(`imx219@10`, `reg = <0x10>`, `clocks`, `clock-names = "xclk"`, supply 3개, `rotation`·`orientation`, endpoint의 `clock-lanes`·`data-lanes = <1 2>`·`clock-noncontinuous`·`link-frequencies = /bits/ 64 <456000000>`). compatible만 `nakta,imx219-study`로 바꾸고 `status = "okay"`로 둔다.
      - vcm 노드, @102·@201·@202 dormant fragment, `__overrides__`는 넣지 않는다. GPIO 상수를 쓰지 않으므로 cpp 전처리 없이 dtc로 빌드되게 한다.
      - 노드 이름은 `imx219@10`를 유지해 엔티티 이름이 드라이버 이름 기준(`imx219-study 10-0010`)이 되게 한다.
  11. **Makefile:** `examples/mpu6050_iio/Makefile` 형식을 따른다. `obj-m := imx219_study.o`, `modules`(기본), `overlay`(`imx219-study.dtbo`, ultrasonic과 같은 dtc 규칙), `deploy`(`.ko` → `pi:bsp/modules/`, dtbo → `pi:bsp/overlays/`), `clean`.
  12. 커널 스타일, 한글 주석은 의도·제약에만 쓴다(출처 주석, 잠금 순서, LP-11 미적용 이유 정도).
- 하지 말 것:
  - 여러 모드·binning·flip·디지털 게인·embedded data·`get_selection`·fwnode 방향/회전 컨트롤·runtime PM·reset GPIO
  - 상태 API(`init_state`·`init_finalize`·`enable_streams`)
  - 위 목록 밖의 컨트롤, 추가 잠금, 모듈 파라미터
  - `examples/camera/` 도구 수정, `.gitignore`·문서 수정(구현 보고 제외), Yocto 레시피
  - 보드 조작
- 빌드 확인 명령:

```bash
# [WSL] ~/bspworkspace/bsp-study
make -C examples/camera_sensor W=1
make -C examples/camera_sensor overlay
modinfo examples/camera_sensor/imx219_study.ko | grep -E '^(alias|depends|name)'
fdtdump examples/camera_sensor/imx219-study.dtbo 2>/dev/null | grep -E 'compatible|link-frequencies|data-lanes' | head
external/linux/scripts/checkpatch.pl --no-tree -f examples/camera_sensor/imx219_study.c
```

## 구현 보고 (Codex)
- 변경 요약:
  - `examples/camera_sensor/imx219_study.c`: 단일 3280×2464 RAW10 센서 드라이버를 구현했다. endpoint의 2레인·456MHz, xclk 24MHz, 전원 3개와 chip ID를 확인하고, source pad 1개·고정 포맷·지정한 컨트롤 7개를 async 등록한다.
  - ctrl handler lock 하나로 `powered`·`streaming`을 보호한다. 시작은 전원 on → `powered=true` → 고정 테이블 → `__v4l2_ctrl_handler_setup()` → MODE_SELECT=1 → `streaming=true` 순서다. `s_ctrl`은 코어가 잡은 같은 lock 안에서 exposure 범위를 갱신하고 `powered`일 때만 레지스터를 쓴다. 시작 실패·중지 시 전원과 두 플래그를 정리하며, remove는 async 등록 해제 후 같은 중지 경로를 사용한다.
  - `examples/camera_sensor/imx219-study-overlay.dts`: fragment@0·@1·@2·@100·@101만 두고 센서 노드와 양방향 endpoint를 펼쳤다. compatible은 `nakta,imx219-study`이며 기존 클럭·supply·rotation·orientation과 2레인·456MHz를 유지한다.
  - `examples/camera_sensor/Makefile`: 외부 모듈 기본 타깃, dtc overlay 빌드, modules·overlay를 선행하는 deploy, clean을 추가했다. deploy 타깃은 작성만 했으며 실행하지 않았다.
  - 소스 변경은 위 세 파일로 한정했다. 문서는 이 구현 보고만 기록했으며 작업 노트·지시·리뷰는 수정하지 않았다.
- 빌드 결과:
  - `make -C examples/camera_sensor W=1`: 종료 0, 최종 소스로 ARM64 외부 모듈 빌드 성공, 컴파일·MODPOST·링크 경고 없음.
  - `make -C examples/camera_sensor overlay`: 종료 0, 대상 커널의 `out/kernel-rpi4/scripts/dtc/dtc -@`로 `imx219-study.dtbo` 생성, 경고 없음.
  - `modinfo examples/camera_sensor/imx219_study.ko`: 이름 `imx219_study`, alias `of:N*T*Cnakta,imx219-study`와 `C*` 확인. 의존 모듈은 `v4l2-fwnode,v4l2-cci,videodev,mc,v4l2-async`, vermagic은 `6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64`다.
  - `file examples/camera_sensor/imx219_study.ko`: `ELF 64-bit LSB relocatable, ARM aarch64` 확인.
  - `fdtdump examples/camera_sensor/imx219-study.dtbo`: compatible, 24MHz, 2레인, 64bit link-frequencies 456000000, 양방향 endpoint phandle과 외부 심볼 fixup을 확인했다. 실제 보드 DT에 적용하거나 부팅한 결과는 아니다.
  - 일회성 Python 정적 비교: 원본 `imx219_common_regs` 23개 + `imx219_2lane_regs` 8개의 값·폭·순서가 일치했다. 고정 모드 계산값·라인 길이 13개를 포함해 총 44개 레지스터 항목을 확인했다. 검증용 파일은 저장소에 추가하지 않았다.
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`):
  - `external/linux/scripts/checkpatch.pl --no-tree -f examples/camera_sensor/imx219_study.c`: 종료 0, `total: 0 errors, 0 warnings, 456 lines checked`.
- 판단이 필요했던 부분:
  - 구현 계획 승인에 따라 지시서의 두 부분을 보완했다. 실제 6.18.59 소스에는 지시서 표에 없는 접근 해제 시퀀스·undocumented 레지스터·X/Y 증가값이 있으므로 `imx219_common_regs` 전체를 순서대로 포함했다. 출처는 배열 위 한글 주석에 명시했다.
  - `__v4l2_ctrl_handler_setup()`이 `s_ctrl`을 호출할 때는 아직 `streaming=false`다. 앞서 승인한 `powered` 플래그로 레지스터 접근을 판단해 시작 전 저장한 컨트롤이 생략되지 않게 했다. 추가 잠금은 없다.
  - probe의 LP-11 진입 토글은 지시대로 생략했다. runtime PM·reset GPIO·상태 API·추가 모드·추가 컨트롤은 넣지 않았다.
  - `.dtbo`는 현재 `.gitignore`에 포함되지 않아 빌드 후 untracked 산출물로 남아 있다. 변경 범위에 없는 `.gitignore`는 수정하지 않았다.
  - 미검증: 보드 probe·chip ID·media 링크·Used by·단독 rmmod 실패·프레임 수신·노출·컬러바·반복 해제·v4l2-compliance·원복. 전체 커널·Yocto 빌드, 보드 조작, 커밋은 수행하지 않았다.

## 리뷰 (Claude)
- 결과: 통과
- 범위: 새 파일 3개(`imx219_study.c`·`imx219-study-overlay.dts`·`Makefile`)뿐이다. 문서는 구현 보고만 고쳤다. 빌드 산출물 `.dtbo`는 머지 후 main `.gitignore`에 `/examples/camera_sensor/*.dtbo`로 추가한다(`.ko`·`.o` 등은 기존 패턴으로 무시됨).
- 레지스터: `imx219_regs`(L41~85)를 6.18 `imx219.c` L160~205와 대조했다. `imx219_common_regs` 23개의 값·폭·순서, 2레인 PLL 8개, `set_framefmt` 계산값 12개, LINE_LENGTH 3448이 맞다. 지시서 표에는 common 2개만 있어 누락됐었고, Codex가 전체를 넣은 판단이 맞다. 노트 '결정'의 개수도 고쳤다.
- 잠금: `s_stream`(L243)은 `ctrl_handler.lock` 안에서 전원 on → `powered` → 테이블 → `__v4l2_ctrl_handler_setup` → MODE_SELECT 1 → `streaming` 순서다. `s_ctrl`(L157)은 코어가 같은 lock을 잡고 부르며 `powered`일 때만 쓴다. 지시서는 `streaming`을 검사하라고 했지만, 그러면 setup 시점(`streaming`=false)의 값이 센서에 들어가지 않는다. `powered`로 바꾼 것이 맞다. 추가 잠금은 없다. 모두 프로세스 문맥이고 sleep(I2C·`usleep_range`)이 가능하다.
- 컨트롤: HBLANK·PIXEL_RATE·LINK_FREQ는 ops가 NULL이고 READ_ONLY다. `__v4l2_ctrl_handler_setup`은 READ_ONLY를 건너뛴다(`v4l2-ctrls-core.c` L2659). NULL ops도 `call_op`가 처리하므로 문제없다. VBLANK에서 exposure 최대를 갱신하는 식(L164)은 mainline L463과 같다.
- 오류·해제:
  - probe는 컨트롤 → entity → async 순서로 등록한다. 실패 경로(L420~423)와 remove(L433~436: async 해제 → 스트림 정지 → entity cleanup → handler free)가 그 역순이다.
  - chip ID를 읽은 직후 성공·실패와 관계없이 전원을 끈다(L394).
  - `s_stream` 실패 시 전원과 두 플래그를 정리한다(L279~282). 정지는 `streaming` 상태에서만 레지스터에 쓴다(L249).
- unicam 호환: `get_fmt`가 state를 쓰지 않고 고정 포맷을 돌려주므로 unicam의 자체 state 호출(L644)과 link_validate의 NULL 호출(L2032)에 모두 응답한다. `set_fmt`도 같은 함수라 `media-ctl -V`는 고정값으로 응답한다. source pad 1개라 metadata 노드는 생기지 않는다.
- overlay: stock `imx219-overlay.dts`의 fragment 0·1·2·100·101과 `imx219.dtsi` 노드와 같다. compatible만 `nakta,imx219-study`로 바뀌었고 24MHz, 2레인, 456MHz, supply 3개가 유지된다. `camera_auto_detect=0`과 함께 쓰면 `sony,imx219` 노드가 없다. 백업·복구(노트 0단계)를 깨지 않는다.
- 스타일: checkpatch 0/0, `W=1` 경고 0. 처리 단위 사이 빈 줄이 빠진 곳(L364~365, L401~403)은 가독성 차이라 보드 확인 후 정리해도 된다.
- 보드 확인 전 미검증: 노트 A1~B3 전부. LP-11 토글 생략(L402)은 A2 결과로 판단한다.
- 수정 지시: 없음
