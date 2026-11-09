#!/usr/bin/env python3
"""기존 MPU6050 IIO 드라이버에서 직접 읽는 라이브 축 확인 도구."""

from collections import deque
from contextlib import contextmanager
import math
import os
from pathlib import Path
import struct
import sys
import time

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import RadioButtons


CHANNELS = (
    "in_accel_x", "in_accel_y", "in_accel_z",
    "in_anglvel_x", "in_anglvel_y", "in_anglvel_z", "in_timestamp",
)
SCAN_BYTES = 24
BUFFER_LENGTH = 128
WINDOW_SECONDS = 10


def find_device():
    devices = [path for path in Path("/sys/bus/iio/devices").glob("iio:device*")
               if (path / "name").read_text().strip() == "mpu6050-iio-study"]
    if len(devices) != 1:
        raise ValueError("expected one mpu6050-iio-study device; load module and overlay first")
    return devices[0]


@contextmanager
def capture(device):
    # 다른 수집 앱이 켜 둔 버퍼를 끄거나 함께 소비하지 않는다.
    if (device / "buffer/enable").read_text().strip() != "0":
        raise ValueError("IIO buffer is active; stop the C app and disable its buffer first")
    if (device / "current_timestamp_clock").read_text().strip() != "monotonic":
        raise ValueError("expected monotonic IRQ timestamps")
    for index, channel in enumerate(CHANNELS):
        base = device / "scan_elements"
        expected = "le:s64/64>>0" if channel == "in_timestamp" else "be:s16/16>>0"
        if (base / f"{channel}_index").read_text().strip() != str(index) or \
                (base / f"{channel}_type").read_text().strip() != expected:
            raise ValueError(f"unexpected scan layout: {channel}")

    scales = [float((device / name).read_text()) for name in
              ("in_accel_scale", "in_anglvel_scale")]
    if not all(math.isfinite(scale) and scale > 0 for scale in scales):
        raise ValueError("invalid IIO scale")
    for channel in CHANNELS:
        (device / f"scan_elements/{channel}_en").write_text("1\n")
    (device / "buffer/length").write_text(f"{BUFFER_LENGTH}\n")
    (device / "buffer/watermark").write_text("1\n")

    fd = os.open(f"/dev/{device.name}", os.O_RDONLY | os.O_NONBLOCK)
    try:
        # 장치 파일을 먼저 연 뒤 IRQ 수집을 시작한다.
        (device / "buffer/enable").write_text("1\n")
        print(f"[IIO] live: /dev/{device.name}; close the window to stop")
        yield fd, scales
    finally:
        try:
            (device / "buffer/enable").write_text("0\n")
        finally:
            os.close(fd)
        print("[IIO] buffer stopped; device closed")


def decode_scans(raw, scales):
    if not raw or len(raw) % SCAN_BYTES:
        raise ValueError("device removed or incomplete 24-byte IIO scan")
    for offset in range(0, len(raw), SCAN_BYTES):
        axes = struct.unpack_from(">6h", raw, offset)
        timestamp = struct.unpack_from("<q", raw, offset + 16)[0]
        # 기존 C 앱과 같은 scale을 적용하며 CSV나 C 앱은 거치지 않는다.
        values = tuple(value * scales[index // 3] for index, value in enumerate(axes))
        yield timestamp, values


def main():
    if len(sys.argv) != 1:
        raise ValueError("usage: python3 imu_plot.py (no CSV argument)")
    device = find_device()
    samples = deque(maxlen=2000)
    first_timestamp = None
    previous_timestamp = None
    last_received = time.monotonic()
    failed = False
    mode = "Gyroscope"

    fig, ax = plt.subplots(figsize=(10, 5))
    fig.subplots_adjust(left=0.10, right=0.77, bottom=0.15)
    buttons = RadioButtons(fig.add_axes([0.80, 0.68, 0.19, 0.18]),
                           ("Gyroscope", "Accelerometer"))
    lines = [ax.plot([], [], color=color, label=axis)[0]
             for axis, color in zip("XYZ", ("tab:red", "tab:green", "tab:blue"))]
    ax.set_xlabel("Elapsed IRQ time (s)")
    ax.grid(True)
    ax.legend(loc="upper left")
    status = fig.text(0.10, 0.03, "Waiting for live IIO samples...")

    def select(label):
        nonlocal mode
        mode = label
        if mode == "Gyroscope":
            ax.set_title("Angular velocity: XYZ")
            ax.set_ylabel("Angular velocity (deg/s)")
            ax.set_ylim(-260, 260)
        else:
            ax.set_title("Acceleration: XYZ (includes gravity)")
            ax.set_ylabel("Acceleration (g)")
            ax.set_ylim(-2.1, 2.1)
        fig.canvas.draw_idle()

    buttons.on_clicked(select)
    select(mode)

    try:
        with capture(device) as (fd, scales):
            last_received = time.monotonic()

            def update(_frame):
                nonlocal first_timestamp, previous_timestamp, last_received, failed
                try:
                    try:
                        # 최대 한 버퍼 분량을 읽고 GUI 이벤트 처리로 돌아간다.
                        raw = os.read(fd, SCAN_BYTES * BUFFER_LENGTH)
                    except BlockingIOError:
                        if time.monotonic() - last_received > 5:
                            raise TimeoutError("no IIO samples for 5 seconds")
                        return
                    for timestamp, values in decode_scans(raw, scales):
                        if timestamp <= 0 or (previous_timestamp is not None and
                                              timestamp <= previous_timestamp):
                            raise ValueError("IRQ timestamp did not advance")
                        if first_timestamp is None:
                            first_timestamp = timestamp
                        elapsed = (timestamp - first_timestamp) / 1_000_000_000
                        samples.append((elapsed, values))
                        previous_timestamp = timestamp
                    last_received = time.monotonic()
                    newest = samples[-1][0]
                    while samples[0][0] < newest - WINDOW_SECONDS:
                        samples.popleft()
                    times = [sample[0] for sample in samples]
                    offset, factor = ((3, 180 / math.pi) if mode == "Gyroscope"
                                      else (0, 1 / 9.80665))
                    for axis, line in enumerate(lines):
                        line.set_data(times, [sample[1][offset + axis] * factor
                                              for sample in samples])
                    ax.set_xlim(max(0, newest - WINDOW_SECONDS), max(WINDOW_SECONDS, newest))
                    status.set_text(f"Live IRQ time: {newest:.3f} s | Close window to stop")
                except (OSError, ValueError) as error:
                    failed = True
                    print(f"[IIO] {error}", file=sys.stderr)
                    plt.close(fig)

            # 센서는 공칭100Hz로 갱신되고 화면은 10Hz로 묶어서 그린다.
            animation = FuncAnimation(fig, update, interval=100, cache_frame_data=False)
            plt.show()
            del animation
    finally:
        plt.close(fig)
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
    except (OSError, ValueError) as error:
        print(f"[IIO] {error}", file=sys.stderr)
        sys.exit(1)
