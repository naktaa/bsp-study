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
- 변경 요약: 지시된 `yocto/meta-bsp-study/` 아래 새 파일 5개를 생성했다. scarthgap 레이어, `raspberrypi4-64`·`poky` 최소 설정 템플릿, 레이어 4개의 상대 경로, 빌드 안내, `core-image-minimal` 기반 dropbear 이미지를 구성했다. 이미지에 추가한 기능은 SSH와 비밀번호 없는 root 로그인에 필요한 항목뿐이다. 기존 작업 노트의 변경은 건드리지 않았다.
- 빌드 결과: 미실행(3단계에서 사용자 확인)
- checkpatch (`external/linux/scripts/checkpatch.pl --no-tree -f <파일>`): 해당 없음(Yocto 메타데이터)
- 판단이 필요했던 부분:
  1. **UART:** `~/yocto/meta-raspberrypi/recipes-bsp/bootfiles/rpi-config_git.bb:44–48`에서 `do_deploy()`의 `CONFIG`가 `${DEPLOYDIR}/${BOOTFILES_DIR_NAME}/config.txt`로 지정된다. 같은 파일 `184–186`행에서 `ENABLE_UART`가 `1` 또는 `0`이면 `enable_uart=${ENABLE_UART}`를 이 파일에 추가한다. 따라서 템플릿의 `ENABLE_UART = "1"`은 `enable_uart=1` 생성으로 이어진다. 생성 산출물 자체는 아직 확인하지 않았다.
  2. **템플릿 치환과 상대 경로:** `~/yocto/poky/scripts/oe-setup-builddir:92–98`은 `sed -e "s|##OEROOT##|$OEROOT|g"`로 토큰을 치환해 빌드 디렉터리의 `conf/bblayers.conf`를 만든다. `~/yocto/poky/oe-init-build-env:38–44`에서 `OEROOT`는 스크립트 디렉터리의 정규화된 경로다(실행하지 않고 읽기 확인). 전제 배치의 `OEROOT`는 `~/yocto/poky`이므로 `meta`·`meta-poky`는 그 아래, meta-raspberrypi는 `../meta-raspberrypi`, 저장소 레이어는 `../../bspworkspace/bsp-study/yocto/meta-bsp-study`다. 이 네 경로를 토큰 치환 후 정규화하여 각 `conf/layer.conf`가 존재함을 확인했다. `BBPATH`, `BBFILES`, `POKY_BBLAYERS_CONF_VERSION = "2"`는 `~/yocto/poky/meta-poky/conf/templates/default/bblayers.conf.sample:3–6`을 따랐다.
  3. **CONF_VERSION:** `~/yocto/poky/meta-poky/conf/templates/default/local.conf.sample:282–285`의 설명과 `CONF_VERSION = "2"`를 확인해 같은 값을 사용했다.
  4. **LICENSE_FLAGS_ACCEPTED:** 현재 이미지에는 필요하지 않다는 정적 판단으로 넣지 않았다. `~/yocto/meta-raspberrypi/conf/machine/raspberrypi4-64.conf:8–13`은 Wi-Fi·BT 펌웨어를 `MACHINE_EXTRA_RRECOMMENDS`에 추가한다. 이 중 `~/yocto/meta-raspberrypi/recipes-kernel/linux-firmware-rpidistro/linux-firmware-rpidistro_git.bb:16`은 `LICENSE_FLAGS = "synaptics-killswitch"`를 요구한다. 다만 `~/yocto/poky/meta/recipes-core/images/core-image-minimal.bb:3`은 `packagegroup-core-boot ${CORE_IMAGE_EXTRA_INSTALL}`만 설치하고, `~/yocto/poky/meta/classes-recipe/core-image.bbclass:82`에서 추가 설치 목록의 기본값은 비어 있다. `~/yocto/poky/meta/recipes-core/packagegroups/packagegroup-core-boot.bb:23–42`는 `MACHINE_ESSENTIAL_EXTRA_RDEPENDS`·`MACHINE_ESSENTIAL_EXTRA_RRECOMMENDS`를 사용하며 `MACHINE_EXTRA_RRECOMMENDS`를 가져오지 않는다. 후자를 가져오는 곳은 `~/yocto/poky/meta/recipes-core/packagegroups/packagegroup-base.bb:132–134`의 `packagegroup-machine-base`다. 따라서 이 최소 이미지에서는 해당 Wi-Fi 펌웨어 레시피를 선택하지 않는다. 플래그를 받지 않은 레시피를 건너뛰는 처리는 `~/yocto/poky/meta/classes-global/base.bbclass:528–536`에 있다. 실제 파싱·의존성 확인은 사용자 3단계에 남긴다.
  5. **비밀번호 없는 root SSH:** 이미지에 `ssh-server-dropbear`, `empty-root-password`, `allow-empty-password`, `allow-root-login`을 추가했다. `~/yocto/poky/meta/classes-recipe/core-image.bbclass:57`은 SSH 기능을 `packagegroup-core-ssh-dropbear`로 연결하고, `~/yocto/poky/meta/recipes-core/packagegroups/packagegroup-core-ssh-dropbear.bb:5–6`은 dropbear 의존성과 `openssh-sftp-server` 기본 권장을 정의한다. 별도 패키지를 직접 추가하지 않았다. `~/yocto/poky/meta/classes-recipe/image.bbclass:43`에서 로그인 기능 세 항목이 유효함을 확인했다. `~/yocto/poky/meta/classes-recipe/rootfs-postcommands.bbclass:7–14`에 따라 빈 root 비밀번호를 유지하고, 빈 비밀번호 SSH 인증과 root SSH 로그인을 허용한다. 같은 파일 `257–264`행은 dropbear에 `-B`를 추가하고 `285–288`행은 root 로그인을 금지하는 `-w`를 제거한다. 필요한 세 기능을 명시했으며 `debug-tweaks`는 추가하지 않았다. 보드에서의 로그인은 미검증이다.
  6. **레이어·이미지 형식:** `~/yocto/meta-raspberrypi/conf/layer.conf:2–13`의 BBPATH·BBFILES·컬렉션·즉시 확장 패턴·우선순위 형식과 우선순위 `9`를 따랐다. 컬렉션은 `bsp-study`, 호환 릴리스는 `scarthgap`, 의존성은 `core raspberrypi`로 지정했다. `~/yocto/poky/meta/recipes-core/images/core-image-minimal.bb:1–9`를 근거로 `require recipes-core/images/core-image-minimal.bb` 뒤에 이미지 설명과 `LICENSE = "MIT"`를 지정했다.

