# 18-2 통합 Yocto 이미지에 U-Boot와 직접 작성 카메라 드라이버 넣기 — Codex 지시서

작업 노트: [18-2-uboot.md](18-2-uboot.md)

## 지시 (Claude)
A와 B는 파일이 겹치지 않는다. 둘 다 데스크톱 WSL에서 구현한다(B는 6.6 빌드 트리가 필요함).

### A. U-Boot
- 변경 파일:
  - `yocto/meta-bsp-study/conf/bsp-study-devices.inc` (수정, `RPI_USE_U_BOOT` 줄만. 카메라 줄은 B)
  - `yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/rpi-u-boot-scr.bbappend` (새 파일)
  - `yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in` (새 파일)
- 요구사항:
  1. `bsp-study-devices.inc` 끝에 한글 주석 1줄과 `RPI_USE_U_BOOT = "1"`을 추가한다. 기존 줄은 바꾸지 않는다.
  2. `rpi-u-boot-scr.bbappend`는 `FILESEXTRAPATHS:prepend := "${THISDIR}/files:"` 한 줄만 둔다.
     - 대상 레시피 이름은 버전이 없는 `meta-raspberrypi/recipes-bsp/rpi-u-boot-scr/rpi-u-boot-scr.bb`이다. 그래서 파일 이름에 `_%`를 붙이지 않는다.
  3. `files/boot.cmd.in`은 meta-raspberrypi 기본 `recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in`(scarthgap `018d444`, 4줄)에서 3번째 줄 `if test ! -e @@BOOT_MEDIA@@ 0:1 uboot.env; then saveenv; fi;`만 뺀 3줄로 만든다.
     - 나머지 줄은 자리표시(`@@KERNEL_IMAGETYPE@@`, `@@KERNEL_BOOTCMD@@`, `@@BOOT_MEDIA@@`)까지 원본과 같게 둔다. 레시피의 `do_compile` sed가 이 자리표시를 치환한다.
     - 맨 앞에 `#` 한글 주석 2줄 이내로 의도를 적는다: firmware DT를 그대로 쓴다는 것, 카드에 환경을 저장하지 않으려고 saveenv를 뺐다는 것.
- 하지 말 것:
  - U-Boot 소스·패치·defconfig 변경
  - 커널 레시피·이미지 레시피·`IMAGE_BOOT_FILES`·`KERNEL_IMAGETYPE`·`config.txt` 관련 변수 변경
  - `local.conf.sample`·`bblayers.conf.sample` 변경
  - bitbake 실행, 문서 수정(이 지시서의 '구현 보고' 섹션만)
- 빌드 확인 명령: bitbake는 사용자가 실행한다. Codex는 아래 정적 확인만 한다.

```bash
# [WSL] ~/bspworkspace/bsp-study
diff <(sed '3d' ~/yocto/meta-raspberrypi/recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in) \
     <(grep -v '^#' yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in) && echo same-as-default-minus-saveenv
grep -n 'RPI_USE_U_BOOT' yocto/meta-bsp-study/conf/bsp-study-devices.inc
cat yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/rpi-u-boot-scr.bbappend
git status --short
```

### B. 카메라 드라이버(imx219_study)
- 변경 파일:
  - `examples/camera_sensor/imx219_study.c` (수정: compat 헤더 include와 6.6에 없는 API 호출부만)
  - `examples/camera_sensor/imx219_compat.h` (새 파일, 6.6에 없는 API가 있을 때만)
  - `examples/camera_sensor/imx219-study-overlay.dts` (수정, `brcm,media-controller;` 한 줄만. 리뷰 B 결정)
  - `yocto/meta-bsp-study/recipes-kernel/imx219-study/imx219-study.bb` (새 파일)
  - `yocto/meta-bsp-study/recipes-kernel/imx219-study/files/Makefile` (새 파일)
  - `yocto/meta-bsp-study/recipes-kernel/linux/linux-raspberrypi_%.bbappend` (수정)
  - `yocto/meta-bsp-study/conf/bsp-study-devices.inc` (수정, 카메라 줄만)
  - `yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb` (수정, `IMAGE_INSTALL` 한 항목)
  - 6.6 `.config`에 `CONFIG_V4L2_CCI_I2C`가 꺼져 있을 때만 `yocto/meta-bsp-study/recipes-kernel/linux/files/bsp-study.cfg`에 한 줄 추가
- 요구사항:
  1. **6.6 API 확인(먼저):** 6.6 `kernel-source`와 빌드 트리 `.config`에서 아래를 확인하고 결과를 구현 보고에 표로 남긴다. 경로는 18단계 Codex A와 같다.
     - `include/media/v4l2-cci.h`의 `CCI_REG8/16`, `cci_read`, `cci_write`, `cci_multi_reg_write`, `devm_cci_regmap_init_i2c` 존재와 시그니처
     - `CONFIG_V4L2_CCI`·`CONFIG_V4L2_CCI_I2C`
     - `devm_v4l2_sensor_clk_get`, `v4l2_i2c_subdev_set_name`, `v4l2_fwnode_device_parse`, `v4l2_ctrl_new_fwnode_properties`, `__v4l2_ctrl_grab`, `v4l2_async_register_subdev_sensor`
     - pad ops 시그니처(`struct v4l2_subdev_state *`), i2c `.probe` 시그니처(인자 1개)
     - stock `arch/arm/boot/dts/overlays/imx219-overlay.dts`·`imx219.dtsi`의 fragment target 라벨과 supply·clock 라벨이 우리 overlay(`i2c0if`, `cam1_clk`, `i2c0mux`, `i2c_csi_dsi`, `csi1`, `cam1_reg`, `cam_dummy_reg`)와 같은지
     - meta-raspberrypi `recipes-bsp/bootfiles/rpi-config_git.bb`에서 `RASPBERRYPI_CAMERA_V2`가 바꾸는 줄 전부
  2. **compat:** 6.6에 없는 API만 `imx219_compat.h`에서 `LINUX_VERSION_CODE`로 갈라 처리한다(`examples/mpu6050_iio/mpu6050_compat.h` 형식). 예: `devm_v4l2_sensor_clk_get`이 없으면 6.6에서만 `devm_clk_get(dev, NULL)`로 대체한다. 6.18 쪽 코드 경로와 동작은 바꾸지 않는다. 모두 있으면 헤더를 만들지 않는다.
  3. **모듈 레시피:** `recipes-kernel/mpu6050-iio/mpu6050-iio.bb`와 같은 형식으로 `imx219-study.bb`를 만든다. `FILESEXTRAPATHS`로 `examples/camera_sensor`를 가리키고 `SRC_URI`에 `imx219_study.c`(있으면 `imx219_compat.h`)와 레시피용 `Makefile`, `inherit module`. `files/Makefile`은 mpu6050-iio 레시피용 Makefile과 같은 형식으로 `obj-m := imx219_study.o`.
  4. **overlay:** `linux-raspberrypi_%.bbappend`의 `FILESEXTRAPATHS`에 `examples/camera_sensor`를 더하고, `SRC_URI`에 `file://imx219-study-overlay.dts`, `do_patch:append`에 기존 3개와 같은 `install` 한 줄을 추가한다.
  5. **`bsp-study-devices.inc`:**
     - `RASPBERRYPI_CAMERA_V2 = "1"` 줄을 지워 `dtoverlay=imx219`가 생기지 않게 한다.
     - `RPI_KERNEL_DEVICETREE_OVERLAYS:append`에 `overlays/imx219-study.dtbo`를 추가한다.
     - `RPI_EXTRA_CONFIG:append`에 `\ndtoverlay=imx219-study`를 추가한다.
     - 요구사항 1에서 `RASPBERRYPI_CAMERA_V2`가 overlay 말고 다른 줄도 바꾼다면 그 줄이 필요한지 구현 보고에 적고 멈춘다.
  6. **이미지:** `bsp-study-image.bb`의 `IMAGE_INSTALL:append`에 `imx219-study`를 추가한다. mainline `imx219.ko`(`kernel-modules`)는 빼지 않는다.
  7. **overlay(리뷰 B 결정):** `fragment@101`(`target = <&csi1>`) `__overlay__`의 `status = "okay";` 다음 줄에 `brcm,media-controller;` 한 줄을 추가한다. 6.6 unicam은 이 속성이 있어야 media controller 모드가 되고(stock `imx219-overlay.dts` L55와 같음), 없으면 subdev가 읽기 전용이 된다.
  8. **빌드 확인:** 18단계 Codex A처럼 6.6 빌드 트리로 외부 모듈을 `W=1` 빌드하고, overlay를 6.6 dtc로 컴파일한다. 레시피용 Makefile로도 한 번 빌드한다. 결과는 임시 디렉터리에만 둔다.
- 하지 말 것:
  - 드라이버 기능 추가·변경(binning·모드·컨트롤), overlay `.dts`의 위 한 줄 외 변경
  - A의 파일(`rpi-u-boot-scr`)과 `RPI_USE_U_BOOT` 줄 수정
  - mainline `imx219`를 이미지에서 빼기, `local.conf.sample`·`bblayers.conf.sample` 변경
  - bitbake 실행, 보드 조작, 문서 수정(구현 보고 제외)
- 빌드 확인 명령(경로는 18단계 Codex A 보고와 같게 맞춘다):

```bash
# [데스크톱 WSL] ~/bspworkspace/bsp-study
K=<6.6 kernel-source>; B=<6.6 빌드 트리>; T=$(mktemp -d)
cp examples/camera_sensor/imx219_study.c examples/camera_sensor/imx219_compat.h yocto/meta-bsp-study/recipes-kernel/imx219-study/files/Makefile "$T"/ 2>/dev/null
make -C "$T" KERNEL_SRC="$K" O="$B" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
"$B/scripts/dtc/dtc" -@ -I dts -O dtb -o "$T/imx219-study.dtbo" examples/camera_sensor/imx219-study-overlay.dts
"$B/scripts/dtc/dtc" -I dtb -O dts "$T/imx219-study.dtbo" 2>/dev/null | grep -n 'brcm,media-controller'
grep -n 'imx219' yocto/meta-bsp-study/conf/bsp-study-devices.inc yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb yocto/meta-bsp-study/recipes-kernel/linux/linux-raspberrypi_%.bbappend
external/linux/scripts/checkpatch.pl --no-tree -f examples/camera_sensor/imx219_study.c
git status --short
```

### C. 레시피용 Makefile `clean` 타깃 (4단계 빌드 실패 수정)
- 배경:
  - 4단계 `bitbake bsp-study-image`에서 `mpu6050-iio`·`inmp441-codec`의 `do_configure`가 `make: *** No rule to make target 'clean'`으로 실패했다.
  - poky `meta/classes-global/base.bbclass` L335~341의 `base_do_configure`는 이전 `configure.sstate`가 있고 해시가 바뀌었으며 `${B}`에 Makefile이 있으면 `oe_runmake clean`을 먼저 실행한다. 이번에 커널이 다시 빌드되면서 두 레시피의 해시가 바뀌었다.
  - `imx219-study`는 첫 빌드라 통과했지만, 다음에 커널이 바뀌면 같은 이유로 실패한다.
  - 18단계 파일 2개는 원래 18-2 범위 밖이지만 이 실패를 고치려면 필요하다(사용자 승인, 2026-11-16).
- 변경 파일:
  - `yocto/meta-bsp-study/recipes-kernel/mpu6050-iio/files/Makefile` (수정)
  - `yocto/meta-bsp-study/recipes-kernel/inmp441-codec/files/Makefile` (수정)
  - `yocto/meta-bsp-study/recipes-kernel/imx219-study/files/Makefile` (수정)
- 요구사항:
  1. 세 파일 모두 `.PHONY`에 `clean`을 더하고, `modules_install` 다음에 아래 타깃을 추가한다. 기존 줄은 바꾸지 않는다.
     ```make
     clean:
     	rm -f *.o *.ko *.mod *.mod.c .*.cmd Module.symvers modules.order
     ```
     - 재작업(2차 빌드 실패 후): 처음에는 `$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) clean`으로 지시했다. 그런데 Kbuild 외부 모듈 clean(`kernel-source/Makefile` L1930~1932)은 `M=` 아래를 하위 폴더까지 `find`해 `*.ko.*` 등을 지운다. `S = ${WORKDIR}`이라 `image/`·`package/`의 `.ko.xz`까지 pseudo 밖에서 지워졌고, `do_package`가 pseudo `path mismatch`로 abort(exit 134)했다.
     - 그래서 poky `meta-skeleton/recipes-kernel/hello-mod/files/Makefile`처럼 현재 폴더만 지우는 `rm -f`로 바꿨다. 사용자 지시로 Claude가 직접 수정했다(2026-11-16).
  2. 주석은 넣지 않는다(이유는 이 지시서와 학습 문서에 둔다).
- 하지 말 것: 레시피 `.bb`에 `CLEANBROKEN` 추가, `examples/` 아래 Makefile 변경, bitbake 실행, 문서 수정(구현 보고 제외)
- 빌드 확인 명령: B와 같은 6.6 경로로 레시피용 Makefile 하나에서 `modules` → `clean` → `modules`를 순서대로 실행한다. `clean` 뒤에 `.ko`가 지워지는지 확인한다.

```bash
# [데스크톱 WSL] ~/bspworkspace/bsp-study
K=<6.6 kernel-source>; B=<6.6 빌드 트리>; T=$(mktemp -d)
cp examples/mpu6050_iio/mpu6050_iio.c examples/mpu6050_iio/mpu6050_compat.h yocto/meta-bsp-study/recipes-kernel/mpu6050-iio/files/Makefile "$T"/
M="make -C $T KERNEL_SRC=$K O=$B ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux-"
$M modules && $M clean && ls "$T"/*.ko 2>/dev/null | wc -l && $M modules    # clean 뒤 0
for f in yocto/meta-bsp-study/recipes-kernel/*/files/Makefile; do echo "== $f"; grep -nE '^\.PHONY|^clean:' "$f"; done
git status --short
```

## 구현 보고 (Codex)

### A. U-Boot
- 변경 요약 (2026-11-16):
  - `bsp-study-devices.inc` 끝에 한글 주석과 `RPI_USE_U_BOOT = "1"`을 추가했다. 기존 카메라 설정과 나머지 줄은 유지했다.
  - 버전 없는 레시피에 맞춰 `rpi-u-boot-scr.bbappend`를 만들고 지정한 `FILESEXTRAPATHS` 한 줄만 넣었다.
  - `files/boot.cmd.in`은 로컬 meta-raspberrypi `018d444` 기본 파일에서 3번째 환경 저장 줄만 제거했다. firmware DT·bootargs 선택, 커널 로드, 커널 시작 명령과 자리표시는 그대로 두고 한글 주석 2줄을 붙였다.
