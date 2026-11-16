# 카메라: IMX219 + unicam + libcamera (직접 작성 센서 드라이버 imx219_study)

## 1. 요약
- **역할:** IMX219 센서의 RAW 프레임을 CSI-2로 받아 V4L2(`/dev/video0`)와 libcamera(`rpicam-*`)로 사진·영상·미리보기를 낸다.
- **상태:** Yocto 카드는 직접 작성 `imx219_study`로 자동 적재, libcamera 0.4.0 인식, HDMI 미리보기·사진 정상(18-2). 64GB 카드는 평소 mainline `imx219`이고, `imx219_study`는 13-3·13-4에서 시험 후 원복했다. 직접 작성 드라이버의 640×480·20fps 연속 프레임 간격은 미검증([13 6장](../learning/13-camera.md#6-트러블슈팅과-한계)).
- **담당 파일:**
  - 센서 드라이버: [imx219_study.c](../../examples/camera_sensor/imx219_study.c), [imx219-study-overlay.dts](../../examples/camera_sensor/imx219-study-overlay.dts), [imx219_compat.h](../../examples/camera_sensor/imx219_compat.h)
  - 앱: [v4l2_capture.c](../../examples/camera/v4l2_capture.c), [raw_tool.py](../../examples/camera/raw_tool.py)
  - Yocto: [imx219-study.bb](../../yocto/meta-bsp-study/recipes-kernel/imx219-study/imx219-study.bb), [libcamera bbappend](../../yocto/meta-bsp-study/recipes-multimedia/libcamera/libcamera_%25.bbappend), [bsp-study-devices.inc](../../yocto/meta-bsp-study/conf/bsp-study-devices.inc)
- **학습 문서:** [13 카메라](../learning/13-camera.md), [18 HDMI 미리보기](../learning/18-integration.md#ssh에서-실행한-카메라가-왜-hdmi-모니터에-나오는가)

## 2. 구성
흐름: `IMX219 → CSI-2 2레인 → unicam(/dev/video0, FS IRQ 시각) → ISP → libcamera → rpicam-*`. 설정은 반대로 `libcamera → /dev/v4l-subdev0 → imx219_study → 카메라 I2C(i2c-10 0x10)`.

| 항목 | 64GB 카드 (6.18.59 Debian) | Yocto 카드 (6.6.63) |
| --- | --- | --- |
| HW 연결 | CSI 커넥터, 카메라 전용 I2C `i2c-10` `0x10` ([bringup](../bringup.md#카메라-연결)) | 같음 |
| DT/overlay | 평소 `camera_auto_detect=1`(firmware가 stock `imx219`). 시험 때 `camera_auto_detect=0` + `dtoverlay=imx219-study` | `dtoverlay=imx219-study`(`RASPBERRYPI_CAMERA_V2` 미사용) |
| 센서 드라이버 | 평소 mainline `imx219`. 시험 때 `imx219_study.ko`를 `insmod` | `imx219_study`(`updates/`) 자동 적재. mainline `imx219.ko`는 복구용으로 남아 있지만 올라오지 않음 |
| 수신 드라이버 | `bcm2835_unicam_legacy` | 같음 |
| media controller | unicam compatible의 match data로 항상 켜짐(`bcm2835-unicam.c` L3303) | overlay의 `brcm,media-controller`가 있어야 켜짐([L67](../../examples/camera_sensor/imx219-study-overlay.dts#L67)) |
| 6.6 차이 | — | `devm_v4l2_sensor_clk_get()`을 compat 헤더가 `devm_clk_get()`으로 대신 |
| libcamera | 0.7.2(Debian) | 0.4.0 + rpicam-apps 1.4.2 |
| 사용자 인터페이스 | `/dev/video0`, `/dev/v4l-subdev0`, `/dev/mediaN`, `rpicam-*` | 같음 |

- 드라이버 이름·compatible은 `imx219-study`/`nakta,imx219-study`지만 엔티티 이름은 `imx219 10-0010`이다([L467](../../examples/camera_sensor/imx219_study.c#L467)). libcamera가 이름 첫 단어로 CamHelper·튜닝 `imx219.json`을 고르기 때문이다.
- 모드는 3280×2464 RAW10 하나(최대 21.19fps)다. 작은 해상도도 ISP가 축소한다.

## 3. 빌드·배포
- **64GB 카드(시험할 때만):** `config.txt`를 백업하고 자동 감지를 끈 뒤 직접 작성 overlay로 바꾼다. 의존 모듈을 먼저 올린다.

```bash
# [WSL] ~/bspworkspace/bsp-study
make -C examples/camera_sensor W=1 && make -C examples/camera_sensor deploy
# [Pi] 64GB
C=/boot/firmware/config.txt; sudo cp $C $C.bak-13-3
sudo cp ~/bsp/overlays/imx219-study.dtbo /boot/firmware/overlays-bsp-study01/
sudo sed -i 's/^camera_auto_detect=1$/camera_auto_detect=0/' $C && echo "dtoverlay=imx219-study" | sudo tee -a $C
sudo reboot   # 재부팅 후: sudo modprobe -a v4l2_cci v4l2_fwnode && sudo insmod ~/bsp/modules/imx219_study.ko
```

- **Yocto 카드:** `imx219-study.bb`가 `examples/camera_sensor`의 `.c`·compat 헤더를 빌드하고, 커널 bbappend가 overlay를 `imx219-study.dtbo`로 만든다. 부팅하면 `nakta,imx219-study` modalias로 자동 적재된다.

## 4. 정상 동작 확인
| 확인 | 명령 | 정상 결과 |
| --- | --- | --- |
| 바인딩 | `readlink /sys/bus/i2c/devices/10-0010/driver` | `.../imx219-study`(64GB 평소는 `imx219`) |
| 모듈 | `lsmod \| grep -E '^(imx219_study\|imx219\|bcm2835_unicam_legacy\|v4l2_cci) '` | Yocto: `imx219_study`·`v4l2_cci`·unicam, mainline `imx219` 없음 |
| probe 로그 | `dmesg \| grep imx219` | `imx219-study 10-0010: chip ID: 0x0219` |
| media 그래프 | `m=$(for m in /dev/media*; do media-ctl -d $m -p 2>/dev/null \| grep -qi unicam && echo $m; done \| head -1); media-ctl -d $m -p \| grep -A2 imx219` | `imx219 10-0010 -> unicam-image` |
| libcamera 인식 | `rpicam-hello --list-cameras` | `imx219 [3280x2464 10-bit RGGB]`, 모드 1개 `[21.19 fps]` |
| 미리보기(HDMI 연결) | `rpicam-hello -t 10000` | 로그 `Made DRM preview window`, 바른 방향·정상 색 |
| 사진 | `rpicam-still -o /tmp/test.jpg` | JPEG 생성, `devcheck cam` PASS |

media 장치 번호는 부팅마다 바뀔 수 있어 위처럼 driver 이름으로 찾는다. subdev·video 노드는 같은 셸에서 `sd=$(media-ctl -d $m -e "imx219 10-0010"); vd=$(media-ctl -d $m -e unicam-image)`로 잡는다. 프레임마다 찍히는 `#N (fps)` 줄은 `rpicam-hello -v 1 2>&1 | grep -v '^#'`로 거른다.

## 5. 변경 가이드
| 하고 싶은 일 | 고칠 파일 | 같이 맞출 것 | 확인 |
| --- | --- | --- | --- |
| 드라이버 로직 수정 | `examples/camera_sensor/imx219_study.c` 한 곳 | 6.18에만 있는 API면 `imx219_compat.h`, 엔티티 이름 `imx219`는 유지 | 6.18 `make W=1`, `bitbake imx219-study`, 4장 libcamera 인식 |
| 컨트롤 추가 | `imx219_init_controls()`([L264](../../examples/camera_sensor/imx219_study.c#L264) 근처)와 `s_ctrl`([L162](../../examples/camera_sensor/imx219_study.c#L162)) | libcamera 필수 컨트롤(GAIN·EXPOSURE·HBLANK·PIXEL_RATE·VBLANK)은 빼지 않음 | `v4l2-ctl -d $sd -l` |
| 모드 추가 | 레지스터 테이블([L46](../../examples/camera_sensor/imx219_study.c#L46)), `get_fmt`·`set_fmt`·`get_selection` | 지금은 상태 API 없이 고정 모드 1개라 구조 변경이 필요 | `--list-cameras` 모드 수 |
| Yocto에서 mainline으로 되돌리기 | 부트 파티션 `config.txt`(카드 리더) 또는 `bsp-study-devices.inc` | `dtoverlay=imx219-study` → `dtoverlay=imx219` | 바인딩 `imx219` |
| overlay 속성 변경 | `imx219-study-overlay.dts` | 6.6은 `brcm,media-controller` 유지, 레인·클럭은 드라이버 검사값(2레인·456MHz·24MHz)과 맞춤 | dmesg probe 오류 |

## 6. 트러블슈팅
먼저 볼 계층: 3 DT(`imx219@10` 노드) → 4 모듈 적재 → 5 probe(chip ID) → 7 media 그래프.

| 계층 | 증상 | 먼저 확인 | 원인 | 조치 |
| --- | --- | --- | --- | --- |
| 4 모듈 적재 | `lsmod`에 `bcm2835_unicam`이 없음 | `lsmod \| grep unicam` | rpi 커널의 실제 모듈 이름이 `bcm2835_unicam_legacy`(기본 DT `brcm,bcm2835-unicam`에 매칭) | 4장 명령의 이름으로 확인 |
| 4 모듈 적재 | `rmmod imx219_study`가 `Module imx219_study is in use` | `lsmod`의 Used by | unicam에 등록될 때 V4L2 코어가 센서 모듈에 `try_module_get`(`v4l2-device.c` L130) | [조치 절차: 센서 모듈 다시 올리기](#조치-절차-센서-모듈-다시-올리기) |
| 7 서브시스템 | 재부팅 뒤 unicam이 `/dev/media2` → `/dev/media0` | `media-ctl -p`의 driver | media 번호는 probe 순서로 정해짐 | 4장처럼 이름으로 찾음 |
| 3 DT·overlay | 부팅 로그 `Fixed dependency cycle(s)`(csi ↔ imx219) | — | endpoint 상호 참조를 fw_devlink가 정리한 정보 메시지 | 조치 없음 |
| 8 사용자 공간 | rpicam 경고 `Mismatch between Unicam and CamHelper for embedded data usage!` | 영상·사진이 정상인지 | 센서가 embedded data 패드를 내지 않음 | 조치 없음 |
| 8 사용자 공간 | Yocto rpicam 경고 `RPiSdn` legacy SDN tuning 1줄 | 영상·사진이 정상인지 | 튜닝 파일의 SDN 항목 관련 경고(원인 확인 안 함) | 조치 없음 |
| 8 사용자 공간 | 수동 노출(`--shutter 12000`) 사진·RAW가 매우 어두움 | 자동 노출 값(66.7ms·게인 9.85) | 실내 조명에 비해 노출·게인 부족, RAW PNG는 감마 없는 선형 | 정상. 게인·노출을 올리거나 밝은 장면 |
| 8 사용자 공간 | v4l2-compliance 뒤 찍은 장면이 컬러바 | `v4l2-ctl -d $sd -C test_pattern` | compliance가 `test_pattern`을 켠 채 남긴 것으로 추정 | `--set-ctrl test_pattern=0` |
| 8 사용자 공간 | libcamera 없이 받은 RAW가 어둡고 초록빛이며 상하 반전 | — | ISP(화이트 밸런스·감마) 없음, 모듈은 `rotation` 180 | 정상. libcamera로 받으면 flip·ISP가 적용됨 |

### 조치 절차: 센서 모듈 다시 올리기
1. 카메라 앱을 모두 끈다.
2. unicam을 먼저 내리고 센서 모듈을 내린다.
3. 새 센서 모듈을 올린 뒤 unicam을 다시 올린다.

```bash
# [Pi] 64GB
sudo rmmod bcm2835_unicam_legacy && sudo rmmod imx219_study
sudo insmod ~/bsp/modules/imx219_study.ko && sudo modprobe bcm2835_unicam_legacy
rpicam-hello --list-cameras
```

### 조치 절차: 카메라만 mainline으로 되돌리기
1. **Yocto:** 카드 리더로 부트 파티션 `config.txt`의 `dtoverlay=imx219-study`를 `dtoverlay=imx219`로 바꾼다. mainline `imx219.ko`·`imx219.dtbo`가 이미지에 있다.
2. **64GB:** 백업으로 되돌리고 재부팅한다(`sudo cp $C.bak-13-3 $C && sudo reboot`). `camera_auto_detect=1`로 돌아간다.
3. 4장 바인딩이 `imx219`인지, `rpicam-still`이 되는지 본다.

64GB 원복은 실행해 확인했고(13-4), Yocto는 준비만 했다.

**점검 포인트**
- Yocto에서 overlay에 `brcm,media-controller`가 빠지면 6.6 unicam이 media controller 모드가 아니어서 subdev가 읽기 전용이 되고 libcamera가 동작하지 않을 것으로 본다(소스 확인, 보드에서 재현하지 않음).
- 렌즈를 가려도 `exposure`·`analogue_gain`이 안 변하면 어두운 방에서 이미 최대인 것이다. 밝게 해서 감소를 본다.

## 7. 알려진 제한
- 직접 작성 드라이버는 모드 1개, embedded data·binning 없음, 컨트롤 이벤트 미지원(v4l2-compliance 46개 중 45 통과).
- 엔티티 이름을 `imx219`로 맞춰 mainline 튜닝을 그대로 쓴다.
