#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pin read-only console regions to the exact released firmware's ELF/hash."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--elf', type=Path, required=True)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--metadata', type=Path, required=True)
    p.add_argument('--maps', type=Path, required=True)
    a = p.parse_args()
    meta = json.loads(a.metadata.read_text())
    digest = hashlib.sha256(a.package.read_bytes()).hexdigest()
    if meta['package_sha256'] != digest or meta.get('source_dirty'):
        raise SystemExit('Console map needs the exact clean-source release package.')
    symbols = subprocess.check_output(['readelf', '-sW', str(a.elf)], text=True)

    def symbol(name, size):
        m = re.search(r'^\s*\d+:\s+([0-9a-f]+)\s+(\d+)\s+.*\s' + name + r'$', symbols, re.M)
        if not m or int(m[2]) != size:
            raise SystemExit(f'Console diagnostic layout changed: {name}')
        address = int(m[1], 16)
        if not 0x1C00000 <= address < address + size <= 0x1C80000:
            raise SystemExit(f'Console region outside RAM: {name}')
        return {'address': address, 'size': size}

    regions = {
        'late_base': symbol('snow_late_base', 4),
        'peak': symbol('snow_profile_max_ticks', 4),
        # This compiler promotes the internal boolean to a 32-bit slot.
        # Read its low byte only (the target is little endian).
        'had_late': {**symbol('snow_profile_had_late', 4), 'size': 1},
        'event': symbol('snow_profile_event', 20),
        'rendered_voices': symbol('snow_rendered_voices', 6),
        'part_cost': symbol('part_cost', 12),
    }
    dbg = symbol('felucca_dbg', 76)
    regions['late_counter'] = {'address': dbg['address'] + 20, 'size': 4}
    record = {'version': meta['version'], 'package_sha256': digest,
              'source_commit': meta['source_commit'], 'elf_sha256': hashlib.sha256(a.elf.read_bytes()).hexdigest(),
              'ticks_per_us': 24, 'half_frames': 256, 'sample_rate': 44100, 'regions': regions}
    catalog = json.loads(a.maps.read_text()) if a.maps.exists() else {'schema': 1, 'maps': []}
    old = next((m for m in catalog['maps'] if m['version'] == record['version']), None)
    if old:
        # A version string is all the existing device reports. Never silently
        # reuse it for a different binary/address layout.
        if old['package_sha256'] != digest or old['regions'] != regions:
            raise SystemExit('Firmware version reused with different bytes/regions; bump the release version.')
    else:
        catalog['maps'].append(record)
    a.maps.parent.mkdir(parents=True, exist_ok=True)
    a.maps.write_text(json.dumps(catalog, indent=2) + '\n')
    print(f"USB console map: {record['version']}, seven read-only regions, exact package {digest}")


if __name__ == '__main__':
    main()
