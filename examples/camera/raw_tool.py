#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
import argparse

import numpy as np
from PIL import Image


def read_raw(path, width, height, stride):
    if width <= 0 or height <= 0 or width % 2 or height % 2:
        raise ValueError("width and height must be positive even numbers")
    if stride < width * 2:
        raise ValueError("stride must be at least width * 2")

    data = np.fromfile(path, dtype=np.uint8)
    if data.size < height * stride:
        raise ValueError("RAW file is shorter than height * stride")

    rows = data[:height * stride].reshape(height, stride)
    raw = rows[:, :width * 2].copy().view("<u2").reshape(height, width)
    if np.any(raw > 1023):
        raise ValueError("RAW values must be in the range 0..1023")
    return raw


def statistics(raw):
    corrected = np.maximum(raw.astype(np.float32) - 64, 0)
    channels = (corrected[0::2, 0::2], corrected[0::2, 1::2],
                corrected[1::2, 0::2], corrected[1::2, 1::2])
    print(" ".join(f"{name}={channel.mean():.3f}"
                   for name, channel in zip(("R", "Gr", "Gb", "B"), channels)),
          f"mean={corrected.mean():.3f}")
    print(f"saturation_percent={np.count_nonzero(raw == 1023) / raw.size * 100:.6f}")


def save_png(raw, path):
    raw = raw.astype(np.float32)
    red = raw[0::2, 0::2]
    green = (raw[0::2, 1::2] + raw[1::2, 0::2]) / 2
    blue = raw[1::2, 1::2]
    rgb = np.maximum(np.stack((red, green, blue), axis=-1) - 64, 0)
    rgb *= 255 / (1023 - 64)
    Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).save(path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("raw")
    parser.add_argument("width", type=int)
    parser.add_argument("height", type=int)
    parser.add_argument("--stride", type=int, required=True)
    parser.add_argument("--png")
    args = parser.parse_args()

    try:
        image = read_raw(args.raw, args.width, args.height, args.stride)
        statistics(image)
        if args.png:
            save_png(image, args.png)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")
