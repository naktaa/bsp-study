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

IMAGE_INSTALL:append = " \
    kernel-modules \
    linux-firmware-rpidistro-bcm43455 \
    wireless-regdb-static \
    wpa-supplicant \
    iw \
    bsp-study-wifi \
    "

WIFI_SSID ??= ""
WIFI_PSK ??= ""

# Wi-Fi 인증 정보가 별도 패키지에 남지 않도록 이미지 후처리에서 쓴다.
ROOTFS_POSTPROCESS_COMMAND += "write_wifi_config "

python write_wifi_config () {
    import hashlib
    import os

    ssid = d.getVar("WIFI_SSID") or ""
    if not ssid:
        bb.warn("WIFI_SSID가 비어 있어 기본 Wi-Fi 설정을 유지합니다.")
        return

    passphrase = (d.getVar("WIFI_PSK") or "").encode("utf-8")
    if not 8 <= len(passphrase) <= 63:
        bb.fatal("WIFI_PSK는 UTF-8 기준 8~63바이트여야 합니다.")

    ssid_bytes = ssid.encode("utf-8")
    psk = hashlib.pbkdf2_hmac("sha1", passphrase, ssid_bytes, 4096, 32)
    config = (
        "ctrl_interface=/var/run/wpa_supplicant\n"
        "country=KR\n"
        "network={\n"
        f"\tssid={ssid_bytes.hex()}\n"
        f"\tpsk={psk.hex()}\n"
        "}\n"
    )

    path = os.path.join(d.getVar("IMAGE_ROOTFS"), "etc/wpa_supplicant.conf")
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as output:
        os.fchmod(output.fileno(), 0o600)
        output.write(config)
}
