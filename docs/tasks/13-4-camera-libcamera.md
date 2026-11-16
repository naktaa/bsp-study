# 13-4 직접 작성 센서 드라이버의 libcamera 연동

상태: 완료 · 브랜치: `feat/camera-libcamera` · Codex 지시서: [13-4-camera-libcamera.codex.md](13-4-camera-libcamera.codex.md)

## 목표
13-3 `imx219_study`를 libcamera가 받아들이는 조건에 맞춘다. 그러면 ISP를 거쳐 정상 색으로, 바로 선 영상을 받을 수 있다. 64GB 카드에서 사진과 VNC 실시간 미리보기까지 확인해 "직접 작성한 센서 드라이버 → libcamera → ISP → 화면" 경로를 설명할 수 있게 한다. Yocto 이미지 반영은 다음 노트에서 한다.

## 알아둘 개념
근거: raspberrypi/libcamera main `670581c`(읽기 확인). 보드 libcamera 0.7.2+rpt의 같은 코드는 확인 필요.

1. **libcamera가 센서를 알아보는 방법:** 엔티티 이름에서 첫 공백 앞을 모델 이름으로 쓴다(`src/libcamera/v4l2_subdevice.cpp` L1727). rpi 파이프라인은 이 이름으로 CamHelper(`src/ipa/rpi/common/ipa_base.cpp` L149)와 튜닝 파일 `imx219.json`(`pipeline/rpi/common/pipeline_base.cpp` L1190)을 고른다. 13-3의 `imx219-study`로는 helper를 만들지 못하므로 엔티티 이름만 `imx219 10-0010`으로 맞춘다. 드라이버 이름과 compatible은 그대로 둔다.
2. **필수와 권장:**
   - 필수 컨트롤 5개(GAIN·EXPOSURE·HBLANK·PIXEL_RATE·VBLANK)가 없으면 오류로 중단한다(`sensor/camera_sensor_legacy.cpp` L455). 13-3에 이미 있다.
   - HBLANK은 최소값과 최대값이 같으면 읽기 전용으로 보고 쓰지 않는다(`ipa_base.cpp` L1866).
   - selection 4종과 `CAMERA_ORIENTATION`은 없어도 경고 후 기본값을 쓴다(L350·L397). 이번에 넣는다.
3. **방향은 누가 정하나:** libcamera는 `CAMERA_SENSOR_ROTATION`(DT `rotation = <180>`)을 읽어 필요한 변환을 계산한다(L614). 쓰기 가능한 `HFLIP`/`VFLIP`이 있으면 이를 켜고, `MODIFY_LAYOUT` 플래그가 있으면 바뀐 Bayer 순서의 코드로 `set_fmt`를 부른다(L366·L775).
   - 드라이버에서 flip을 하드코딩하지 않는 이유: 장착 방향은 보드마다 다르므로 드라이버는 DT 값과 수단만 제공하고, 결정은 사용자 공간이 해야 같은 드라이버를 다른 보드에서도 쓸 수 있다.

## 결정
- 추가 4가지: 엔티티 이름, rotation·orientation 컨트롤, flip 컨트롤과 Bayer 코드 전환, `get_selection`. overlay·config는 13-3 그대로 쓴다.
- binning 모드는 넣지 않는다.
  - 3280×2464 한 모드라 vblank 최소 32에서 최대 약 21.2fps이고, 20fps는 vblank 181로 맞는다.
  - 작은 해상도도 ISP가 8MP에서 축소한다(mainline은 센서 binning 모드 사용). 미리보기가 끊기는지 보고 Yocto 단계 전에 판단한다.
- 비교 기준(mainline): 13-3 원복 사진 `~/bsp/cam3/after.jpg`.
- 송출(`rpicam-vid` TCP·VEDA 앱)은 이번 범위에서 뺀다. 확인은 VNC 미리보기까지이고, 최종 확인은 Yocto 이미지에서 한다.

## 진행 순서

### 0. [Pi] 13-3과 같은 config로 부팅

```bash
# [WSL] ~/bspworkspace/bsp-study — Codex 구현·리뷰 통과 후
make -C examples/camera_sensor W=1 && make -C examples/camera_sensor deploy
```

