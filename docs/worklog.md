# 다음 작업

main에서만 갱신한다(머지할 때, 브랜치를 시작·대기로 돌릴 때). 완료 상태와 학습 문서는 [roadmap](roadmap.md), 진행 중 상태는 각 작업 노트의 `상태:` 줄, 환경·배선은 [bringup](bringup.md)에 둔다.

- 18 통합 완료(2026-11-15 main 머지): [통합 문서](learning/18-integration.md). Yocto 6.6.63 카드에서 6개 장치 자동 적재, `devcheck all` 6개 PASS, HDMI DRM 미리보기·사진 확인. 64GB 카드와 IMU·마이크 값 일치, PCA9685 LED0 1카운트 차이는 6.6 올림·6.18 버림의 드라이버 차이.
- 학습 문서 정리 완료(2026-11-15 main): 06 형식 기준으로 01~18을 재작성·점검하고 [전체 지도](learning/00-overview.md)를 추가했다.
- 13-3 카메라 센서 드라이버 완료(2026-11-16 main 머지): [카메라 문서](learning/13-camera.md). 직접 작성한 고정 모드 `imx219_study`로 unicam 연결, 634라인·50.001ms·컬러바가 13-2 mainline과 같음, v4l2-compliance 45/46(컨트롤 이벤트 미지원).
- 17단계 모터 PWM은 제외했다(18단계에서 sysfs 값만 확인). 마이크 CPU DAI(I2S·DMA) 직접 작성은 선택 과제로 남긴다.
