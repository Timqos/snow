#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fetch only the three checked SDK files required by the firmware packager."""
import argparse
import hashlib
import time
import urllib.error
import urllib.request
from pathlib import Path

from build import SDK_SHA256

SDK_TAG = "AC79NN_SDK_V1.2.1_2023-12-13"
SDK_URL = f"https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/{SDK_TAG}/cpu/wl82/tools/"


def fetch_sdk(destination):
    for name, expected in SDK_SHA256.items():
        target = destination / "cpu/wl82/tools" / name
        if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == expected:
            print(f"SDK {name}: checksum OK (cached)")
            continue
        for attempt in range(3):
            try:
                with urllib.request.urlopen(SDK_URL + name, timeout=60) as response:
                    content = response.read()
                break
            except (urllib.error.URLError, TimeoutError):
                if attempt == 2:
                    raise
                time.sleep(attempt + 1)
        actual = hashlib.sha256(content).hexdigest()
        if actual != expected:
            raise SystemExit(f"SDK {name}: SHA-256 mismatch: expected {expected}, got {actual}")
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_name(target.name + ".download")
        temporary.write_bytes(content)
        temporary.replace(target)
        print(f"SDK {name}: {len(content)} bytes, checksum OK")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="SDK root to populate")
    fetch_sdk(parser.parse_args().destination)
