# 다음 작업

main에서만 갱신한다(머지할 때, 브랜치를 시작·대기로 돌릴 때). 완료 상태와 학습 문서는 [roadmap](roadmap.md), 진행 중 상태는 각 작업 노트의 `상태:` 줄, 환경·배선은 [bringup](bringup.md)에 둔다.

- 12단계 Yocto 최소 이미지 완료(2026-11-13 main 머지): [Yocto 문서](learning/12-yocto-minimal.md). Wi-Fi SSH까지 확인, 커널 6.6.63. 64GB 카드 원복은 확인 생략.
- 17단계 내장 PWM(계획, 노트북, 부품 불필요): 브랜치 `feat/motor-pwm`. 업스트림 `pwm-bcm2835` 신호 확인만 하고 직접 작성은 없다.
- 15단계 PCA9685 완료(2026-11-14 main 머지): [PCA9685 문서](learning/15-pca9685.md). 업스트림과 직접 작성한 `pca9685_simple`의 레지스터 값 일치, 서보 실구동 미검증. 18단계 Yocto에는 업스트림 `pwm-pca9685`를 쓴다.
- 이후 18단계에서 통합한다. 커널은 12단계 Yocto 기본 커널 6.6.63에 맞추고, 직접 작성한 드라이버·overlay를 모듈 레시피로 다시 빌드한다. 마이크 CPU DAI(I2S·DMA) 직접 작성은 선택 과제로 남긴다.
