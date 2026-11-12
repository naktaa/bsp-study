# 12 Yocto 최소 이미지 — Codex 지시서

작업 노트: [12-yocto-minimal.md](12-yocto-minimal.md)

## 지시 (Claude)
- 실행 장소: 데스크톱 WSL. 작업 노트 1·2단계가 끝나 `~/yocto/poky`·`~/yocto/meta-raspberrypi`(scarthgap)가 있는 상태에서 시작한다. 없으면 멈추고 보고한다.
- 변경 파일(모두 새 파일, `yocto/meta-bsp-study/` 아래):
  - `conf/layer.conf`
  - `conf/templates/bsp-study/local.conf.sample`
  - `conf/templates/bsp-study/bblayers.conf.sample`
  - `conf/templates/bsp-study/conf-notes.txt`
  - `recipes-core/images/bsp-study-image.bb`
- 요구사항 — `layer.conf`:
  - 컬렉션 이름 `bsp-study`, 패턴·우선순위는 meta-raspberrypi `conf/layer.conf` 형식을 따른다
  - `LAYERSERIES_COMPAT_bsp-study = "scarthgap"`, `LAYERDEPENDS_bsp-study = "core raspberrypi"`
- 요구사항 — `local.conf.sample`:
  - `MACHINE = "raspberrypi4-64"`, `DISTRO = "poky"`
  - `DL_DIR`·`SSTATE_DIR`는 `${TOPDIR}/../downloads`, `${TOPDIR}/../sstate-cache`(빌드 디렉터리 `~/yocto/build` 기준)
  - `ENABLE_UART = "1"`(meta-raspberrypi `rpi-config`가 `config.txt`에 `enable_uart=1`을 쓰는지 확인하고 근거를 구현 보고에 적는다)
  - `BB_NUMBER_THREADS`·`PARALLEL_MAKE`는 넣지 않는다(bitbake 기본값 사용, 메모리 부족 시 사용자가 빌드 디렉터리 `local.conf`에서 조정)
  - `~`·`$HOME`·`/home/...` 절대 경로를 쓰지 않는다
  - `CONF_VERSION`은 poky `meta-poky/conf/templates/default/local.conf.sample`의 값과 같게
  - `LICENSE_FLAGS_ACCEPTED`는 raspberrypi4-64 기본 패키지(Wi-Fi·BT 펌웨어 등)가 요구할 때만 넣고, 어떤 레시피가 요구하는지 근거를 구현 보고에 적는다
  - poky 기본 sample 전체를 복사하지 않는다. 위 항목과 동작에 필요한 최소 줄만 둔다
- 요구사항 — `bblayers.conf.sample`:
  - poky `meta`·`meta-poky`, meta-raspberrypi, meta-bsp-study 4개. `meta-yocto-bsp`는 넣지 않는다
  - 경로는 `##OEROOT##` 기준 상대 경로로 쓴다. 디렉터리 배치는 `~/yocto/poky`, `~/yocto/meta-raspberrypi`, `~/bspworkspace/bsp-study`를 전제로 하고, 치환 방식은 poky `scripts/oe-setup-builddir`에서 확인한다
- 요구사항 — `conf-notes.txt`: `bitbake bsp-study-image` 한 줄 안내
- 요구사항 — `bsp-study-image.bb`:
  - `core-image-minimal`을 `require`로 가져온다
  - SSH는 dropbear(`ssh-server-dropbear`)
  - 비밀번호 없는 root SSH 로그인을 켠다. scarthgap에서 쓰는 방식(`debug-tweaks` 또는 `empty-root-password`·`allow-empty-password`·`allow-root-login`)을 poky `meta/classes-recipe/`·`meta/conf/documentation.conf` 등에서 확인하고 근거를 구현 보고에 적는다
  - `SUMMARY`·`LICENSE`는 OE-core 이미지 레시피 형식을 따른다. 그 밖의 패키지는 넣지 않는다
- 주석은 한글로, 코드만으로 알기 어려운 이유(학습용 root 로그인 등)에만 쓴다
- 하지 말 것: bitbake·`bitbake-layers`·`oe-init-build-env` 실행, `~/yocto` 아래 파일 수정, U-Boot(`RPI_USE_U_BOOT`)·Wi-Fi 설정·커널 bbappend·센서 레시피·추가 패키지, 문서 수정(구현 보고 제외), 보드 조작
- 확인 명령:

```bash
# [데스크톱 WSL] ~/bspworkspace/bsp-study
find yocto/meta-bsp-study -type f | sort
git -C ~/yocto/poky log -1 --format=%h
git -C ~/yocto/meta-raspberrypi log -1 --format=%h
grep -n 'LAYERSERIES_COMPAT' ~/yocto/meta-raspberrypi/conf/layer.conf
grep -rnE '/home|~/|\$HOME|BB_NUMBER_THREADS|PARALLEL_MAKE' yocto/meta-bsp-study || echo none
git diff --check
```

## 구현 보고 (Codex)
- 변경 요약:
- 빌드 결과:
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`):
- 판단이 필요했던 부분:

## 리뷰 (Claude)
- 결과: 통과 | 수정 필요
- 수정 지시:
