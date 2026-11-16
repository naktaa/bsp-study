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
  7. **빌드 확인:** 18단계 Codex A처럼 6.6 빌드 트리로 외부 모듈을 `W=1` 빌드하고, overlay를 6.6 dtc로 컴파일한다. 레시피용 Makefile로도 한 번 빌드한다. 결과는 임시 디렉터리에만 둔다.
- 하지 말 것:
  - 드라이버 기능 추가·변경(binning·모드·컨트롤), overlay `.dts` 내용 변경
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
grep -n 'imx219' yocto/meta-bsp-study/conf/bsp-study-devices.inc yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb yocto/meta-bsp-study/recipes-kernel/linux/linux-raspberrypi_%.bbappend
external/linux/scripts/checkpatch.pl --no-tree -f examples/camera_sensor/imx219_study.c
git status --short
```

## 구현 보고 (Codex)
A와 B를 나눠 적는다.
- 변경 요약:
- 빌드 결과:
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`): 해당 없음(C 소스 없음)
- 판단이 필요했던 부분:

## 리뷰 (Claude)
- 결과: 통과 | 수정 필요
- 수정 지시:
