#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""STATTIC migration acceptance checks. Hardware deadlines require a real FM-1."""
import os,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/stattic-check';OUT.mkdir(exist_ok=True,parents=True)
def run(name,args,env=None):
    with (OUT/(name+'.log')).open('w') as log:
        result=subprocess.run([str(x) for x in args],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,env={**os.environ,**(env or {})})
    if result.returncode:raise SystemExit(f'FAIL {name}: see {OUT/name}.log')
    print('PASS',name,flush=True)
    return (OUT/(name+'.log')).read_text()
def compile_test(name,source,extra=(),native=False):
    includes=(['-Ibuild/native-gen'] if native else [])+['-Ibuild/gen','-Ifirmware/src','-Ifirmware/hal']
    exe=OUT/name
    run('compile-'+name,['cc','-O2','-w','-fwrapv',*includes,*extra,source,'-lm','-lpthread','-o',exe])
    return exe
for script in ['stattic_engine_vectors','stattic_fx_vectors']:
    run(script,['python3','tests/'+script+'.py'])
for name in ['runtime','project','state','storage','ui','loader_screen']:
    exe=compile_test(name,'tests/stattic_'+name+'_test.c')
    run(name,[exe,*([OUT] if name in ['ui','loader_screen'] else [])])
exe=compile_test('mix','tests/stattic_mix_test.c')
hashes=[]
for name,env in [('one-core',{'CORES':'1'}),('two-core',{'CORES':'2'}),('chaos',{'CORES':'2','CHAOS':'7'}),('timeout',{'CORES':'2','CPU1_TIMEOUT_AT':'13'})]:
    data=run('mix-'+name,[exe],env);hashes.append(re.search(r'mix ([0-9a-f]+)',data)[1])
assert len(set(hashes))==1,hashes
run('web-c-integration',['node','tests/stattic_web_test.mjs'])
run('usb-console',['node','tests/stattic_console_test.mjs'])
for name in ['project','storage','upreset']:
    exe=compile_test('native-'+name,'tests/'+name+'_test.c',native=True)
    run('native-'+name,[exe])
print('All STATTIC acceptance checks passed; hardware audio qualification pending.',flush=True)