```bash
# [Pi] ~ — 64GB 카드. 13-3 0단계와 같음
C=/boot/firmware/config.txt
sudo cp $C $C.bak-13-4
sudo cp ~/bsp/overlays/imx219-study.dtbo /boot/firmware/overlays-bsp-study01/
sudo sed -i 's/^camera_auto_detect=1$/camera_auto_detect=0/' $C
echo "dtoverlay=imx219-study" | sudo tee -a $C
grep -nE 'camera_auto_detect|dtoverlay' $C
sudo reboot
```

확인: **[붙여 주세요]** `camera_auto_detect=0`과 맨 끝 `dtoverlay=imx219-study`.
- 복구: `sudo cp $C.bak-13-4 $C && sudo reboot`. 부팅이 안 되면 카드 리더로 `config.txt.bak-13-4`를 `config.txt`로 복사한다.

### A1. [Pi] libcamera 인식과 사진

```bash
# [Pi] ~ — 재부팅 후
sudo modprobe -a v4l2_cci v4l2_fwnode
sudo insmod ~/bsp/modules/imx219_study.ko
lsmod | grep -E '^(imx219|imx219_study) '                   # imx219_study만
m=$(for m in /dev/media*; do media-ctl -d "$m" -p 2>/dev/null | grep -q -i unicam && echo "$m"; done | head -1)
media-ctl -d "$m" -p | grep -E 'entity.*imx219'              # imx219 10-0010
rpicam-hello --list-cameras
mkdir -p ~/bsp/cam4 && cd ~/bsp/cam4
rpicam-still -n --immediate -o still.jpg 2>&1 | grep -E 'WARN|ERROR|Sensor:|camera helper|needs to be fixed'
ls -l still.jpg
```

```bash
# [WSL] ~/bspworkspace/bsp-study
mkdir -p out/cam4 && scp pi:bsp/cam4/still.jpg pi:bsp/cam3/after.jpg out/cam4/
explorer.exe "$(wslpath -w out/cam4)"
```

확인: **[붙여 주세요]**
- `lsmod`에 `imx219_study`만 있고, 엔티티 이름이 `imx219 10-0010`
- `--list-cameras`에 `imx219 [3280x2464 10-bit RGGB]`와 모드 1개
- 로그에 `Could not create camera helper`·`needs to be fixed`가 없음. `Sensor:` 줄의 포맷이 `SBGGR10_1X10`이면 flip이 적용된 것
- `still.jpg`가 `after.jpg`(mainline)처럼 정상 색·바른 방향인지

### A2. [Pi] 실행 중 컨트롤 값

```bash
# [Pi] A1 셸 — rpicam-hello를 백그라운드로 20초 실행하고 그동안 subdev 컨트롤을 읽는다
sd=$(media-ctl -d "$m" -e "imx219 10-0010")
rpicam-hello -n -t 20000 >/dev/null 2>&1 &
sleep 3
v4l2-ctl -d "$sd" --get-ctrl camera_orientation,camera_sensor_rotation,horizontal_flip,vertical_flip
for i in $(seq 1 12); do
  echo "$i $(v4l2-ctl -d "$sd" --get-ctrl exposure,analogue_gain | tr '\n' ' ')"
  sleep 1
done                                                          # 5초쯤부터 휴대폰 플래시를 렌즈 가까이 비춘다
wait
v4l2-ctl -d "$sd" --get-ctrl horizontal_flip,vertical_flip    # 정지 후 값
```

확인: **[붙여 주세요]**
- `camera_sensor_rotation` 180, `horizontal_flip` 1, `vertical_flip` 1(libcamera가 rotation을 보고 켬)
- 플래시를 비춘 뒤 `exposure`·`analogue_gain`이 작아짐(자동 노출이 드라이버 컨트롤로 내려옴). 어두운 방에서는 처음부터 최대값(2492·230)이라 렌즈를 가려도 변화가 없다

### B. [Pi VNC] 실시간 미리보기

VNC로 Pi 데스크톱에 접속해 데스크톱 안의 터미널에서 실행한다. SSH에서 실행하면 데스크톱 표시 환경 변수가 없어 창이 뜨지 않는다.

```bash
# [Pi VNC 데스크톱 터미널] — A1처럼 imx219_study가 올라와 있는 상태
rpicam-hello -t 30000 2>&1 | grep -E 'preview window|Mode selection|Sensor:|WARN|ERROR'
```

확인: **[붙여 주세요]**
- `Made ... preview window`가 있고 `Preview window unavailable`이 없는지
- 30초 동안 VNC 화면에 바른 방향·정상 색의 움직이는 영상이 뜨는지(손을 흔들어 지연·끊김 확인)
- 선택 모드(3280x2464)와 WARN·ERROR 여부

### C. [Pi] 원복

```bash
# [Pi] ~
sudo cp /boot/firmware/config.txt.bak-13-4 /boot/firmware/config.txt
grep -nE 'camera_auto_detect|dtoverlay' /boot/firmware/config.txt
sudo reboot
# 재부팅 후
lsmod | grep -E '^(imx219|imx219_study) '                       # imx219만
rpicam-still -n --immediate -o ~/bsp/cam4/after.jpg && ls -l ~/bsp/cam4/after.jpg
```

확인: **[붙여 주세요]** `camera_auto_detect=1` 복귀, mainline `imx219`, `rpicam-still` 정상.

## 완료 기준
- [x] 6.18 크로스 빌드 `W=1` 경고 0, checkpatch 결과 기록(Codex)
- [x] 엔티티 `imx219 10-0010`, libcamera가 `imx219`로 인식하고 `camera helper`·`needs to be fixed` 오류 없음
- [x] `rpicam-still` 사진이 mainline 사진과 같은 색·방향
- [x] 실행 중 rotation 180·hflip 1·vflip 1, 밝게 하면 exposure·gain 감소
- [x] VNC 실시간 미리보기가 바른 방향·정상 색으로 나옴
- [x] 원복 후 mainline `rpicam-still` 정상

## 결과
- 0 config: `camera_auto_detect=0`(L24), 맨 끝 `dtoverlay=imx219-study`(L63), 기존 3줄 유지
- A1 인식·사진: `lsmod`에 `imx219_study`만(Used by 1). 엔티티 `imx219 10-0010`. `--list-cameras`에 `0 : imx219 [3280x2464 10-bit RGGB] (/base/soc/i2c0mux/i2c@1/imx219@10)`, 모드 `SRGGB10_CSI2P 3280x2464 [21.19 fps - (0, 0)/3280x2464 crop]` 1개(mainline 같은 모드와 fps·crop 표시 같음). `rpicam-still` 로그에 WARN·ERROR·`camera helper`·`needs to be fixed` 없음, `Selected sensor format: 3280x2464-SBGGR10_1X10` → libcamera가 rotation 180을 보고 flip을 켜고 바뀐 Bayer 코드로 설정. `still.jpg` 2,213,275 B, 사용자 확인 정상
- A2 컨트롤: 실행 중 `camera_orientation` 2(External), `camera_sensor_rotation` 180, `horizontal_flip`·`vertical_flip` 1(정지 뒤에도 값 유지, grab만 풀림). 어두운 책상에서는 12초 내내 `exposure` 2492(vblank 32에서 최대) · `analogue_gain` 230(≈9.85배, 13단계 mainline 자동 노출 게인과 같음)으로 고정 → 가려도 변화 없음. 휴대폰 플래시를 비추자 2492/230 → 1587/191 → 1051/128 → 542~932/128로 감소 → libcamera 자동 노출이 드라이버 컨트롤로 내려옴
- B VNC 미리보기: `Made wayland-egl preview window`, `Mode selection for 1640:1232` 요청에 유일한 모드 `3280x2464-SBGGR10_1X10` 선택, WARN·ERROR 없음. 30초 동안 바른 방향·정상 색 영상(사용자 확인). mainline은 같은 요청에 1640×1232 binning 모드를 고르는 점이 다름
- C 원복: `camera_auto_detect=1`, `dtoverlay` 기존 3줄. 재부팅 후 mainline `imx219`만(Used by 3), `rpicam-still` libcamera v0.7.2·`imx219.json`, mainline은 4개 모드 중 3280x2464 선택, `after.jpg` 2,201,712 B 정상(사용자 확인)
- 미검증: 모드 1개(최대 21.19fps)에서 640×480·20fps 연속 프레임 간격, 장시간 스트림, 보드 libcamera 0.7.2 소스와 읽은 main 코드의 일치(동작으로는 A1·A2에서 문제없음), 6.6.63(Yocto) 빌드
