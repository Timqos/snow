#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The real target oscillator loop and its callees must execute in SRAM."""
import re
from pathlib import Path

listing = Path("build/felucca.dis").read_text()
for name in ("snow_wave", "snow_render", "snow_amp"):
    body = re.split(r"^" + name + r":\s*$", listing, flags=re.M)[1]
    body = re.split(r"^[A-Za-z_]\w*:\s*$", body, flags=re.M)[0]
    instructions = re.findall(r"^\s*([0-9a-f]+):\s+.*", body, re.M)
    assert instructions and all(0x1C00000 <= int(a, 16) < 0x1C06000 for a in instructions), name
    for line in body.splitlines():
        if re.search(r"\bcall\b", line):
            target = re.search(r"<([^:]+): ([0-9a-f]+) >", line)
            assert target and 0x1C00000 <= int(target[2], 16) < 0x1C06000, line
    print(f"Target SRAM contract: {name} and every compiled call remain in fast RAM")
