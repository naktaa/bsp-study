# 다음 작업

main에서만 갱신한다(머지할 때, 브랜치를 시작·대기로 돌릴 때). 완료 상태와 학습 문서는 [roadmap](roadmap.md), 진행 중 상태는 각 작업 노트의 `상태:` 줄, 환경·배선은 [bringup](bringup.md)에 둔다.

- 12단계 Yocto 최소 이미지(진행, 데스크톱): 브랜치 `feat/yocto-minimal`, 노트 `docs/tasks/12-yocto-minimal.md`(브랜치에 있음) 인수인계부터 이어 간다.
- 17단계 내장 PWM(계획, 노트북, 부품 불필요): 브랜치 `feat/motor-pwm`. 업스트림 `pwm-bcm2835` 신호 확인만 하고 직접 작성은 없다.
- 15단계 PCA9685(대기, 보드 도착 후): 브랜치 `feat/pca9685`, 노트 `docs/tasks/15-pca9685.md`(브랜치에 있음). 15-A 업스트림 레지스터 확인 → 15-B provider 직접 작성·비교.
- 이후 18단계에서 통합한다. 마이크 CPU DAI(I2S·DMA) 직접 작성은 선택 과제로 남긴다.
