require recipes-core/images/core-image-minimal.bb

SUMMARY = "Minimal BSP study image with Dropbear SSH access"
LICENSE = "MIT"

# 학습용 보드에 비밀번호 없이 root로 SSH 접속하기 위한 설정이다.
IMAGE_FEATURES += " \
    ssh-server-dropbear \
    empty-root-password \
    allow-empty-password \
    allow-root-login \
    "
