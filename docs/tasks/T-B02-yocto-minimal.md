# T-B02 Yocto 최소 이미지 빌드·부팅

상태: 계획
관련 roadmap 단계: 12 (Yocto 최소 이미지)
유형: C 부팅·빌드 체계
브랜치: feat/yocto-minimal (예정, 데스크톱에서 main으로부터 생성)

이 파일은 노트북 세션에서 만든 **계획 초안**이다. 데스크톱 세션에서 아래 '결정할 것'과 '데스크톱 환경 확인'을 채운 뒤 사용자 승인을 받아 상태를 `승인`으로 바꾼다.

## 목표 (Claude)
meta-raspberrypi와 저장소의 `yocto/meta-bsp-study` 레이어로 Pi 4용 최소 이미지를 빌드하고, 새 SD 카드에 써서 부팅함. 끝나면 레이어·레시피·`MACHINE`·이미지가 어떻게 커널·DTB·부트 파일·rootfs로 이어지는지 설명할 수 있음. IMU 모듈·overlay·수집 앱을 레시피로 넣고 IIO 버퍼를 다시 검증하는 일은 다음 TASK로 나눔(아래 '범위 나누기').

## 배경 개념 (Claude)
1. **레이어와 `bblayers.conf`:** poky(OE-core·bitbake) 위에 BSP 레이어(meta-raspberrypi)와 프로젝트 레이어(meta-bsp-study)를 쌓음. 레이어는 레시피(`.bb`)와 덧붙임(`.bbappend`)을 제공하고, 같은 레시피는 우선순위로 정해짐. 근거: Yocto 공식 문서 "Understanding and Creating Layers"(릴리스별 확인 필요).
2. **`MACHINE`과 BSP:** `MACHINE = "raspberrypi4-64"`이 커널 레시피(`linux-raspberrypi`), DTB·overlay 목록, firmware(`rpi-bootfiles`), `config.txt` 생성(`rpi-config`)을 정함. U-Boot 사용 여부는 meta-raspberrypi 변수(`RPI_USE_U_BOOT`로 알려짐, 선택한 브랜치 문서에서 확인 필요)로 정함.
3. **빌드 산출물과 카드 쓰기:** 이미지 레시피(`core-image-minimal` 등)가 rootfs를 만들고 wic가 부트(FAT)·rootfs(ext4) 파티션을 묶어 `.wic`(압축 시 `.wic.bz2` 등)를 만듦. 이 파일을 카드 전체에 씀. 기존 카드처럼 파일을 추가하는 방식이 아니라 카드 전체를 덮어씀.

참고 자료(읽기 전용): `../VEDA/임베디드리눅스/강의자료/임베디드리눅스2.pdf`(Yocto 빌드·`runqemu`), `주요 Yocto 환경변수과모듈레시피.pdf`. 노트북에서 텍스트만 일부 확인했고 내용 대조는 아직 하지 않음.

## 결정할 것 (데스크톱 세션에서 확정)
| 항목 | 선택지 | 초안 추천 | 확인할 근거 |
| --- | --- | --- | --- |
| Yocto 릴리스 | 최신 LTS / 최신 안정 릴리스 | 최신 LTS(지금 알려진 LTS는 scarthgap 5.0, 그 이후 LTS 출시 여부 확인 필요) | Yocto 릴리스 표, 호스트 Ubuntu 지원 목록 |
| meta-raspberrypi 브랜치 | 릴리스와 같은 이름의 브랜치 | 릴리스와 맞춤 | meta-raspberrypi 저장소 브랜치 |
| 커널 | (a) meta-raspberrypi 기본 `linux-raspberrypi` / (b) bbappend로 현재 커널 커밋 `b3495ea`(6.18.59)에 고정 | (a)로 먼저 부팅 확인. (b)는 IMU 레시피 TASK에서 필요하면 검토 | 선택한 브랜치의 기본 커널 버전 |
| 이미지 | `core-image-minimal` / `core-image-base` | `core-image-minimal` + SSH(`dropbear` 또는 `openssh`) | 원격 확인용 SSH 필요 |
| U-Boot | 포함 / 미포함 | 미포함으로 먼저 부팅 확인. 포함은 별도 확인 항목 | T-B01 결과([U-Boot 문서](../../boot/uboot/uboot-boot.md)) |
| 빌드 관리 | 수동 `oe-init-build-env` + conf / kas | 수동(학습 목적, 강의와 같은 흐름) | — |
| 병렬도 | `BB_NUMBER_THREADS`·`PARALLEL_MAKE` | 메모리에 맞춰 설정(노트북 7GB 기준이면 4) | 데스크톱 코어·메모리 |

