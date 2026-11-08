#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Frozen STATTIC rack outputs AND complete terminal histories; not a CPU test."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(args):return subprocess.check_output(args,cwd=ROOT,text=True)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--reference-tree',type=Path);a=ap.parse_args()
    out=ROOT/'build/host';out.mkdir(exist_ok=True,parents=True);source=ROOT/'tests/stattic_fx_vectors.c';fixture=ROOT/'tests/fixtures/stattic-fx-vectors.json'
    if a.reference_tree:
        ref=a.reference_tree.resolve();head=run(['git','-C',str(ref),'rev-parse','HEAD']).strip()
        if not head.startswith('b0cb3a5') or run(['git','-C',str(ref),'diff','HEAD','--','firmware/src','web/emu']).strip():raise SystemExit('Unmodified old STATTIC b0cb3a5 required')
        exe=out/'stattic-fx-old';run(['cc','-O2','-w','-fwrapv','-I'+str(ref/'build/gen'),'-DSTATTIC_REFERENCE_SOURCE="'+str(ref/'web/emu/felucca_web.c')+'"',str(source),'-lm','-o',str(exe)])
        record={'commit':head,'snow_fx_sha256':hashlib.sha256((ref/'firmware/src/snow_fx.c').read_bytes()).hexdigest(),'vectors':run([str(exe)]).splitlines()}
        if fixture.exists() and json.loads(fixture.read_text())!=record:raise SystemExit('Frozen rack vectors differ; refusing overwrite')
        fixture.write_text(json.dumps(record,indent=2)+'\n')
    expected=json.loads(fixture.read_text())['vectors'];exe=out/'stattic-fx-new'
    run(['cc','-O2','-w','-fwrapv','-Ibuild/gen','-Ifirmware/src',str(source),'-lm','-lpthread','-o',str(exe)])
    actual=run([str(exe)]).splitlines()
    if actual!=expected:
        for old,new in zip(expected,actual):
            if old!=new:print('old:',old,'new:',new)
        raise SystemExit('STATTIC rack mismatch')
    print(f'{len(expected)} frozen STATTIC rack scenes: exact output and all history bytes, with automation/bypass/fragmented blocks')
if __name__=='__main__':main()
