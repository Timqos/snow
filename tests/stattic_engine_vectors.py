#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Frozen old STATTIC vs its native SLOOP engine adapter; no golden refresh."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PIN='b0cb3a5'

def run(cmd,**kw):
    return subprocess.run(cmd,check=True,text=True,capture_output=True,**kw).stdout

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--reference-tree',type=Path)
    a=ap.parse_args()
    out=ROOT/'build/stattic-engine-vectors';out.mkdir(parents=True,exist_ok=True)
    cc=['cc','-O2','-w','-I'+str(ROOT/'build/gen'),'-I'+str(ROOT/'firmware/src')]
    source=ROOT/'tests/stattic_engine_vectors.c'
    fixture=ROOT/'tests/fixtures/stattic-engine-vectors.json'
    if a.reference_tree:
        ref=a.reference_tree.resolve()
        head=run(['git','-C',str(ref),'rev-parse','HEAD']).strip()
        if not head.startswith(PIN) or run(['git','-C',str(ref),'diff','HEAD','--','firmware/src','web/emu']).strip():
            raise SystemExit('Reference must be the unmodified pinned STATTIC b0cb3a5 source')
        ref_exe=out/'reference'
        run(['cc','-O2','-w','-I'+str(ref/'build/gen'),
             '-DSTATTIC_REFERENCE_SOURCE="'+str(ref/'web/emu/felucca_web.c')+'"',
             str(source),'-lm','-o',str(ref_exe)])
        expected=run([str(ref_exe)]).splitlines()
        provenance={
            'source':'https://github.com/Timqos/snow', 'commit':head,
            'files':{str(p):hashlib.sha256((ref/p).read_bytes()).hexdigest() for p in
                     map(Path,['firmware/src/eng_snow.c','firmware/src/snow_presets.h','firmware/src/dsp.c','firmware/src/voice.c'])},
            'vectors':expected,
        }
        if fixture.exists() and json.loads(fixture.read_text())!=provenance:
            raise SystemExit('Frozen reference differs; refusing to overwrite it')
        fixture.parent.mkdir(parents=True,exist_ok=True)
        fixture.write_text(json.dumps(provenance,indent=2)+'\n')
    expected=json.loads(fixture.read_text())['vectors']
    exe=out/'sloop-adapter'
    run([*cc,str(source),'-lm','-lpthread','-o',str(exe)])
    actual=run([str(exe)]).splitlines()
    # Role 5 (SY and CH) plays the KEYS voice since the owner's 2026-11-08 decision: the old
    # three-operator chord voice is no longer the reference there. Roles 0..4 stay frozen.
    keys=lambda line:line.split()[0]=='5'
    if len(expected)!=len(actual) or [keys(x) for x in expected]!=[keys(y) for y in actual]:
        raise SystemExit(f'vector lists differ: lengths {len(expected)}/{len(actual)}')
    diffs=[(x,y) for x,y in zip(expected,actual) if x!=y and not keys(x)]
    if diffs:
        for x,y in diffs[:12]:print('old:',x,'\nnew:',y)
        raise SystemExit(f'{len(diffs)} different vectors outside KEYS')
    k=[y.split() for y in actual if keys(y)]
    silent=[y for y in k if not 0<int(y[4])<=32767]
    if silent:raise SystemExit(f'KEYS renders silent or out of range: {silent[:3]}')
    print(f'{len(expected)-len(k)} exact STATTIC renders and terminal states (KICK..BASS) on the SLOOP API; '
          f'{len(k)} KEYS renders sounding, peak {min(int(y[4]) for y in k)}..{max(int(y[4]) for y in k)}')

if __name__=='__main__':main()