## 데스크톱 환경 확인 (데스크톱 세션 첫 작업)
지금까지의 환경 기록(AGENTS.md 환경, bringup)은 노트북 WSL 기준이다.

```bash
# [데스크톱 WSL]
lsb_release -a; nproc; free -g; df -h ~
git -C ~/bspworkspace/bsp-study status -sb      # boot 브랜치, origin/boot와 일치
ls ~/bspworkspace/bsp-study/external ~/bspworkspace/bsp-study/out 2>&1   # 없을 수 있음(Git 대상 아님)
ssh -G pi | grep -E '^(hostname|user) '         # pi 별칭 설정 여부
```

- [ ] Ubuntu 버전이 선택한 Yocto 릴리스의 지원 호스트인지
- [ ] 디스크 여유 100GB 이상(`~/yocto` 아래 build·downloads·sstate)
- [ ] 메모리·코어 수 → 병렬도 결정. 필요하면 Windows `%UserProfile%\.wslconfig`로 WSL 메모리 조정
- [ ] Yocto 호스트 패키지 설치(공식 문서의 릴리스별 목록)
- [ ] `external/linux`·`out/kernel-rpi4`가 없음을 확인. 기본 커널(a)을 쓰면 필요 없음

## 범위 (Claude, 초안)
- 새로 만드는 파일(예정): `yocto/meta-bsp-study/conf/layer.conf`, 빌드 설정 템플릿 또는 예시(`local.conf`·`bblayers.conf`에 넣을 줄), 필요하면 이미지 레시피 1개
- 저장소 밖: `~/yocto/`(poky·meta-raspberrypi 체크아웃, `build/`, `downloads/`(DL_DIR), `sstate-cache/`(SSTATE_DIR))
- 하지 않는 것: IMU 모듈·overlay·앱 레시피, 커널 커밋 고정, U-Boot 포함, 그래픽 이미지, 기존 SD 카드 변경
- 범위 나누기: roadmap 12단계를 둘로 나눔. **T-B02 = 최소 이미지 빌드·부팅**, **T-B03 = IMU 모듈·overlay·수집 앱 레시피와 IIO 버퍼 재검증**

## 백업·복구 (Claude)
- 새 SD 카드에만 이미지를 씀. 기존 카드(Debian + 6.18.59, 센서 트랙)는 건드리지 않음
- 복구: Yocto 카드가 부팅하지 않으면 전원 분리 → 기존 카드로 교체. 기존 카드의 백업(`out/boot-backup-tb01/`, 노트북)은 그대로 유지
- 카드 쓰기 전 확인: Windows 도구에서 대상 드라이브가 **새 카드**인지 용량·이름으로 확인(잘못 고르면 다른 디스크를 덮어씀)
- 카드 구분 표시: 두 카드에 "boot"/"sensor" 표시

## 완료 기준 (Claude, 초안)
- [ ] 데스크톱 환경 확인 완료, 결정 항목 확정
- [ ] `bitbake <이미지>` 성공, `.wic` 산출물과 크기 기록
- [ ] 이미지 내용 확인: 부트 파티션의 커널·DTB·`config.txt`, rootfs의 `/lib/modules/<버전>` (빌드 산출물 읽기)
- [ ] 새 카드에 쓰고 부팅: HDMI 로그인 프롬프트 또는 SSH 접속, `uname -r`이 선택한 커널 버전
- [ ] `/proc/device-tree/model`이 Raspberry Pi 4, `cat /etc/os-release`로 Yocto 배포판 확인
- [ ] 기존 카드로 바꿔 끼우면 기존 환경(6.18.59) 정상 부팅

## 검증 절차 (Claude)
결정 확정 후 작성한다. 형식은 [T-B01](T-B01-uboot-boot.md) '검증 절차'처럼 단계별 소제목과 **[붙여 주세요]** 표시를 쓴다.

## 구현 보고 (Codex)
- 변경 요약:
- 빌드 명령·결과:
- checkpatch 결과: 해당 없음(C 소스 없음 예상)
- 판단이 필요했던 부분:
- 사용자가 보드에서 실행할 명령:

## 리뷰 (Claude)
- [ ] 범위 준수
- [ ] 복구 경로
- [ ] 과잉 구현 없음
- 수정 지시:
- 이해 확인 질문:

## 실측 결과 (사용자 제공, Claude 기록)
사용자가 준 보드 출력·로그의 원문 일부(대표 로그)와 요약을 적는다. 해석은 마감에서 한다.

## 마감 (Claude)
- 갱신한 문서:
- 미검증 항목:
- 마감 때 갱신 예정: bringup.md SD 카드 표(기존 카드 = 장치 TASK, 새 카드 = 부팅·Yocto, 새 카드 용량), 데스크톱 빌드 환경 기록
- 추천 커밋 메시지:
