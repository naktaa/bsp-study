# 다음 작업

main에서만 갱신한다(머지할 때, 브랜치를 시작·대기로 돌릴 때). 완료 상태와 학습 문서는 [roadmap](roadmap.md), 진행 중 상태는 각 작업 노트의 `상태:` 줄, 환경·배선은 [bringup](bringup.md)에 둔다.

- 18 통합 완료(2026-11-15 main 머지): [통합 문서](learning/18-integration.md). Yocto 6.6.63 카드에서 6개 장치 자동 적재, `devcheck all` 6개 PASS, HDMI DRM 미리보기·사진 확인. 64GB 카드와 IMU·마이크 값 일치, PCA9685 LED0 1카운트 차이(6.6 소스 확인 필요).
- 17단계 내장 PWM(계획, 노트북, 부품 불필요): 브랜치 `feat/motor-pwm`. 업스트림 `pwm-bcm2835` 신호 확인만 하고 직접 작성은 없다.
- 15단계 PCA9685 완료(2026-11-14 main 머지): [PCA9685 문서](learning/15-pca9685.md). 업스트림과 직접 작성한 `pca9685_simple`의 레지스터 값 일치, 서보 실구동 미검증. 18단계 Yocto에는 업스트림 `pwm-pca9685`를 쓴다.
- 17단계가 끝나면 18단계 이미지의 모터 PWM 값을 17단계 결과와 비교한다. 마이크 CPU DAI(I2S·DMA) 직접 작성은 선택 과제로 남긴다.