- 빌드 결과:
  - A는 지시에 따라 빌드를 실행하지 않고 정적 확인만 했다. `boot.scr` 생성·이미지 빌드·배포·부팅은 미검증이다.
  - 아래 원본 비교 명령은 종료 코드 0, `same-as-default-minus-saveenv`를 출력했다.

    ```bash
    diff <(sed '3d' ~/yocto/meta-raspberrypi/recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in) \
         <(grep -v '^#' yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/files/boot.cmd.in) && echo same-as-default-minus-saveenv
    ```

  - `rg -n 'RPI_USE_U_BOOT' yocto/meta-bsp-study/conf/bsp-study-devices.inc`: L16에 `RPI_USE_U_BOOT = "1"` 한 줄을 확인했다.
  - `cat yocto/meta-bsp-study/recipes-bsp/rpi-u-boot-scr/rpi-u-boot-scr.bbappend`: 지정한 한 줄과 일치했다.
  - `git diff --check`: 종료 코드 0, 출력 없음.
- checkpatch: 해당 없음(C 소스 변경 없음).
- 판단이 필요했던 부분:
  - 주석에도 환경 저장 명령 이름을 넣지 않아 후속 `strings boot.scr` 검색에서 실행문과 주석이 혼동되지 않게 했다.
  - 작업 시작 전부터 수정되어 있던 `docs/tasks/18-2-uboot.md`는 수정하지 않았다.

### B. 카메라 드라이버
- 변경 요약 (2026-11-16, 갱신된 B 지시 승인 후):
  - `imx219_study.c`에는 `imx219_compat.h` include만 추가했다. 기존 함수·초기화·모드·컨트롤 코드는 그대로다.
  - 새 compat 헤더는 `LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)`에서만 `devm_v4l2_sensor_clk_get()`을 `devm_clk_get()`으로 처리한다. 기존 호출 인자가 `NULL`이므로 6.6에서는 `devm_clk_get(dev, NULL)`이 된다. 6.18에서는 기존 V4L2 helper를 사용한다.
  - `imx219-study-overlay.dts`의 `fragment@101`(`csi1`) `__overlay__`에서 `status = "okay";` 바로 다음에 `brcm,media-controller;` 한 줄만 추가했다. HEAD 원본과 전체 내용을 비교해 이 한 줄 외 변경이 없음을 확인했다.
  - MPU6050 레시피 형식으로 `imx219-study.bb`와 레시피용 `files/Makefile`을 추가했다. 소스·compat 헤더는 `examples/camera_sensor`에서 가져오고 Makefile은 레시피 디렉터리를 우선한다.
  - 커널 bbappend에 카메라 소스 검색 경로·overlay URI·`do_patch:append`의 복사 한 줄을 추가했다.
  - 장치 설정에서 `RASPBERRYPI_CAMERA_V2 = "1"`을 제거하고 study DTBO·overlay 설정을 추가했다. 이미지에는 `imx219-study` 한 항목을 추가했다. `kernel-modules`와 stock `imx219.dtbo` 기본 목록은 유지했다.
  - `CONFIG_V4L2_CCI`·`CONFIG_V4L2_CCI_I2C`가 이미 `m`이어서 config fragment는 변경하지 않았다. A의 파일·설정도 수정하지 않았다.
- 확인 경로:
  - `K=~/yocto/build/tmp/work-shared/raspberrypi4-64/kernel-source`
  - `B=~/yocto/build/tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git/linux-raspberrypi4_64-standard-build`
  - 아래는 로컬 6.6 소스·빌드 설정의 읽기 확인 결과이며 실행·실측 결과가 아니다.

| 확인 항목 | 6.6 확인 결과 |
| --- | --- |
| `CCI_REG8(x)` / `CCI_REG16(x)` | `include/media/v4l2-cci.h` L43~44에 각각 폭 1·2바이트 정의가 있다. |
| `cci_read` | `int cci_read(struct regmap *map, u32 reg, u64 *val, int *err)`가 있다. |
| `cci_write` | `int cci_write(struct regmap *map, u32 reg, u64 val, int *err)`가 있다. |
| `cci_multi_reg_write` | `int cci_multi_reg_write(struct regmap *map, const struct cci_reg_sequence *regs, unsigned int num_regs, int *err)`가 있다. |
| `devm_cci_regmap_init_i2c` | `struct regmap *devm_cci_regmap_init_i2c(struct i2c_client *client, int reg_addr_bits)`가 있다. `CONFIG_V4L2_CCI_I2C` 조건부 선언이다. |
| CCI 설정 | 빌드 `.config` L4587~4588에 `CONFIG_V4L2_CCI=m`, `CONFIG_V4L2_CCI_I2C=m`. config fragment 추가 불필요. |
| `devm_v4l2_sensor_clk_get` | `include/`와 `drivers/media/v4l2-core/` 검색에서 없음. compat 처리 필요. |
| `v4l2_i2c_subdev_set_name` | `void (struct v4l2_subdev *sd, struct i2c_client *client, const char *devname, const char *postfix)` 선언이 있다. |
| `v4l2_fwnode_device_parse` | `int (struct device *dev, struct v4l2_fwnode_device_properties *props)` 선언이 있다. |
| `v4l2_ctrl_new_fwnode_properties` | `int (struct v4l2_ctrl_handler *hdl, const struct v4l2_ctrl_ops *ctrl_ops, const struct v4l2_fwnode_device_properties *p)` 선언이 있다. |
| `__v4l2_ctrl_grab` | `void (struct v4l2_ctrl *ctrl, bool grabbed)` 선언이 있다. |
| `v4l2_async_register_subdev_sensor` | `int (struct v4l2_subdev *sd)` 선언이 있다. |
| pad ops | `enum_mbus_code`·`enum_frame_size`·`get_fmt`·`set_fmt`·`get_selection` 모두 두 번째 인자가 `struct v4l2_subdev_state *`이며 현재 구현과 맞는다. |
| I2C `.probe` | `int (*probe)(struct i2c_client *client)`로 인자 1개이며 현재 구현과 맞는다. |
| stock overlay 라벨 | `imx219-overlay.dts`·`imx219.dtsi`의 기본 target `i2c0if`·`cam1_clk`·`i2c0mux`·`i2c_csi_dsi`·`csi1`, clock `cam1_clk`, supply `cam1_reg`·`cam_dummy_reg`는 study overlay와 같다. |
| `RASPBERRYPI_CAMERA_V2` | meta-raspberrypi `rpi-config_git.bb` L225~228에서 값이 `1`이면 `# Enable Sony RaspberryPi Camera(imx219)` 주석과 `dtoverlay=imx219`를 추가한다. 다른 기능 설정은 추가하지 않는다. |

- 빌드 결과 (데스크톱 6.6.63):
  - 최종 직접 외부 모듈 빌드와 레시피용 Makefile 빌드는 모두 종료 코드 0·`W=1` 경고 0이다. 두 방식 모두 ARM aarch64 `imx219_study.ko`를 생성했다.
  - 레시피용 첫 빌드는 종료 코드 0이지만 생성 파일 시각이 약 2초 미래라는 `Clock skew` 경고가 나왔다. 아래 `clean`으로 임시 산출물만 정리한 뒤 재빌드해 최종 로그에서 경고가 없음을 확인했다.
  - 실제 임시 디렉터리는 `/tmp/imx219-study.aW6rp3`이다. 소스·설정 트리를 수정하지 않고 산출물·로그는 이 아래에 두었다. 직접 빌드의 첫 시도는 임시 Makefile 복사 누락으로 실패했으나 기존 Makefile을 복사한 뒤 성공했다. 최종 실행 명령은 다음과 같다.

    ```bash
    W=$HOME/yocto/build/tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git
    K=$HOME/yocto/build/tmp/work-shared/raspberrypi4-64/kernel-source
    B=$W/linux-raspberrypi4_64-standard-build
    T=/tmp/imx219-study.aW6rp3
    export PATH=$W/recipe-sysroot-native/usr/bin/aarch64-poky-linux:$W/recipe-sysroot-native/usr/bin:$PATH
    # 작업 때 mktemp로 만든 T의 direct·recipe·dtbo 디렉터리를 사용했다.
    cp examples/camera_sensor/imx219_study.c examples/camera_sensor/imx219_compat.h examples/camera_sensor/Makefile "$T/direct/"
    cp examples/camera_sensor/imx219_study.c examples/camera_sensor/imx219_compat.h yocto/meta-bsp-study/recipes-kernel/imx219-study/files/Makefile "$T/recipe/"
    make -C "$K" O="$B" M="$T/direct" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
    make -C "$K" O="$B" M="$T/recipe" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- clean
    make -C "$T/recipe" KERNEL_SRC="$K" O="$B" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
    "$B/scripts/dtc/dtc" -@ -I dts -O dtb -o "$T/dtbo/imx219-study.dtbo" examples/camera_sensor/imx219-study-overlay.dts
    "$B/scripts/dtc/dtc" -I dtb -O dts "$T/dtbo/imx219-study.dtbo" > "$T/dtbo/imx219-study.dts" 2> "$T/dtc-decompile.log"
    rg -n -B 3 -A 2 'brcm,media-controller' "$T/dtbo/imx219-study.dts"
    ```

  - DTBO 컴파일은 종료 코드 0·경고 0이다. 역변환한 `fragment@101` `__overlay__`의 L71에 `brcm,media-controller;`가 있음을 확인했다.
  - 역변환 자체는 종료 코드 0이지만 `/__local_fixups__/fragment@100/__overlay__/imx219@10`에 `reg`가 없다는 `unit_address_vs_reg` 경고 1건이 나왔다. `-@`가 생성한 참조 수정용 노드에 대한 경고이며 소스의 센서 노드에는 `reg = <0x10>;`이 있다. overlay의 다른 내용은 변경하지 않았다.
  - `FILESEXTRAPATHS`의 실제 경로 해석, 레시피용 Makefile 우선 선택, overlay와 C 소스의 변경 범위 비교를 통과했다. `git diff --check`도 종료 코드 0이다.
  - BitBake·전체 커널 빌드·설치·보드 조작은 미실행이다. 생성될 `config.txt`·패키지·자동 적재·libcamera 동작은 미검증이다. 로컬 6.18 트리가 없어 6.18 재빌드도 미실행이다.
