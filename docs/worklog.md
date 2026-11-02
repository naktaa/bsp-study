# 진행 상태와 기록

## 현재 상태
- 현재 단계: 2·7의 빌드·부팅·모듈 로드/해제 실습 완료. 다음은 3단계 사용자/커널 경계 계획 설명. 0단계 미확인 항목은 필요 시 보완
- 완료한 실습: 1 — 사용자 프로그램 실행, 2 — 모듈 크로스 빌드·로드·해제, 7 — 커스텀 커널 빌드·부팅·모듈 확인 및 복구 절차 기록 (실제 복구 시험은 미수행)
- 준비된 항목: 드라이버 중심 지침·로드맵·기록 양식
- 참고 자료: 로컬 경로·커밋과 일부 VEDA 예제·기존 IMU 소스 확인, 상세 범위는 bringup 참고
- 보드 검증: hello-arm64 실행 및 커스텀 커널 6.18.59-v8-bsp-study01 부팅·ARM64·기존 ext4 루트·새 모듈 디렉터리 확인. bsp_hello 로드·해제와 커널 로그 확인. 센서 검증은 미수행
- 다음 작업: 3단계 문자 디바이스의 목적과 VEDA 대응 코드를 설명하고 작은 작업 계획 제시. 실습 소스 수정·빌드는 사용자가 직접 수행하며 에이전트는 설명·읽기 검토·진행 기록만 수행
- 막힌 항목: 없음. 환경 정보 수집 중이며 센서 실물·배선 등은 미확인.

## 재개 방법
AGENTS.md와 이 파일을 읽고 roadmap의 현재 단계만 확인한다. 이전 결과를 다시 조사하기 전에 아래 근거와 bringup을 확인한다. 환경이 바뀌었으면 바뀐 항목만 재확인한다.
구현·수정 전에 목적·개념·변경 파일과 범위·검증 방법을 먼저 설명하고 사용자와 대화하며 진행한다.

## 필수 단계별 상태
- [ ] 0. 환경·배선·참고 자료 확인
- [x] 1. 사용자 프로그램 빌드·배포
- [x] 2. 커널 모듈 기초
- [ ] 3. 사용자/커널 경계
- [ ] 4. I2C 레지스터와 센서 기준선
- [ ] 5. Device Tree와 장치 매칭
- [ ] 6. 최소 I2C 센서 드라이버
- [x] 7. 커널 빌드·부팅·복구
- [ ] 8. 인터럽트·FIFO 수집
- [ ] 9. IIO와 버퍼 수집
- [ ] 10. 측정·디버깅·재현

EIS·Yocto·U-Boot·카메라는 선택 과제이며 필수 완료 조건에 포함하지 않는다.

## 기록
완료한 작업 또는 의미 있는 실패만 아래 형식으로 짧게 추가한다. 매 명령마다 보고서를 만들지 않는다. 현재 상태도 함께 갱신한다.

### YYYY-MM-DD / 단계 / 작업
- 수행:
- 관측: 실행 위치와 핵심 출력 또는 로그 경로
- 문제·해결: 없으면 생략
- 다음:

### 2026-11-02 / 준비 / 드라이버 중심 학습 방향 정리
- 수행: 사용자와 합의한 저수준 드라이버·수집 중심으로 문서 5개 수정. 구현 전 계획 설명과 대화, 사용자가 직접 커밋하고 에이전트는 짧은 Conventional Commits 메시지만 제안하는 규칙 명시. EIS·Yocto는 선택 과제로 분리.
- 관측: 로컬 VEDA·기존 프로젝트 일부 소스와 참고 커밋 확인. 문서 준비와 실습 완료를 구분하며 모든 보드 실습은 미완료로 유지.
- 다음: 실제 보드·센서 보유 및 접속 상태와 개발 호스트 환경 확인.

### 2026-11-02 / 준비 / 커밋 수행 방식 수정
- 수행: 사용자 요청으로 초기 커밋을 취소하고 문서 파일은 유지. 스테이징도 해제. 기본은 사용자 직접 커밋, 특정 파일 커밋을 명시적으로 요청받은 경우만 에이전트가 수행하도록 지침·README 수정.
- 관측: 문서 변경은 작업 트리에 유지하며 새 커밋은 만들지 않음.
- 다음: 추천 커밋 메시지만 전달. 다음 학습 작업은 환경 확인 단계.

