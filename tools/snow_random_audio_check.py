#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure actual generated audio; this is not a subjective listening test."""
import hashlib
import json
import math
import struct
import sys
from pathlib import Path
from snow_audio_check import fft


def main():
    root = Path(sys.argv[1])
    report = {}
    for k, role in enumerate(('kick', 'snare', 'hat', 'perc', 'bass', 'chord')):
        data = (root / f'random-{k}.pcm').read_bytes()
        profiles, hashes = [], set()
        for p in range(64):
            raw = data[p * 65536:(p + 1) * 65536]
            hashes.add(hashlib.sha256(raw).digest())
            x = struct.unpack('<32768h', raw)
            peak = max(map(abs, x))
            energy = sum(v * v for v in x)
            assert peak < 32767 and energy > 100000, (role, p, peak, energy)
            # A very short kick may already be silent at 46 ms. Analyse its
            # body while it exists, rather than integer rounding in the tail.
            accumulated, end95 = 0, 0
            for end95, value in enumerate(x):
                accumulated += value * value
                if accumulated >= energy * .95:
                    break
            start = min(2048, end95 // 4) if k == 0 else 4096 if k >= 4 else 0
            n = 8192
            power = [abs(v) ** 2 for v in fft([x[start+i] * (.5-.5*math.cos(2*math.pi*i/(n-1))) for i in range(n)])]
            total = sum(power)
            share = lambda hz: sum(power[:math.ceil(hz*n/44100)]) / total
            profile = dict(patch=p+1, peak=peak, energy95_ms=end95/44.1, below_200=share(200), below_1k=share(1000), strongest_bin=max(power)/total)
            if role == 'kick':
                assert profile['below_200'] > .75, (role, profile)
            if role == 'hat':
                assert profile['below_1k'] < .05 and profile['strongest_bin'] < .035, (role, profile)
            if role == 'bass':
                assert profile['below_200'] > .25, (role, profile)
            profiles.append(profile)
        assert len(hashes) == 64, (role, 'repeated audio')
        report[role] = profiles
        print(f'{role}: 64 distinct dry audio renders, bounded peaks and instrument spectrum checks passed', flush=True)
    (root / 'random-audio-check.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
