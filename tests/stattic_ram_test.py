#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The real target oscillator loop and its callees must execute in SRAM."""
import re
import hashlib
import json
import struct
import subprocess
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

# Check the linked bytes that crt0 actually copies, not just C attributes.
symbols = subprocess.check_output(["readelf", "-sW", "build/felucca.elf"], text=True)
def symbol(name):
    m = re.search(r"^\s*\d+:\s+([0-9a-f]+)\s+(\d+)\s+.*\s" + name + r"$", symbols, re.M)
    assert m, name
    return int(m[1], 16), int(m[2])

start, _ = symbol("_data_start")
data = Path("build/data.bin").read_bytes()
# Captured from unchanged rc8 tables before relocation. Never regenerate to
# excuse altered waveform, filter or saturation values.
tables = {
    "SINE": (1025, "h", "b85305dcb6b12014dc712260916a6de587b4730c59ce9a7cec5a7cad0f28c4cc"),
    "SVF_G": (128, "H", "35af2de8c4a074be5f29598ca0a1068b0aa45fa080e3e142bbe93e95f7455da2"),
    "TANH_Q15": (257, "h", "6e75086b380b3356c1f4ea9b3252b824c8d89688ccdade5126cdec9c8057cb2f"),
}
for name, (count, fmt, expected) in tables.items():
    address, size = symbol(name)
    assert 0x1C08000 <= address < address + size <= 0x1C20000 and size == count * 2, name
    offset = address - start
    assert 0 <= offset <= len(data) - size, name
    values = list(struct.unpack_from("<" + fmt * count, data, offset))
    assert hashlib.sha256(json.dumps(values).encode()).hexdigest() == expected, name
    print(f"Target data SRAM: {name}, {size} exact bytes in the startup-copy image")