### 2026-11-02 / 준비 / 커밋 접두사 표기 수정
- 수행: 사용자 요청으로 커밋 접두사의 첫 글자를 대문자로 표기하도록 지침과 README 예시 수정 (`Chore:`, `Fix:`, `Feat:`).
- 관측: 커밋·스테이징 없이 문서만 수정.
- 다음: 환경 확인 단계.

### 2026-11-02 / 0 / 호스트 설명과 보드 환경 출력 확인
- 수행: 사용자 설명과 보드의 model・uname・os-release 출력을 해석해 bringup에 기록.
- 관측: Windows 노트북 + WSL2, Ubuntu 22.04 LTS는 추정. 보드는 Raspberry Pi 4 Model B Rev 1.5, aarch64, 커널 6.18.50+rpt-rpi-v8, Debian 13 (trixie), 버전 상세 13.7. 센서는 보유하나 미연결이며 정확한 모델은 미확인.
- 다음: WSL 실제 버전·아키텍처·툴체인과 센서 모델 확인. 0단계는 진행 중이며 완료 처리하지 않음.

### 2026-11-02 / 0 / WSL 환경과 기존 센서 설정 확인
- 수행: 사용자 제공 WSL 출력 반영, 기존 프로젝트의 IMU 설정·기본값·설명 문서 검토.
- 관측: Ubuntu 22.04.5 LTS, x86_64, WSL2 커널 6.18.33.2-microsoft-standard-WSL2. gcc와 aarch64-linux-gnu-gcc 경로 존재. 프로젝트는 MPU-6050 기준, 기본 bus=1·addr=0x68·GPIO line offset=17·target_hz=500·FIFO 활성. 실제 센서 모듈·배선·수집률 확인과 구분.
- 다음: 컴파일러 버전·타깃 및 보드 모듈 빌드 경로·I2C 상태 확인.

### 2026-11-02 / 0 / 컴파일러 버전과 타깃 확인
- 수행: 사용자 제공 gcc 버전·크로스 타깃 출력을 기록.
- 관측: gcc와 aarch64-linux-gnu-gcc 모두 GCC 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04.3), 크로스 타깃 aarch64-linux-gnu. 실제 보드 실행·모듈 빌드는 미검증.
- 다음: 보드의 실행 커널용 모듈 디렉터리·build 링크와 I2C 장치 노드 확인.

### 2026-11-02 / 0 / 보드 모듈 디렉터리와 I2C 노드 확인
- 수행: 사용자 제공 ls 출력 기록.
- 관측: 실행 커널의 모듈 디렉터리와 linux-headers-6.18.50+rpt-rpi-v8을 가리키는 build 심볼릭 링크 존재. 링크 대상 실재와 빌드 가능 여부는 미검증. /dev/i2c-1·20·21은 root:i2c, crw-rw----. 센서 인식 확인과 구분.
- 다음: 링크 대상 디렉터리와 kernel.release 확인. 센서 연결과 커널 모듈 빌드는 아직 수행하지 않음.

### 2026-11-02 / 0~1 / 헤더 경로 확인과 첫 사용자 프로그램 준비
- 수행: 사용자 제공 build 링크 대상·kernel.release 출력 기록. 실행 환경의 아키텍처·커널을 출력하는 examples/hello/hello.c 추가.
- 관측: 헤더 대상 존재, kernel.release=6.18.50+rpt-rpi-v8로 실행 커널과 일치. 모듈 빌드 성공을 의미하지는 않음. 사용자 프로그램 보드 실행은 미수행.
- 다음: WSL 크로스 빌드·실행 형식 확인 후 보드에서 출력 검증. 0단계 전체 완료 표시는 보류.

