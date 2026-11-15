# 18-2 통합 Yocto 이미지에 U-Boot 넣기 — Codex 지시서

작업 노트: [18-2-uboot.md](18-2-uboot.md)

## 지시 (Claude)
- 변경 파일:
  - `yocto/meta-bsp-study/conf/bsp-study-devices.inc` (수정)
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

## 구현 보고 (Codex)
- 변경 요약:
- 빌드 결과:
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`): 해당 없음(C 소스 없음)
- 판단이 필요했던 부분:

## 리뷰 (Claude)
- 결과: 통과 | 수정 필요
- 수정 지시:
