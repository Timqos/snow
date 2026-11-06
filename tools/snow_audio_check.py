#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check rendered preset audio, not merely parameter differences. Stdlib only."""
import cmath
import json
import math
import struct
import sys
import wave
from pathlib import Path


def fft(values):
    a = [complex(x) for x in values]
    n = len(a)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j ^= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    size = 2
    while size <= n:
        root = cmath.exp(-2j * math.pi / size)
        for start in range(0, n, size):
            w = 1
            for i in range(start, start + size // 2):
                u, v = a[i], a[i + size // 2] * w
                a[i], a[i + size // 2] = u + v, u - v
                w *= root
        size *= 2
    return a[:n // 2 + 1]


def main():
    root = Path(sys.argv[1])
    report = {}
    for role in ('kick', 'snare', 'hat', 'perc'):
        with wave.open(str(root / f'{role}-presets.wav'), 'rb') as wav:
            assert wav.getframerate() == 44100 and wav.getnchannels() == 1
            data = struct.unpack('<' + 'h' * wav.getnframes(), wav.readframes(wav.getnframes()))
        profiles = []
        for p in range(10):
            x = data[p * 57330:p * 57330 + 44100]
            energy = sum(v * v for v in x)
            assert energy > 100000 and max(map(abs, x)) < 32767
            acc = 0
            for end, v in enumerate(x):
                acc += v * v
                if acc >= energy * .95:
                    break
            n = 8192
            spectrum = fft([v * (.5 - .5 * math.cos(2 * math.pi * i / (n - 1))) for i, v in enumerate(x[:n])])
            power = [abs(z) ** 2 for z in spectrum]
            total = sum(power)
            centroid = sum(v * i * 44100 / n for i, v in enumerate(power)) / total
            low = sum(power[:math.ceil(1000 * n / 44100)]) / total
            peak_bin = max(power) / total
            profile = dict(preset=p + 1, energy95_ms=round(end * 1000 / 44100, 2), centroid_hz=round(centroid), below_1k=round(low, 4), strongest_bin=round(peak_bin, 4))
            if role == 'kick':
                attack = x[:882]  # first 20 ms, before the low body/tail
                profile['attack_peak'] = max(map(abs, attack))
                profile['attack_rms'] = round(math.sqrt(sum(v*v for v in attack)/len(attack)))
                assert profile['attack_peak'] > 700 and profile['attack_rms'] > 300, profile
            if role == 'hat':
                assert low < .05, profile
                assert peak_bin < .035, profile  # no dominant glass/bell tone
            profiles.append(profile)
        if role in ('kick', 'snare', 'hat'):
            for i, a in enumerate(profiles):
                for b in profiles[:i]:
                    duration = abs(math.log(a['energy95_ms'] / b['energy95_ms']))
                    colour = abs(math.log(a['centroid_hz'] / b['centroid_hz']))
                    assert duration > .12 or colour > .12, (role, a, b)
        report[role] = profiles
        print(role, '10 audible renders: bounded peaks, distinct duration/colour', flush=True)
    (root / 'preset-audio-check.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
