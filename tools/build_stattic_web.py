#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the complete STATTIC UI/DSP browser adapter, using target-generated assets."""
import hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/stattic-web'
def main():
    emcc=shutil.which('emcc')
    if not emcc:raise SystemExit('Add Emscripten to PATH before building the simulator')
    samples=ROOT/'build/gen/felucca_samples.h'
    if not samples.exists() or 'gen_stattic_drums.py' not in samples.read_text()[:120]:
        raise SystemExit('Run build.sh --stattic first: the simulator requires STATTIC sample assets')
    OUT.mkdir(parents=True,exist_ok=True)
    exports=['init','screen','encoder','button','key','frame','render','flash','resume','state','request_buffer','response_buffer','request','panic']
    subprocess.run([emcc,'firmware/host/stattic_sim.c','-O2','-fwrapv','-fno-strict-aliasing','-Wno-unused-function','-Wno-int-to-pointer-cast','-Ibuild/gen','-Ifirmware/src','--no-entry','-sSTANDALONE_WASM=1','-sFILESYSTEM=0','-sINITIAL_MEMORY=16777216','-sSTACK_SIZE=131072','-sEXPORTED_FUNCTIONS='+json.dumps(['_stattic_sim_'+x for x in exports]),'-o',str(OUT/'stattic.wasm')],cwd=ROOT,check=True)
    def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
    record={'source_commit':git('rev-parse','HEAD'),'source_dirty':bool(git('status','--porcelain')),'wasm_sha256':hashlib.sha256((OUT/'stattic.wasm').read_bytes()).hexdigest()}
    (OUT/'build.json').write_text(json.dumps(record,indent=2)+'\n')
    print(OUT/'stattic.wasm')
if __name__=='__main__':main()
