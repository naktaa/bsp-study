# firmware가 config.txt의 dtparam·dtoverlay를 적용해 넘긴 DT를 그대로 쓴다.
fdt addr ${fdt_addr}

# env bootargs가 있으면 fdt_chosen()이 덮어쓰므로 firmware가 만든 값으로 맞춘다.
fdt get value bootargs /chosen bootargs

load ${devtype} ${devnum}:${distro_bootpart} ${kernel_addr_r} kernel8-bsp-study01.img
load ${devtype} ${devnum}:${distro_bootpart} ${ramdisk_addr_r} initramfs8-bsp-study01

# 비압축 arm64 Image이므로 booti. initrd는 주소:크기로 넘긴다.
booti ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr}
