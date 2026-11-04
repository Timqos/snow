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
VERSION = "v0.3-snow"


def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True, exist_ok=True)
    run("cc", "-O1", "-w", "-Ibuild/gen", "-Ifirmware/src", "-o", "build/snow_test", "tests/snow_test.c", "-lm")
    run(str(ROOT / "build/snow_test"))
    run("emcc", "-O2", "-ffp-contract=off", "-std=gnu99", "-w", "-DFELUCCA_SNOW=1",
        f'-DFELUCCA_VERSION="{VERSION}"', "-Ibuild/gen", "-Ifirmware/src", "--no-entry",
        "-sSTANDALONE_WASM", "-sSTACK_SIZE=1048576", "-sINITIAL_MEMORY=33554432", "-sFILESYSTEM=0",
        "-o", str(OUT / "felucca.wasm"), "web/emu/felucca_web.c")
    run("node", "tests/snow_wasm.mjs", str(OUT / "felucca.wasm"))
    html = (ROOT / "web/emu/index.html").read_text().replace("@VERSION@", VERSION).replace("@PREVIEW@", " (prototype)")
    html = html.replace('<title>Try Skiff</title>', '<title>Snow 0.3 · sound laboratory</title>')
    html = html.replace('"skiff-emu-flash"', '"snow-fm1-prototype-03"')
    wasm_key = hashlib.sha256((OUT / "felucca.wasm").read_bytes()).hexdigest()[:12]
    worklet_key = hashlib.sha256((ROOT / "web/emu/worklet.js").read_bytes()).hexdigest()[:12]
    html = html.replace('"felucca.wasm"', f'"felucca.wasm?v={wasm_key}"').replace('"worklet.js"', f'"worklet.js?v={worklet_key}"')
    html = html.replace('</style>', (ROOT / "web/snow/lab.css").read_text() + "\n</style>", 1)
    html = html.replace('<main>', '<main>\n' + (ROOT / "web/snow/lab.html").read_text(), 1)
    html = html.replace('<a href="../webapp/installer/">Install</a>', '<a href="snow-0.3.fwsc">Prototype firmware</a>')
    html = html.replace('href="https://github.com/illuminus5/skiff">Source', 'href="https://github.com/Timqos/snow/tree/codex/snow-deep-synthesis">Source')
    send_line = 'const send = (m) => { if (node) node.port.postMessage(m); };'
    assert send_line in html
    html = html.replace(send_line, send_line + "\n" + (ROOT / "web/snow/lab.js").read_text())
    html = html.replace('if (m.type === "ready") res();', 'if (m.type === "ready") { snowReady(m.snowSchema); res(); }\n        else if (m.type === "snow-schema") snowDesigner(m.schema);\n        else if (m.type === "snow-notice") snowById("snow-notice").textContent=m.text;')
    html = html.replace('if (m.fb) drawScreen(m.fb);', 'if (m.snow) snowFrame(m.snow);\n          if (m.fb) drawScreen(m.fb);')
    (OUT / "index.html").write_text(html)
    shutil.copy(ROOT / "web/emu/worklet.js", OUT / "worklet.js")
    shutil.copytree(ROOT / "web/emu/fonts", OUT / "fonts", dirs_exist_ok=True)
    shutil.copy(ROOT / "build/felucca.fwsc", OUT / "snow-0.3.fwsc")
    shutil.copytree(ROOT / "LICENSES", OUT / "LICENSES", dirs_exist_ok=True)
    for name in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / name, OUT / name)
    source = Path(os.environ.get("SNOW_SOURCE_DIR", ROOT))
    metadata = {"stage":"Snow fixed instruments, channel FX and style generator", "version":VERSION, "source_commit":subprocess.check_output(["git","rev-parse","HEAD"],cwd=source,text=True).strip(),
                "source_dirty":bool(subprocess.check_output(["git","status","--porcelain"],cwd=source,text=True).strip()),
                "package_sha256":hashlib.sha256((OUT / "snow-0.3.fwsc").read_bytes()).hexdigest()}
    (OUT / "build.json").write_text(json.dumps(metadata,indent=2)+"\n")
    print(f"Snow prototype: {OUT}")


if __name__ == "__main__":
    main()
