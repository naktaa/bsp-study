# 13-4 직접 작성 센서 드라이버의 libcamera 연동 — Codex 지시서

작업 노트: [13-4-camera-libcamera.md](13-4-camera-libcamera.md)

## 지시 (Claude)
- 변경 파일: `examples/camera_sensor/imx219_study.c`만 고친다(overlay·Makefile 변경 없음).
- 참고(읽기 전용): `external/linux/drivers/media/i2c/imx219.c`(flip·Bayer 코드·selection·fwnode 컨트롤 처리), `external/linux/drivers/media/v4l2-core/v4l2-i2c.c` L32 `v4l2_i2c_subdev_set_name`
- 요구사항:
  1. **엔티티 이름:** `v4l2_i2c_subdev_init()` 바로 뒤, `v4l2_async_register_subdev_sensor()` 전에 `v4l2_i2c_subdev_set_name(&sensor->sd, client, "imx219", NULL)`을 호출해 엔티티 이름을 `imx219 10-0010`으로 한다. i2c 드라이버 이름 `imx219-study`와 compatible `nakta,imx219-study`는 그대로 둔다. 이유(libcamera가 엔티티 이름으로 센서 모델을 정함)를 한 줄 주석으로 남긴다.
  2. **rotation·orientation:** 컨트롤 초기화에서 `v4l2_fwnode_device_parse(dev, &props)` → `v4l2_ctrl_new_fwnode_properties(hdl, &imx219_ctrl_ops, &props)`. DT의 `rotation`·`orientation`이 읽기 전용 컨트롤로 노출되어야 한다. 실패 경로는 기존 `free_controls`로 간다.
  3. **flip 컨트롤:**
     - `V4L2_CID_HFLIP`, `V4L2_CID_VFLIP`(0~1, 기본 0)을 만들고 둘 다 `V4L2_CTRL_FLAG_MODIFY_LAYOUT`을 켠다. 포인터를 구조체에 둔다.
     - `s_ctrl`에서 둘 중 하나가 바뀌면 `CCI_REG8(0x0172)`에 `hflip->val | vflip->val << 1`을 쓴다. 쓰기는 기존 `powered` 규칙(전원이 꺼져 있으면 값만 저장)을 따른다.
     - `v4l2_ctrl_handler_init` 크기 힌트를 늘어난 컨트롤 수에 맞춘다.
  4. **스트리밍 중 flip 변경 금지:** `s_stream(1)`에서 `streaming = true` 직후 `__v4l2_ctrl_grab(hflip, true)`·`__v4l2_ctrl_grab(vflip, true)`를 한다. `s_stream(0)`과 시작 실패 정리 경로에서는 `false`로 푼다. 모두 기존 ctrl handler lock 안에서 한다.
  5. **Bayer 코드:**
     - 표 `{ MEDIA_BUS_FMT_SRGGB10_1X10, MEDIA_BUS_FMT_SGRBG10_1X10, MEDIA_BUS_FMT_SGBRG10_1X10, MEDIA_BUS_FMT_SBGGR10_1X10 }`을 인덱스 `hflip->val | vflip->val << 1`로 고르는 함수 하나를 둔다(mainline `imx219_mbus_formats` 순서와 같음). 함수 안에 `lockdep_assert_held(sensor->ctrl_handler.lock)`.
     - `enum_mbus_code`(index 0만), `enum_frame_size`(요청 코드가 현재 코드일 때만), `get_fmt`, `set_fmt`는 ctrl handler lock을 잡고 이 함수로 현재 코드를 얻는다. lock은 코드를 읽는 동안만 잡는다.
     - `set_fmt`는 요청에 다른 코드나 크기가 와도 현재 flip에 맞는 코드와 고정 크기를 돌려준다(`-EINVAL`을 내지 않음).
  6. **`get_selection`:** pad 0만 받는다.
     - `V4L2_SEL_TGT_CROP`·`V4L2_SEL_TGT_CROP_DEFAULT`·`V4L2_SEL_TGT_CROP_BOUNDS` → `{ left 8, top 8, width 3280, height 2464 }`
     - `V4L2_SEL_TGT_NATIVE_SIZE` → `{ 0, 0, 3296, 2480 }`
     - 그 밖은 `-EINVAL`
     - 값은 mainline `IMX219_PIXEL_ARRAY_*`·`IMX219_NATIVE_*`와 같다.
  7. 13-3의 잠금 구조(ctrl handler lock 하나, `powered`·`streaming` 플래그), 레지스터 테이블, 전원 순서, probe·remove 순서는 바꾸지 않는다.
- 하지 말 것:
  - binning·추가 모드, 디지털 게인, 컨트롤 이벤트, 상태 API, runtime PM, embedded data
  - flip 기본값을 1로 두는 등 방향 하드코딩
  - overlay·Makefile·`examples/camera` 도구·문서 수정(구현 보고 제외), 보드 조작
- 빌드 확인 명령:

```bash
# [WSL] ~/bspworkspace/bsp-study
make -C examples/camera_sensor W=1
external/linux/scripts/checkpatch.pl --no-tree -f examples/camera_sensor/imx219_study.c
grep -n 'v4l2_i2c_subdev_set_name\|MODIFY_LAYOUT\|__v4l2_ctrl_grab\|lockdep_assert_held\|get_selection\|new_fwnode_properties' examples/camera_sensor/imx219_study.c
```

## 구현 보고 (Codex)
- 변경 요약:
  - 소스는 `examples/camera_sensor/imx219_study.c`만 수정했다. subdev 초기화 직후 `v4l2_i2c_subdev_set_name(..., "imx219", NULL)`을 호출하고 libcamera 모델 식별 이유를 한글 주석으로 남겼다. 드라이버 이름·compatible은 유지했다.
  - 컨트롤 수 힌트를 11로 늘리고 기본값 0의 HFLIP·VFLIP과 `MODIFY_LAYOUT`을 추가했다. DT rotation·orientation은 `v4l2_fwnode_device_parse()` → `v4l2_ctrl_new_fwnode_properties()`로 노출한다. 초기화 오류는 probe의 기존 `free_controls` 경로로 전달한다.
  - `s_ctrl`의 기존 `powered` 검사 뒤에서 `0x0172`에 두 flip의 조합을 쓴다. 스트림 시작 성공 시 flip을 grab하고, 중지·시작 실패에서는 해제한다. 전원 on 실패도 전원 off 없이 grab 해제 경로로 간다. 모두 기존 handler lock 안에서 처리한다.
  - `imx219_get_format_code()`에 mainline 순서의 Bayer 코드 4개와 `lockdep_assert_held()`를 두었다. 포맷 조회·코드 및 크기 열거는 코드 조회 구간에서만 handler lock을 잡는다. `set_fmt`는 기존처럼 `get_fmt`를 공유해 요청 코드·크기 대신 현재 flip 코드와 3280×2464를 반환한다.
  - pad 0의 `get_selection`을 추가했다. crop·default·bounds는 `(8, 8, 3280, 2464)`, native size는 `(0, 0, 3296, 2480)`이며 다른 pad·target은 `-EINVAL`이다.
- 빌드 결과:
  - `make -C examples/camera_sensor W=1`: 종료 0. ARM64 외부 모듈의 컴파일·MODPOST·링크 성공, 경고 없음.
  - `modinfo examples/camera_sensor/imx219_study.ko`: 이름 `imx219_study`, 기존 `nakta,imx219-study` OF alias, vermagic `6.18.59-v8-bsp-study01 SMP preempt mod_unload modversions aarch64` 확인.
  - 요구 API 정적 확인: `rg -n 'v4l2_i2c_subdev_set_name|MODIFY_LAYOUT|__v4l2_ctrl_grab|lockdep_assert_held|get_selection|new_fwnode_properties' examples/camera_sensor/imx219_study.c`로 호출·등록 위치를 확인했다. `git diff --check`도 종료 0이다.
  - 변경 전후 SHA-256 비교로 레지스터 테이블, endpoint 검사, 전원 on/off, remove 함수, overlay·Makefile, 작업 노트, 이 지시서의 지시·리뷰 섹션이 유지됨을 확인했다. probe에는 엔티티 이름 설정만 추가했다.
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`):
  - `external/linux/scripts/checkpatch.pl --no-tree -f examples/camera_sensor/imx219_study.c`: 종료 0, `total: 0 errors, 0 warnings, 557 lines checked`.
- 판단이 필요했던 부분:
  - rotation·orientation은 대상 커널의 `v4l2-ctrls-defs.c`에서 기본 `READ_ONLY`로 지정하므로 별도 플래그 변경이나 `s_ctrl` 레지스터 처리를 추가하지 않았다. 초기 적용에서도 코어가 읽기 전용 컨트롤을 건너뛴다.
  - Bayer 표와 flip 레지스터 비트 조합, selection 영역 값은 대상 `external/linux/drivers/media/i2c/imx219.c`와 정적으로 대조했다. 코드 조회에는 ctrl handler lock 하나만 사용하며 상태 API·추가 잠금은 넣지 않았다.
  - 미검증: 실제 엔티티 이름, 보드 컨트롤·flip 변경 거부, libcamera 인식, 사진의 색·방향, 자동 노출, 프레임 간격·송출·CPU 부하·원복. 보드 조작·배포·전체 커널 및 Yocto 빌드·커밋은 수행하지 않았다. 문서는 이 구현 보고만 기록했다.

## 리뷰 (Claude)
- 결과: 통과
- 범위: `imx219_study.c` 한 파일만 바뀌었다(+106/−5). 레지스터 표·전원·endpoint 검사·remove는 그대로다.
- 엔티티 이름: L465에서 `v4l2_i2c_subdev_init` 바로 뒤, async 등록 전에 호출한다. `v4l2_i2c_subdev_set_name`(`v4l2-i2c.c` L41)은 `"%s%s %d-%04x"` 형식이라 `imx219 10-0010`이 되고, libcamera 모델 추출(`v4l2_subdevice.cpp` L1728 정규식)과 맞는다.
- rotation·orientation: `v4l2_fwnode_device_parse`(L258) → `v4l2_ctrl_new_fwnode_properties`. `sd.dev`는 `v4l2_i2c_subdev_init`가 채운다. 두 컨트롤은 코어 기본이 READ_ONLY라 `__v4l2_ctrl_handler_setup`이 건너뛰고, `s_ctrl` default(−EINVAL)로 가지 않는다. 실패는 probe `free_controls`로 간다.
- flip:
  - 두 컨트롤 모두 `MODIFY_LAYOUT`이 켜져 있어 libcamera `queryFlip`(`camera_sensor_legacy.cpp` L366)이 Bayer 순서가 바뀌는 flip으로 인식한다. 기본값은 0이고 하드코딩은 없다.
  - `0x0172` 쓰기(L191)는 기존 `powered` 검사 뒤에 있다. 스트림 시작 때 setup이 HFLIP·VFLIP을 각각 호출해 같은 값을 두 번 쓰는 것은 mainline과 같고 문제없다.
- grab:
  - `streaming = true` 직후 grab(L305)한다. 정지·시작 실패 경로는 `release_controls`(L313)로 모인다. 전원 on 실패는 전원 off를 건너뛰고 grab만 푼다.
  - `__v4l2_ctrl_grab`은 handler lock을 요구하는데(`v4l2-ctrls-core.c` L2626) 모두 `s_stream` lock 안이다.
  - libcamera는 `setFormat`에서 flip을 스트림 전에 설정하므로(L775) grab과 충돌하지 않는다.
- Bayer 코드: `imx219_get_format_code`(L322)의 인덱스 `hflip | vflip << 1`과 표 순서가 mainline `imx219_mbus_formats`·`imx219_get_format_code`와 같다. `lockdep_assert_held`가 있고, 네 pad op는 코드를 읽는 동안만 lock을 잡는다.
- 잠금 순서:
  - pad op → ctrl lock은 한 단계뿐이다.
  - ctrl lock을 잡은 `s_stream`·`s_ctrl` 경로는 pad op를 부르지 않는다.
  - unicam도 센서 `s_stream` 안에서 `get_fmt`를 부르지 않으므로 교착 경로가 없다.
- `set_fmt`: `get_fmt`를 공유해 요청 코드와 관계없이 현재 flip 코드와 고정 크기를 반환한다. libcamera는 flip을 켠 뒤 `SBGGR10`으로 요청하므로 요청과 결과가 같아진다.
- `get_selection`(L404): CROP 계열 (8, 8, 3280, 2464), NATIVE 3296×2480이다. mainline `set_pad_format`의 crop 계산(left = (3296 − 3280) / 2)과 `IMX219_NATIVE_*`와 같다.
- 스타일: checkpatch 0/0, `W=1` 경고 0. L464~466 `set_name`과 endpoint 검사 사이 빈 줄 하나는 보드 확인 후 정리해도 된다.
- 보드 확인 전 미검증: 노트 A1~C 전부. 특히 보드 libcamera 0.7.2의 모델 추출·flip 처리가 읽은 main 코드와 같은지는 A1 로그로 판단한다.
- 수정 지시: 없음