### 2026-11-02 / 1 / ARM64 사용자 프로그램 크로스 빌드 확인
- 수행: 사용자가 WSL에서 aarch64-linux-gnu-gcc -Wall -Wextra -O0 -g로 hello.c 빌드 후 file 출력 제공.
- 관측: ELF 64-bit LSB PIE, ARM aarch64, 동적 링크, interpreter=/lib/ld-linux-aarch64.so.1, debug_info 포함, not stripped. 보드 실행은 미검증.
- 문제·해결: 최초 bspworkspace에서 상대 경로 examples/hello/hello.c를 찾지 못함. bsp-study로 이동 후 해결.
- 다음: 보드 libc·로더 및 전송 대상 확인 후 전송·실행.

### 2026-11-02 / 1 / 보드 로더와 libc 확인
- 수행: 사용자 제공 hostname·로더 경로·getconf 출력 기록.
- 관측: hostname=nakta, 동적 로더 링크 존재, glibc 2.41. SSH 대상은 사용자 제시 nakta@192.168.45.224. 실행 호환성은 프로그램 실행 전까지 미검증.
- 다음: scp 전송 후 보드에서 프로그램 출력과 종료 상태 확인.

### 2026-11-02 / 1 / 보드 실행 확인 및 단계 완료
- 수행: 사용자가 보드의 /tmp/bsp-study-hello-arm64 실행과 echo $? 결과 제공. 성공한 빌드·전송·실행 명령은 examples/hello/README.md에 기록.
- 관측: Hello, BSP! / Running on: aarch64 / Kernel: 6.18.50+rpt-rpi-v8 / 종료 상태 0. WSL x86_64에서 생성한 ARM64 동적 링크 프로그램의 보드 실행 확인.
- 다음: 사용자 공간 프로그램과 커널 모듈 차이 및 init/exit 실습 계획 설명.

### 2026-11-02 / 2 / 모듈 빌드 도구 확인과 최소 코드 준비
- 수행: 사용자 제공 보드 GCC·Make·커널 Makefile·Module.symvers 출력 기록. examples/hello_module에 bsp_hello.c와 Kbuild용 Makefile 추가.
- 관측: 보드 GCC 14.2.0, Make 4.4.1, 커널 Makefile·Module.symvers 존재. 첫 모듈은 기존 헤더를 활용해 보드 네이티브 빌드로 진행. 코드는 init/exit 로그만 출력하며 GPIO·센서 접근 없음. 빌드·로드·해제는 아직 미수행.
- 다음: 보드에서 make로 빌드하고 modinfo의 이름·vermagic 확인. 이후 로드·해제와 로그 검증.

### 2026-11-02 / 2 / 수업 코드 대조 방식 반영
- 수행: 사용자 요청으로 VEDA 대응 파일과 구현 차이·이유를 함께 설명하도록 AGENTS.md에 명시. hello_module.c·Makefile과 현재 예제 대조.
- 관측: init/exit 등록 구조는 동일. 로그 수준 NOTICE→INFO, 명시적 printk 헤더, 이름·메타데이터·스타일 차이 설명. 수업 소스의 단독 i는 문법 오류이며 현재 Makefile은 보드 네이티브 빌드용 최소 Kbuild 설정.
- 다음: 사용자가 코드를 함께 검토하고 보드 빌드·modinfo 결과 확인.

### 2026-11-02 / 2 / Makefile 작성 학습 반영
- 수행: 사용자 요청으로 현재 실습부터 Make를 함께 학습하도록 지침·로드맵 수정. 모듈 Makefile에 all/clean, KDIR, 재귀 make, .PHONY, KERNELRELEASE 분기 추가.
- 관측: 수업 Makefile과 같은 편의 타깃을 제공하되 보드 네이티브 빌드용 기본 KDIR, 재정의 가능 변수와 Kbuild 분리를 사용. 모듈 소스의 pr_info는 유지하며 KERN_INFO 대응임을 설명. 보드 빌드·로드는 아직 미검증.
- 다음: 파일 재전송 후 보드에서 make와 modinfo 출력 확인.

### 2026-11-02 / 7 / VEDA 흐름으로 커널 크로스 빌드 준비
- 수행: PDF 10·29~31·37쪽 수업 명령 확인. 사용자 요청으로 WSL 크로스 빌드를 기본으로 지침·로드맵 조정.
- 관측: 수업은 rpi-6.12.y와 ARM64 크로스 빌드 사용. 이번에는 6.18 계열·Pi 4 설정과 기존 GCC 11.4를 기준으로 준비. 소스 다운로드·설정·커널 빌드는 아직 미수행. 기존 모듈 Makefile은 수정하지 않음.
- 다음: 소스 다운로드·커밋 확인 후 설정 진행. 설치·백업·복구와 initramfs 처리는 설치 단계에서 설명.