확인 명령은 저장소 루트에서 지시서 그대로 실행했다. 출력은 다음과 같다(`git diff --check`는 출력 없이 종료 코드 0).

```text
$ find yocto/meta-bsp-study -type f | sort
yocto/meta-bsp-study/conf/layer.conf
yocto/meta-bsp-study/conf/templates/bsp-study/bblayers.conf.sample
yocto/meta-bsp-study/conf/templates/bsp-study/conf-notes.txt
yocto/meta-bsp-study/conf/templates/bsp-study/local.conf.sample
yocto/meta-bsp-study/recipes-core/images/bsp-study-image.bb
$ git -C ~/yocto/poky log -1 --format=%h
6b7c1066f7
$ git -C ~/yocto/meta-raspberrypi log -1 --format=%h
018d444
$ grep -n 'LAYERSERIES_COMPAT' ~/yocto/meta-raspberrypi/conf/layer.conf
12:LAYERSERIES_COMPAT_raspberrypi = "nanbield scarthgap"
$ grep -rnE '/home|~/|\$HOME|BB_NUMBER_THREADS|PARALLEL_MAKE' yocto/meta-bsp-study || echo none
none
$ git diff --check
```

추가 정적 확인: 네 레이어 경로의 토큰 치환·정규화와 `conf/layer.conf` 존재 확인 통과. 새 파일 5개의 줄 끝 공백·마지막 개행 확인 통과. `bitbake`·`bitbake-layers`·`oe-init-build-env`는 실행하지 않았으며, `~/yocto` 소스 수정·보드 조작·커밋도 하지 않았다.

## 리뷰 (Claude)
- 결과: 통과(정적 검토. 파싱·빌드는 작업 노트 3·4단계에서 확인)
- 범위: 새 파일 5개(`yocto/meta-bsp-study/`)만 추가됨. `~/yocto` 소스·다른 문서 변경 없음(`git status`). 홈 절대 경로·병렬도 없음, `git diff --check` 통과
- `layer.conf`: `LAYERDEPENDS`의 `raspberrypi`가 meta-raspberrypi `BBFILE_COLLECTIONS` 이름과 같음(`conf/layer.conf` L8). 형식·우선순위 9도 같음
- `TEMPLATECONF`: `oe-setup-builddir`가 "한 단계 위가 `templates`이고 `../../layer.conf`가 있어야 함"을 검사함(L54~56). `conf/templates/bsp-study` 배치가 이 조건을 만족함. `conf-summary.txt`가 없으면 poky 기본 파일을 씀(L103~106)
- `bblayers.conf.sample`: `OEROOT`는 `oe-init-build-env`에서 `readlink -f`로 정규화된 `~/yocto/poky`(L39~40)이고 `##OEROOT##`를 그 값으로 치환함(`oe-setup-builddir` L96). `../../bspworkspace/...`는 `~/yocto`와 `~/bspworkspace`가 홈 바로 아래에 있을 때만 맞음. 지시서가 정한 배치라 유지
- `ENABLE_UART`: `rpi-config_git.bb` L184~186에서 `enable_uart=1`을 `config.txt`에 추가함. `CONF_VERSION = "2"`는 poky 기본 sample과 같음
- 이미지: 세 로그인 기능이 `image.bbclass` L43의 유효 항목에 있고, `rootfs-postcommands.bbclass` L7~14가 각각 빈 비밀번호 유지·빈 비밀번호 SSH·root SSH를 처리함. `debug-tweaks` 대신 필요한 기능만 명시한 판단이 맞음
- `LICENSE_FLAGS_ACCEPTED` 생략: Wi-Fi·BT 펌웨어는 `MACHINE_EXTRA_RRECOMMENDS`(`raspberrypi4-64.conf` L8~13)에 있고, 이를 가져오는 `packagegroup-base`를 core-image-minimal이 쓰지 않으므로 맞음
- 참고(수정 요구 아님): `kernel-modules`도 같은 `MACHINE_EXTRA_RRECOMMENDS`에 있어(`rpi-base.inc` L130) 이미지에 들어가지 않음. 보드에서 `/lib/modules/$(uname -r)`가 거의 비어 있을 수 있고, 작업 노트 7단계처럼 결과만 기록함
- 수정 지시: 없음