- checkpatch (이 데스크톱에는 `external/linux`가 없어 이전 18단계와 같은 6.6 `$K/scripts/checkpatch.pl --no-tree -f <파일>`을 사용):
  - `examples/camera_sensor/imx219_study.c`: 종료 코드 0, **0 errors / 0 warnings**(559줄).
  - `examples/camera_sensor/imx219_compat.h`: 종료 코드 1, **0 errors / 2 warnings**(18줄). 1건은 지시서에서 요구한 `LINUX_VERSION_CODE` 사용이고, 다른 1건은 양쪽이 상수인 `LINUX_VERSION_CODE < KERNEL_VERSION(...)`의 비교 방향 경고다. 기존 `mpu6050_compat.h`와 같은 표기를 유지하고 억제 옵션은 쓰지 않았다.
- 판단이 필요했던 부분:
  - **버전 경계:** GitHub 도구로 upstream [v6.17 헤더](https://github.com/torvalds/linux/blob/v6.17/include/media/v4l2-common.h)에는 helper가 없고 [v6.18 헤더](https://github.com/torvalds/linux/blob/v6.18/include/media/v4l2-common.h)에는 `static inline struct clk *devm_v4l2_sensor_clk_get(struct device *dev, const char *id)`가 있는 것을 확인했다. 따라서 6.18을 경계로 잡았다. 로컬 6.18 트리와의 비교·빌드는 후속 확인이다.
  - **clock 대체:** v6.18 helper 설명은 clock producer가 있는 경우 `devm_clk_get()`과 같은 동작임을 명시한다. 이번 DT는 기존 `cam1_clk`를 연결하므로 구형 분기에서 해당 API로 대체한다. ACPI용 동작은 추가하지 않았다.
  - **media controller:** 초기에는 overlay 변경이 금지되어 보류했으나, 갱신된 B 요구사항 7과 사용자 승인에 따라 지정한 한 줄만 추가했다. 6.6 unicam의 기본값·DT 속성 판정·subdev 등록 방식은 앞선 읽기 확인과 같다. 실제 libcamera·6.18 동작은 사용자 검증으로 남긴다.

### C. 레시피용 Makefile `clean` 타깃
- 변경 요약 (2026-11-16):
  - `mpu6050-iio`·`inmp441-codec`·`imx219-study` 레시피의 `files/Makefile` 세 파일에 `.PHONY`의 `clean`과 `modules_install` 다음의 `clean` 타깃을 추가했다.
  - `clean`은 `$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) clean`으로 대상 커널 Kbuild에 현재 외부 모듈 디렉터리 정리를 맡긴다. 기존 `obj-m`·`modules`·`modules_install` 처리와 기본 타깃은 그대로다.
  - 주석·`CLEANBROKEN`은 추가하지 않았다. 레시피 `.bb`·`examples/` Makefile·기존 A/B 구현은 수정하지 않았다.
- 빌드 결과 (데스크톱 6.6.63):
  - 지시서대로 MPU6050 소스·compat 헤더·수정한 레시피용 Makefile을 임시 디렉터리 `/tmp/recipe-clean.gBKSP4`에 복사해 검증했다.
  - `modules` → `clean` → `modules` 모두 종료 코드 0, 오류·경고 로그 0건이다. 두 `modules` 빌드는 `W=1`을 적용했다.
  - 첫 빌드 뒤 `mpu6050_iio.ko` 존재를 확인했다. `clean` 뒤 해당 파일이 없고 임시 디렉터리의 `.ko` 수가 `0`임을 확인했다. 재빌드 뒤 ARM aarch64 `mpu6050_iio.ko`가 다시 생성됐다.
  - 실행 명령:

    ```bash
    W=$HOME/yocto/build/tmp/work/raspberrypi4_64-poky-linux/linux-raspberrypi/6.6.63+git
    K=$HOME/yocto/build/tmp/work-shared/raspberrypi4-64/kernel-source
    B=$W/linux-raspberrypi4_64-standard-build
    T=$(mktemp -d /tmp/recipe-clean.XXXXXX)
    cp examples/mpu6050_iio/mpu6050_iio.c examples/mpu6050_iio/mpu6050_compat.h yocto/meta-bsp-study/recipes-kernel/mpu6050-iio/files/Makefile "$T/"
    export PATH=$W/recipe-sysroot-native/usr/bin/aarch64-poky-linux:$W/recipe-sysroot-native/usr/bin:$PATH
    make -C "$T" KERNEL_SRC="$K" O="$B" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
    test -f "$T/mpu6050_iio.ko"
    make -C "$T" KERNEL_SRC="$K" O="$B" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- clean
    test ! -e "$T/mpu6050_iio.ko"
    find "$T" -maxdepth 1 -name '*.ko' -print | wc -l
    make -C "$T" KERNEL_SRC="$K" O="$B" ARCH=arm64 CROSS_COMPILE=aarch64-poky-linux- W=1 modules
    test -f "$T/mpu6050_iio.ko"
    file "$T/mpu6050_iio.ko"
    ```

  - 세 Makefile 모두 L3의 `.PHONY`에 `clean`, L11~12에 지정한 타깃·레시피가 있음을 확인했다. 수정 전 내용과 비교해 `.PHONY` 확장과 마지막 `clean` 블록 추가 외 변경이 없음을 확인했다.
  - 세 Makefile을 지정한 `git diff --check -- <세 파일>`은 종료 코드 0이다. 전체 `git diff --check`는 기존 C 지시문 코드 블록 L99의 공백 뒤 탭 들여쓰기를 지적한다. 해당 줄은 구현 보고 밖의 지시문이므로 수정하지 않았다.
  - `inmp441-codec`·`imx219-study`는 동일한 타깃의 정적 확인만 했다. BitBake `do_configure` 재실행·이미지 빌드·보드 실행은 미검증이다. 전체 커널 빌드·설치·보드 조작도 실행하지 않았다.
- checkpatch: 해당 없음(C 소스 변경 없음, 실행하지 않음).
- 판단이 필요했던 부분:
  - 로컬 poky `meta/classes-global/base.bbclass`의 `base_do_configure`에서 stamp 해시가 달라지고 Makefile이 있으면 `oe_runmake clean`을 호출하는 것을 읽기 확인했다. 이를 우회하는 설정 대신 지시된 정리 타깃을 제공했다.
  - 검증의 `M=$(CURDIR)`는 임시 모듈 디렉터리만 가리킨다. 커널 소스·빌드 트리 전체를 정리하는 `make clean`은 실행하지 않았다.

## 리뷰 (Claude)

### A. U-Boot — 통과 (2026-11-16)
- 범위: 변경은 지시한 3개 파일뿐이다(`git status`: `bsp-study-devices.inc` 수정, `recipes-bsp/` 새 디렉터리). 카메라 줄·다른 변수는 그대로다.
- `bsp-study-devices.inc` L15~16: 주석 1줄과 `RPI_USE_U_BOOT = "1"`만 끝에 추가했다.
- `rpi-u-boot-scr.bbappend`: `FILESEXTRAPATHS:prepend := "${THISDIR}/files:"` 한 줄. 레시피 이름에 버전이 없어 `_%`를 붙이지 않은 것도 맞다.
- `files/boot.cmd.in`: 주석 2줄 + 원본 1·2·4줄이다. 자리표시 `@@BOOT_MEDIA@@`·`@@KERNEL_IMAGETYPE@@`·`@@KERNEL_BOOTCMD@@`가 그대로라 레시피 `do_compile`의 sed가 치환한다. Claude도 원본 3번째 줄(`... saveenv; fi;`)과 대조했다.
- 주석에 `saveenv` 단어를 넣지 않아 4단계 `strings boot.scr | grep saveenv` 판정이 흐려지지 않는다. 좋은 판단이다.
- 확인 필요(보드): `#` 주석 줄은 U-Boot hush 파서가 처리한다. 11단계 v2026.10 `boot.cmd`도 주석이 있었지만 2024.01에서는 6단계 부팅으로 확인한다.

### B. 카메라 — 보류 판단 타당, 지시서 갱신 후 재개
- 6.6 API 표는 Claude가 같은 경로에서 다시 확인했다. `CONFIG_V4L2_CCI_I2C=m`이고, 6.6에 없는 것은 `devm_v4l2_sensor_clk_get` 하나라 compat 범위가 작다.
- 보류 사유를 확인했다.
  - 6.6 `bcm2835-unicam.c` L88 `static int media_controller;`(기본 0), L3387~3389에서 DT `brcm,media-controller`가 있을 때만 `mc_api = true`가 된다.
  - stock `imx219-overlay.dts` L55의 `csi1`에는 이 속성이 있고, study overlay `fragment@101`에는 없다. 6.6 기본 DT(`arch/arm/boot/dts/broadcom/`)에도 없다.
  - mc_api가 false이면 subdev 노드가 읽기 전용이고 libcamera rpi 파이프라인(media controller 필요)이 동작하지 않을 가능성이 크다.
- 6.18(13-3·13-4)에서 같은 overlay로 `media-ctl`·libcamera가 동작한 이유는 확인 필요다. 6.18 unicam 기본값이나 64GB 카드 설정 차이일 수 있다. 노트북에서 확인한다(아래 지시의 노트북 항목).
- 수정 지시(사용자 승인, 2026-11-16): B 변경 파일에 `imx219-study-overlay.dts`를 넣고 요구사항 7(`brcm,media-controller;` 한 줄)을 추가했다. 나머지 B 요구사항은 그대로 진행한다.

### B. 카메라 — 통과 (2026-11-16)
- 범위: 변경 파일은 갱신된 B 목록과 같다. `imx219_study.c`는 include 1줄(L16), overlay는 `fragment@101`의 `brcm,media-controller;` 1줄만 바뀌었다. config fragment는 `CONFIG_V4L2_CCI_I2C=m`이라 건드리지 않았다. A의 파일과 `RPI_USE_U_BOOT` 줄도 그대로다.
- compat(`imx219_compat.h`): 6.18 미만에서만 `devm_v4l2_sensor_clk_get()`을 `devm_clk_get(dev, id)`로 대신한다. 호출부(`imx219_study.c` L477)는 `NULL` id라 6.6에서는 DT `clocks = <&cam1_clk>`의 첫 clock을 얻고, 바로 뒤 24MHz 검사(L481)가 그대로 동작한다. 6.18 경로는 바뀌지 않는다. 경계(6.18)는 upstream 헤더 비교 근거이고, rpi 6.18.59에서 이 helper로 빌드된 13-3·13-4가 같은 결론이다.
- 레시피: `imx219-study.bb`는 `mpu6050-iio.bb`와 이름·경로만 다르다. `files/`가 앞이라 레시피용 Makefile이 `examples/camera_sensor/Makefile`보다 먼저 선택된다.
- 커널 bbappend·`bsp-study-devices.inc`: overlay 4번째 경로·SRC_URI·`do_patch` 복사 1줄, DTBO 목록·`dtoverlay=imx219-study` 추가, `RASPBERRYPI_CAMERA_V2` 제거(meta-raspberrypi `rpi-config_git.bb` L225~228은 이 값으로 `dtoverlay=imx219`만 넣음). stock `overlays/imx219.dtbo`는 `rpi-base.inc` L45 기본 목록에 남아 "카메라만 되돌리기"가 가능하다.
- 이름: driver `imx219-study`(L550), compatible `nakta,imx219-study`(L543), subdev 이름 `imx219`(L467)라 노트 6단계 기대값과 맞는다.
- 빌드·checkpatch: 6.6 직접·레시피 Makefile 빌드 `W=1` 경고 0, dtbo에 `brcm,media-controller` 포함. `imx219_study.c` 0/0. `imx219_compat.h`의 경고 2건(`LINUX_VERSION_CODE`)은 `mpu6050_compat.h`와 같은 표기라 허용한다. 역변환 `unit_address_vs_reg` 1건은 `__local_fixups__` 노드 경고라 무시한다.
- 남은 확인: 노트북 6.18 재빌드(노트 3단계), bitbake 결과(4단계), 6.6 libcamera 0.4.0 인식과 media controller 모드(6단계).

### C. Makefile `clean` — 1차 통과 판단 철회, 재작업 (2026-11-16)
- 아래 1차 리뷰의 확인에는 하위 폴더가 없어서 Kbuild clean이 `image/`·`package/`까지 지우는 것을 놓쳤다. 2차 빌드가 `do_package` pseudo abort로 실패했다(원인은 지시 C 요구사항 1).
- 재작업: 세 Makefile의 `clean` 레시피만 `rm -f *.o *.ko *.mod *.mod.c .*.cmd Module.symvers modules.order`로 바꿨다(사용자 지시로 Claude 직접 수정). `.PHONY`와 다른 타깃은 그대로다.
- 오염된 pseudo DB는 세 레시피 `bitbake -c clean` 후 다시 빌드해 정리한다(노트 4단계 결과).

### C. Makefile `clean` — 1차 리뷰 (2026-11-16, 위에서 철회)
- 범위: 바뀐 파일은 지시한 레시피용 Makefile 3개뿐이다. 세 파일 모두 `.PHONY`에 `clean`을 넣고 끝에 `clean` 블록(L11~12, 탭 들여쓰기)만 추가했다. 레시피 `.bb`·`examples/` Makefile·A/B 파일은 그대로다.
- 동작: 기본 타깃은 그대로 `modules`이고 `module.bbclass`가 부르는 `modules`·`modules_install`도 바뀌지 않았다.
- bitbake 호출 형태 확인(Claude):
  - Codex 검증은 `O=`(빌드 트리)를 넘겼지만, `do_configure`는 `make KERNEL_SRC=<kernel-source> clean`처럼 `O=` 없이 부른다.
  - 그래서 Claude가 같은 형태로 다시 실행했다. `.config`가 없는 `work-shared/.../kernel-source`에서도 exit 0이고 가짜 `.o`·`.ko`·`modules.order`가 지워졌다. 커널의 `clean`은 `.config`가 필요 없는 타깃이고 `M=` 디렉터리만 정리한다.
- `git diff --check`가 지적한 지시문 L99는 Makefile 예시라 탭이 필요한 줄이다. 그대로 둔다.
- 남은 확인: 4단계 `bitbake bsp-study-image` 재실행에서 `mpu6050-iio`·`inmp441-codec` `do_configure` 통과
