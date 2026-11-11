#!/usr/bin/env python3
"""stdin의 리틀엔디언 int32 모노 샘플을 파형과 RMS로 표시한다."""

import argparse
from collections import deque
import math
import os
import struct
import sys
import threading

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation


def positive_int(text):
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return value


def positive_float(text):
    value = float(text)
    if not math.isfinite(value) or value <= 0:
        raise argparse.ArgumentTypeError("must be finite and positive")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rate", type=positive_int, default=48000)
    parser.add_argument("--window-ms", type=positive_float, default=50)
    parser.add_argument("--history-s", type=positive_float, default=10)
    args = parser.parse_args()

    waveform = deque(maxlen=max(1, int(args.rate * args.window_ms / 1000)))
    levels = deque()
    lock = threading.Lock()
    finished = threading.Event()
    stopped = threading.Event()
    errors = []

    def read_samples():
        pending = b""
        total_frames = 0
        try:
            while not stopped.is_set():
                raw = os.read(sys.stdin.fileno(), 4096)
                if not raw:
                    if pending:
                        raise ValueError("incomplete int32 sample at EOF")
                    break

                raw = pending + raw
                usable = len(raw) - len(raw) % 4
                pending = raw[usable:]
                if not usable:
                    continue
                values = [sample[0] for sample in struct.iter_unpack("<i", raw[:usable])]
                rms = math.sqrt(sum(value * value for value in values) / len(values))
                total_frames += len(values)
                elapsed = total_frames / args.rate

                with lock:
                    waveform.extend(values)
                    levels.append((elapsed, rms))
                    while levels and levels[0][0] < elapsed - args.history_s:
                        levels.popleft()
        except (OSError, ValueError) as error:
            errors.append(str(error))
            print(f"[PLOT] {error}", file=sys.stderr)
        finally:
            finished.set()

    fig, (wave_ax, level_ax) = plt.subplots(2, 1, figsize=(10, 6))
    fig.tight_layout(pad=3)
    wave_line, = wave_ax.plot([], [], color="tab:blue")
    wave_ax.set(title="Microphone waveform", xlabel="Time from latest sample (ms)",
                ylabel="Sample", xlim=(-args.window_ms, 0), ylim=(-8388608, 8388608))
    wave_ax.grid(True)
    level_line, = level_ax.plot([], [], color="tab:orange")
    level_ax.set(title="Block RMS", xlabel="Time from latest block (s)",
                 ylabel="RMS", xlim=(-args.history_s, 0), ylim=(0, 8388608))
    level_ax.grid(True)

    def update(_frame):
        at_eof = finished.is_set()
        with lock:
            values = list(waveform)
            history = list(levels)

        if values:
            times = [(index - len(values) + 1) * 1000 / args.rate
                     for index in range(len(values))]
            wave_line.set_data(times, values)
        if history:
            newest = history[-1][0]
            level_line.set_data([stamp - newest for stamp, _ in history],
                                [rms for _, rms in history])
            level_ax.set_ylim(0, max(1, max(rms for _, rms in history) * 1.1))
        if at_eof:
            animation.event_source.stop()
        return wave_line, level_line

    fig.canvas.mpl_connect("close_event", lambda _event: stopped.set())
    animation = FuncAnimation(fig, update, init_func=lambda: (wave_line, level_line),
                              interval=50, cache_frame_data=False)
    # 창을 닫으면 stdin 대기 중인 스레드가 프로세스 종료를 막지 않는다.
    reader = threading.Thread(target=read_samples, daemon=True)
    reader.start()
    try:
        plt.show()
    finally:
        stopped.set()
        plt.close(fig)
    return 1 if errors else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
    except (OSError, ValueError) as error:
        print(f"[PLOT] {error}", file=sys.stderr)
        sys.exit(1)
