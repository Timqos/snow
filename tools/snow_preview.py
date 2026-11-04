#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and check the Snow prototype preview after FELUCCA_SNOW=1 sh build.sh."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/snow-preview"
VERSION = "v0.1-snow"


def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    run("cc", "-O1", "-w", "-Ibuild/gen", "-Ifirmware/src", "-o", "build/snow_test", "tests/snow_test.c", "-lm")
    run(str(ROOT / "build/snow_test"))
    run("emcc", "-O2", "-ffp-contract=off", "-std=gnu99", "-w", "-DFELUCCA_SNOW=1",
        f'-DFELUCCA_VERSION="{VERSION}"', "-Ibuild/gen", "-Ifirmware/src", "--no-entry",
        "-sSTANDALONE_WASM", "-sSTACK_SIZE=1048576", "-sINITIAL_MEMORY=33554432", "-sFILESYSTEM=0",
        "-o", str(OUT / "felucca.wasm"), "web/emu/felucca_web.c")
    run("node", "tests/snow_wasm.mjs", str(OUT / "felucca.wasm"))
    html = (ROOT / "web/emu/index.html").read_text().replace("@VERSION@", VERSION).replace("@PREVIEW@", " (prototype)")
    html = html.replace('<title>Try Skiff</title>', '<title>Snow · six FM machines</title>')
    html = html.replace('"skiff-emu-flash"', '"snow-fm1-prototype-01"')
    toolbar = '''<section style="padding:16px;border:1px solid #665936;background:#191b17;color:#f1dda0;margin-bottom:16px">
<strong>SNOW 0.1 · FM PROTOTYPE</strong>
<p>6 FM machines · no samples · parameter locks · Control All · temporary Save / Reload.<br>
Independent delay/reverb chains and the final pixel interface are still in development.</p>
<p><button id="snow-demo" disabled>Load demo pattern</button> <button id="snow-save" disabled>Temp Save</button>
<button id="snow-reload" disabled>Reload all</button> <button id="snow-reload-track" disabled>Reload track</button></p>
<p id="snow-tracks">''' + " ".join(f'<button data-track="{k}" disabled>{name}</button>' for k, name in enumerate(("KICK","SNARE","HAT","PERC","BASS","CHORD"))) + '''</p>
<label>Include in Control All: <span id="snow-mask">''' + " ".join(f'<label><input type="checkbox" data-mask="{k}" checked>{name}</label>' for k, name in enumerate(("KIK","SNR","HAT","PRC","BAS","CHD"))) + '''</span></label>
<p>Switch on → Load demo pattern → PLAY. Loading the demo replaces the current pattern.<br>
Hold GLO + K1–K4: Control All. GLO + SAVE: Temp Save. GLO + HOME: Reload.<br>
GLO + OCT−: exclude selected track; GLO + OCT+: reload selected track.<br>
Drag knobs vertically. Shift-click GLO keeps it held. <a href="../try/">Skiff baseline</a>.</p></section>'''
    html = html.replace('<main>', '<main>\n' + toolbar, 1)
    html = html.replace('<a href="../webapp/installer/">Install</a>', '<a href="snow-0.1.fwsc">Prototype firmware</a>')
    html = html.replace('href="https://github.com/illuminus5/skiff">Source', 'href="https://github.com/Timqos/snow/tree/codex/snow-machines">Source')
    setup = '''
let snowSelected = 0;
document.getElementById("snow-demo").onclick = () => send({type:"snow-demo"});
document.getElementById("snow-save").onclick = () => send({type:"snow-save"});
document.getElementById("snow-reload").onclick = () => send({type:"snow-reload",mask:63});
document.getElementById("snow-reload-track").onclick = () => send({type:"snow-reload",mask:1<<snowSelected});
for (const b of document.querySelectorAll("[data-track]")) b.onclick = () => { snowSelected=+b.dataset.track; send({type:"snow-track",track:snowSelected}); };
for (const b of document.querySelectorAll("[data-mask]")) b.onchange = () => { let mask=0; for(const c of document.querySelectorAll("[data-mask]")) if(c.checked) mask|=1<<+c.dataset.mask; send({type:"snow-mask",mask}); };
'''
    html = html.replace('const send = (m) => { if (node) node.port.postMessage(m); };', 'const send = (m) => { if (node) node.port.postMessage(m); };\n' + setup)
    html = html.replace('go.textContent = "On";', 'go.textContent = "On";\nfor (const b of document.querySelectorAll("#snow-tracks button, #snow-demo, #snow-save, #snow-reload, #snow-reload-track")) b.disabled = false;')
    html = html.replace('if (m.fb) drawScreen(m.fb);', '''if (m.snow) {
            snowSelected=m.snow.selected;
            for(const b of document.querySelectorAll("[data-track]")) b.style.background=+b.dataset.track===snowSelected?"#ecbd60":"";
            for(const b of document.querySelectorAll("[data-mask]")) b.checked=!!(m.snow.mask & (1<<+b.dataset.mask));
          }
          if (m.fb) drawScreen(m.fb);''')
    (OUT / "index.html").write_text(html)
    shutil.copy(ROOT / "web/emu/worklet.js", OUT / "worklet.js")
    shutil.copytree(ROOT / "web/emu/fonts", OUT / "fonts", dirs_exist_ok=True)
    shutil.copy(ROOT / "build/felucca.fwsc", OUT / "snow-0.1.fwsc")
    shutil.copytree(ROOT / "LICENSES", OUT / "LICENSES", dirs_exist_ok=True)
    for name in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / name, OUT / name)
    source = Path(os.environ.get("SNOW_SOURCE_DIR", ROOT))
    metadata = {"stage":"Snow FM prototype", "version":VERSION, "source_commit":subprocess.check_output(["git","rev-parse","HEAD"],cwd=source,text=True).strip(),
                "source_dirty":bool(subprocess.check_output(["git","status","--porcelain"],cwd=source,text=True).strip()),
                "package_sha256":hashlib.sha256((OUT / "snow-0.1.fwsc").read_bytes()).hexdigest()}
    (OUT / "build.json").write_text(json.dumps(metadata,indent=2)+"\n")
    print(f"Snow prototype: {OUT}")


if __name__ == "__main__":
    main()