### 2026-11-02 / 7 / 커널 소스 다운로드 확인
- 수행: 사용자 clone·log 출력과 로컬 소스 대조.
- 관측: external/linux의 HEAD=b3495ea401d709af0701a85babcba5e3d788a602 (6.18.59), 검토 커밋과 일치. 로컬 min-tool-version.sh는 ARM64에 GCC 8.1 이상, binutils 2.30 이상을 요구. 현재 도구가 기본 요구사항을 충족하며 설정 생성·빌드는 아직 미수행.
- 다음: 커밋 고정과 Pi 4 ARM64 기본 설정 생성.

### 2026-11-02 / 7 / Pi 4 기본 설정 생성 확인
- 수행: 사용자 제공 bcm2711_defconfig 출력과 로컬 .config 확인.
- 관측: out/kernel-rpi4/.config 생성, ARM64·MODULES·MODVERSIONS·EXT4_FS 활성, LOCALVERSION=-v8. 소스는 detached HEAD 상태. HOSTCC/HOSTLD는 WSL용 설정 도구 빌드이며 ARM64 커널 본체 빌드는 아직 미수행.
- 다음: LOCALVERSION=-v8-bsp-study01 지정 후 설정 정리·커널 릴리스 확인. Git 자동 접미사 방지를 위해 이후 커널·모듈 명령에 LOCALVERSION=를 일관되게 사용.

### 2026-11-02 / 7 / 버전 설정 변경과 생성 설정 불일치 확인
- 수행: 사용자 kernelrelease=6.18.59-v8 출력과 로컬 설정·setlocalversion 소스 대조.
- 관측: .config의 LOCALVERSION은 -v8-bsp-study01로 변경됐지만 include/config/auto.conf는 -v8 유지. olddefconfig의 No change는 scripts/config가 이미 수정한 .config를 더 변경하지 않았다는 뜻. kernelrelease는 auto.conf를 읽으므로 이전 값 출력.
- 문제·해결: 앞선 안내에 생성 설정 동기화 단계 누락. make prepare 후 kernelrelease를 다시 확인하도록 안내. prepare 실행·해결 결과는 아직 미확인.
- 다음: WSL에서 prepare와 kernelrelease 출력 확인.

### 2026-11-02 / 7 / prepare 및 커스텀 버전 확인
- 수행: 사용자 제공 prepare·kernelrelease 출력과 로컬 생성 설정 대조.
- 관측: prepare 정상 종료 출력, .config와 auto.conf의 LOCALVERSION=-v8-bsp-study01 일치. include/config/kernel.release와 명령 출력 모두 6.18.59-v8-bsp-study01. 전체 커널 빌드·보드 설치·부팅은 아직 미수행.
- 다음: 수업과 같은 -j4 Image modules dtbs 크로스 빌드 진행.

### 2026-11-02 / 7 / 커널 크로스 빌드 완료
- 수행: 사용자가 Image modules dtbs 빌드 종료 상태 0을 전달. 로컬 산출물 확인.
- 관측: ARM64 Image 약 28MiB, Pi 4 DTB·vc4-kms-v3d DTBO·Module.symvers·modules.order 존재, .ko 1917개. kernel.release=6.18.59-v8-bsp-study01. Pi 설치·부팅은 아직 미수행.
- 다음: 설치 계획·백업·복구 수단 확인 후 배포 준비. 7단계 전체 완료 표시는 부팅 검증 후 진행.

### 2026-11-02 / 7 / 기존 부팅 파일 백업 확인
- 수행: 사용자 제공 scp 백업 확인 출력과 로컬 파일 확인.
- 관측: out/boot-backup-20261102-222418/firmware의 config.txt·cmdline.txt·kernel8.img·initramfs8 확인. 사용자가 카드 리더기 사용 가능하다고 확인. Pi 부팅 설정은 아직 변경하지 않음.
- 다음: WSL에서 modules_install의 INSTALL_MOD_PATH를 로컬 배포 디렉터리로 지정.

### 2026-11-02 / 7 / 모듈 배포 트리 구성 완료
- 수행: 사용자 modules_install 종료 상태 0 및 로컬 배포 트리 확인.
- 관측: out/pi4-install/lib/modules/6.18.59-v8-bsp-study01에 modules.dep·modules.alias 생성, 배포 트리 약 30MiB. build는 WSL 커널 출력 디렉터리로 연결됨. Pi 시스템 설치는 미수행.
- 다음: 커널 이미지·DT 파일을 별도 설치 이름으로 함께 묶어 Pi 홈 디렉터리에 전송.

### 2026-11-02 / 7 / 배포 파일 Pi 전송 확인
- 수행: 사용자에게 Pi 홈의 pi4-study01.tar.gz 수신 확인 받음. 로컬 배포 아카이브 구성 확인.
- 관측: 약 32MiB 아카이브에 새 이름의 커널·Pi 4 DTB·overlays-bsp-study01과 버전별 모듈 트리 포함. Pi 시스템 설치·부팅 설정 변경은 아직 미수행.
- 다음: .config를 /boot/config-새버전으로 배치하고 새 파일 설치·initramfs 생성. mkinitramfs에 새 커널 버전과 출력 파일을 명시해 사용.

### 2026-11-02 / 7 / Pi 파일 설치·initramfs 생성 완료 보고
- 수행: 사용자가 파일 설치·depmod·mkinitramfs 안내 작업을 모두 완료했다고 전달.
- 관측: 개별 명령 출력·종료 상태는 아직 미수신. 새 파일 존재와 initramfs 내용 확인 전이며 새 커널 부팅도 미검증.
- 다음: Pi 파일 확인 후 새 커널 선택 설정 및 재부팅 검증.

### 2026-11-02 / 7 / 새 부팅 파일·모듈·initramfs 확인
- 수행: 사용자 제공 Pi 파일 목록·모듈 디렉터리·lsinitramfs 출력 검토.
- 관측: 새 Image 28MiB, Pi 4 DTB 56KiB, vc4-kms-v3d DTBO, 새 initramfs 18MiB 존재. 기존·새 모듈 디렉터리 공존. initramfs에서 새 버전 modules.symbols 등 확인.
- 다음: Pi 4 선택 블록을 config.txt 앞에 두고 기존 auto_initramfs=1 유지. 설정 출력 확인 후 재부팅.

### 2026-11-02 / 7 / 커스텀 커널 부팅 확인
- 수행: 사용자가 부팅 설정 변경·재부팅 후 uname·findmnt·모듈 디렉터리 출력 제공.
- 관측: uname -r=6.18.59-v8-bsp-study01, aarch64, /dev/mmcblk0p2 ext4 루트 정상 마운트, 해당 버전 /lib/modules 디렉터리 존재. SSH 재접속 확인. 복구 방법은 준비했으나 실제 복구 시험은 미수행.
- 다음: 승인 후 모듈 Makefile을 같은 WSL 커널 빌드 환경으로 수정하고 bsp_hello.ko 빌드·로드·해제로 모듈 동작 확인.

### 2026-11-03 / 2 / 사용자 직접 모듈 빌드·로드·해제 완료
- 수행: 사용자가 Makefile 수정·크로스 빌드, clean·재빌드·증분 빌드 확인 후 Pi에서 insmod·lsmod·dmesg·rmmod 실행. 에이전트는 산출물과 메타데이터 읽기 확인만 수행.
- 관측: ARM64 .ko의 vermagic은 6.18.59-v8-bsp-study01과 일치. Pi lsmod에서 bsp_hello 확인, loaded 로그 12843.104640. rmmod 후 lsmod 결과 없음, unloaded 로그 12932.342295. out-of-tree taint는 외부 모듈 로드 이력임을 설명.
- 다음: 사용자/커널 데이터 전달을 위한 문자 디바이스 실습 계획 설명. 실습 파일·빌드 산출물은 변경하지 않았고 커밋하지 않음.
