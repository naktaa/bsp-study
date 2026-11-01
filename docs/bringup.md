# 환경과 참고 자료

미확인 값은 추측하지 않고 실제 명령 출력·배선 확인 후 채운다.

## 개발 호스트
| 항목 | 확인값 |
| --- | --- |
| OS / 배포판 / 아키텍처 | 미확인 |
| 컴파일러 / 버전 / 타깃 | 미확인 |
| bsp-study 경로 | `/home/nakta/bspworkspace/bsp-study` |
| VEDA 경로 / 참고 커밋 | `../VEDA` / `c2f2ace95af528c6f806c8fc273f02bdb10120c6` |
| VEDA_Final_Project 경로 / 참고 커밋 | `../VEDA_Final_Project` / `8d0ddbe579c41886f6b9b0b50eafb30e64128633` |
| 커널 소스 경로 / 커밋 / 빌드 출력 경로 | 미확인 |

## 타깃 보드
| 항목 | 확인값 |
| --- | --- |
| 보드 모델 | Raspberry Pi 4 예정, 실측 미확인 |
| OS / 아키텍처 / 커널 버전 | 미확인 |
| 부트 파티션 / 설정 파일 위치 | 미확인 |
| SSH 접속 별칭 | 미확인 |
| 센서 모델 / 보유 여부 | MPU-6050 기준 계획, 실물 미확인 |
| 데이터시트 출처 / 개정 / 센서 식별값 | 미확인 |
| I2C 버스 / 주소 / 주소 선택 핀 | 미확인 |
| SDA / SCL / INT / 전원 / GND 배선 | 미확인 |
| GPIO 번호 체계 | BCM·물리 핀·기존 wiringPi 번호 구분 필요 |
| 카메라 모델 / 연결 상태 | 미확인, 초기 단계 필수 아님 |
| 센서 모듈 전원·I/O 전압 조건 / 근거 | 미확인 |
| 실행 커널용 헤더·설정·심볼 정보 확보 방법 | 미확인 |
| 기존 센서 드라이버 / 바인딩 상태 / 전환 절차 | 미확인 |
| INT 극성·해제 조건 / FIFO 형식 | 미확인, 데이터시트와 실측 대조 필요 |
| 부팅 백업 / 복구 방법 | 미확인 |

## 첫 확인 명령
보드에서 실행한다. 장치 경로가 없다는 결과도 그대로 기록한다.

```bash
cat /proc/device-tree/model
uname -a
uname -m
cat /etc/os-release
ls -l /lib/modules
ls -l /dev/i2c-*
```

호스트에서 실행한다. 컴파일러가 없으면 그 사실부터 기록한다.

```bash
uname -m
cat /etc/os-release
command -v aarch64-linux-gnu-gcc
aarch64-linux-gnu-gcc --version
```

## 참고 자료 대조
2026-11-02 로컬 경로·커밋과 아래 일부 소스를 확인함. PDF는 파일 위치만 확인했으며 본문은 아직 검토하지 않음. 보드 실행 결과는 없음.

| 출처 / 경로 | 확인 범위와 목적 | 적용 판단 |
| --- | --- | --- |
| VEDA / 리눅스커널/수업코드/kmodule/{Makefile,hello_module.c} | 모듈 init/exit와 빌드 설정 확인 | 교육 실습에 사용. 고정 KDIR·타깃 설정은 실제 환경에 맞게 변경하고 단독 `i` 문법 오류 수정 필요 |
| VEDA / 리눅스커널/수업코드/gpiobtn/gpiobtn.c | 문자 디바이스·GPIO IRQ 흐름 확인 | 교육 실습에 사용. 고정 GPIO 번호·자원 및 오류 처리 방식은 그대로 복사하지 않고 대상 커널과 대조 |
| VEDA / 리눅스커널/강의자료, 임베디드리눅스/강의자료, 리눅스디버깅/강의자료 | 모듈·커널·Yocto·디버깅 PDF 위치 확인 | 본문 미검토, 필요한 단계에서 읽기 |
| VEDA_Final_Project / tank/src/imu_reader.cpp | 사용자 공간 I2C·FIFO·GPIO IRQ, 단위·축·시간 처리 확인 | 기존 방식은 비교 기준으로 유지. 커널 수집 구현은 별도로 검증 |
| VEDA_Final_Project / tank/include/imu_reader.hpp | ImuReader 인터페이스 확인 | EIS 선택 과제 진행 시 재검토 |
| VEDA_Final_Project / tank/config/config_template.ini | 참고 후보 | 내용 미검토, 센서 설정 실습 시 확인 |
| VEDA_Final_Project / tank/docs/HYBRID_EIS_FRAMEWORK.md | 참고 후보 | 내용 미검토, EIS 선택 과제에서 확인 |

센서 데이터시트, 대상 커널의 드라이버·DT binding·API는 환경 확정 후 출처·버전과 함께 추가한다. 과거 예제의 API를 현재 환경에 적용할 수 있다고 가정하지 않는다.

비교 기록은 필요할 때 아래 형식으로 한 항목씩 추가한다.
- 주제 / 참고 파일·커밋:
- 기존 방식의 목적:
- 판단: 유지 / 교육 실습에만 사용 / 변경
- 이유 / 대상 커널 근거:
