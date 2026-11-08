#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the STATTIC site and the exact tested firmware; no deploy or device IO."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--preview',action='store_true');ap.add_argument('--output',type=Path,default=ROOT/'build/stattic-site');a=ap.parse_args()
    dirty=bool(git('status','--porcelain'))
    if dirty and not a.preview:raise SystemExit('Commit the reviewed source before packaging a public candidate')
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    shutil.copytree(ROOT/'web/stattic',out,dirs_exist_ok=True)
    wasm=ROOT/'build/stattic-web/stattic.wasm'
    if not wasm.exists():raise SystemExit('Build tools/build_stattic_web.py first')
    shutil.copy(wasm,out/'sim/stattic.wasm')
    if a.preview:
        config={'previewOnly':True,'version':'Local preview','product':'FM-1_990'}
    else:
        trace=json.loads((ROOT/'build/stattic-build.json').read_text());pkg=ROOT/'build'/trace['package']
        web=json.loads((wasm.parent/'build.json').read_text())
        if web['source_commit']!=git('rev-parse','HEAD') or web['source_dirty']:raise SystemExit('Rebuild the browser simulator from the clean site source')
        if web['wasm_sha256']!=hashlib.sha256(wasm.read_bytes()).hexdigest():raise SystemExit('Simulator differs from its build record')
        if trace['source_commit']!=git('rev-parse','HEAD') or trace['source_dirty']:raise SystemExit('Firmware source differs from the clean site source')
        if hashlib.sha256(pkg.read_bytes()).hexdigest()!=trace['package_sha256']:raise SystemExit('Firmware checksum differs from its build record')
        (out/'firmware').mkdir(exist_ok=True);shutil.copy(pkg,out/'firmware'/pkg.name)
        config={'version':trace['version'],'product':trace['product'],'sha256':trace['package_sha256'],'package':'../firmware/'+pkg.name,'source_commit':trace['source_commit']}
        (out/'firmware/manifest.json').write_text(json.dumps(trace,indent=2)+'\n')
        (out/'sim/build.json').write_text(json.dumps(web,indent=2)+'\n')
        subprocess.run(['git','archive','--format=zip','--output='+str(out/'firmware/source.zip'),'HEAD'],cwd=ROOT,check=True)
    (out/'install/config.json').write_text(json.dumps(config,indent=2)+'\n')
    (out/'hardware-status.json').write_text(json.dumps({'version':'STATTIC 0.9.0-S1','base':'SLOOP 2.8.5','source_commit':git('rev-parse','HEAD'),'source_dirty':dirty,'hardware_audio_validation':'pending','browser_performance_is_not_device_performance':True},indent=2)+'\n')
    print(out)
if __name__=='__main__':main()
