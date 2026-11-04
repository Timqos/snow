#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and check the Snow prototype preview after FELUCCA_SNOW=1 sh build.sh."""
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/snow-preview"
VERSION = "v0.6-snow"


def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True, exist_ok=True)
    run("cc", "-O1", "-w", "-Ibuild/gen", "-Ifirmware/src", "-o", "build/snow_test", "tests/snow_test.c", "-lm")
    run(str(ROOT / "build/snow_test"))
    run("cc", "-O1", "-w", "-Ibuild/gen", "-Ifirmware/src", "-o", "build/snow_audition", "tests/snow_audition.c", "-lm")
    run(str(ROOT / "build/snow_audition"), str(OUT))
    run("python3", "tools/snow_audio_check.py", str(OUT))
    random_audio = ROOT / "build/snow-random-audio"
    random_audio.mkdir(exist_ok=True)
    run("cc", "-O1", "-w", "-Ibuild/gen", "-Ifirmware/src", "-o", "build/snow_random_audio", "tests/snow_random_audio.c", "-lm")
    run(str(ROOT / "build/snow_random_audio"), str(random_audio))
    run("python3", "tools/snow_random_audio_check.py", str(random_audio))
    shutil.copy(random_audio / "random-audio-check.json", OUT / "random-audio-check.json")
    run("emcc", "-O2", "-ffp-contract=off", "-std=gnu99", "-w", "-DFELUCCA_SNOW=1",
        f'-DFELUCCA_VERSION="{VERSION}"', "-Ibuild/gen", "-Ifirmware/src", "--no-entry",
        "-sSTANDALONE_WASM", "-sSTACK_SIZE=1048576", "-sINITIAL_MEMORY=33554432", "-sFILESYSTEM=0",
        "-o", str(OUT / "felucca.wasm"), "web/emu/felucca_web.c")
    run("node", "tests/snow_wasm.mjs", str(OUT / "felucca.wasm"))
    package_name = "snow-0.6.fwsc"
    package_hash = hashlib.sha256((ROOT / "build/felucca.fwsc").read_bytes()).hexdigest()
    source = Path(os.environ.get("SNOW_SOURCE_DIR", ROOT))
    commit = subprocess.check_output(["git","rev-parse","HEAD"],cwd=source,text=True).strip()
    html = (ROOT / "web/emu/index.html").read_text().replace("@VERSION@", VERSION).replace("@PREVIEW@", " (experimental)")
    html = html.replace('<title>Try Skiff</title>', '<title>Snow 0.6 · FM-1 groovebox</title>')
    html = html.replace('Skiff, the six-track groovebox firmware for the M-VAVE FM-1, running in your browser: the same code the unit runs.', 'Snow for the M-VAVE FM-1: six instruments, smart sound generation, generative patterns and a browser simulator. Install, back up and learn the controls.')
    html = html.replace('"skiff-emu-flash"', '"snow-fm1-prototype-06"')
    html = html.replace('const raw = JSON.parse(localStorage.getItem(STORE) || "{}"), out = {};',
        'const current = localStorage.getItem(STORE), old05 = current ? null : localStorage.getItem("snow-fm1-prototype-05"), old04 = current || old05 ? null : localStorage.getItem("snow-fm1-prototype-04");\n    snowLegacy04 = !!old04;\n    const raw = JSON.parse(current || old05 || old04 || "{}"), out = {};')
    wasm_key = hashlib.sha256((OUT / "felucca.wasm").read_bytes()).hexdigest()[:12]
    worklet_key = hashlib.sha256((ROOT / "web/emu/worklet.js").read_bytes()).hexdigest()[:12]
    html = html.replace('"felucca.wasm"', f'"felucca.wasm?v={wasm_key}"').replace('"worklet.js"', f'"worklet.js?v={worklet_key}"')
    css = (ROOT / "web/snow/site.css").read_text()
    html = html.replace('</style>', css + "\n</style>", 1)
    (OUT / "site.css").write_text(css)
    site = (ROOT / "web/snow/site.html").read_text().replace("@VERSION@", "0.6").replace("@PACKAGE@", package_name)
    install, guide = site.split('<section class="snow-guide"', 1)
    main_start, main_end = html.index('<main>'), html.index('</main>') + len('</main>')
    device = html[html.index('<div class="device"'):html.index('<p class="keys">')]
    keys_start = html.index('<p class="keys">')
    keys = html[keys_start:html.index('</p>', keys_start) + len('</p>')]
    footer = html[html.index('<footer>'):html.index('</footer>') + len('</footer>')]
    footer = re.sub(r'<nav>.*?</nav>', '', footer, flags=re.S)
    footer = footer.replace('Skiff is not affiliated', 'Snow is not affiliated')
    heading = '<header class="snow-header"><strong>SNOW</strong><span>SIX INSTRUMENTS / M-VAVE FM-1 / 0.6</span></header>'
    html = html[:main_start] + '<main>\n' + heading + device + install + keys + '<section class="snow-guide"' + guide + footer + '\n</main>' + html[main_end:]
    send_line = 'const send = (m) => { if (node) node.port.postMessage(m); };'
    assert send_line in html
    html = html.replace(send_line, send_line + "\n" + (ROOT / "web/snow/site.js").read_text())
    html = html.replace('if (m.type === "ready") res();', 'if (m.type === "ready") { snowReady(m.snowSchema); res(); }')
    html = html.replace('if (m.fb) drawScreen(m.fb);', 'if (m.snow) snowFrame(m.snow);\n          if (m.fb) drawScreen(m.fb);')
    (OUT / "index.html").write_text(html)
    for i, body in enumerate(re.findall(r'<script[^>]*>([\s\S]*?)</script>', html)):
        if body.strip():
            module = ROOT / f"build/snow-inline-{i}.mjs"
            module.write_text(body)
            run("node", "--check", str(module))
    shutil.copy(ROOT / "web/emu/worklet.js", OUT / "worklet.js")
    shutil.copytree(ROOT / "web/emu/fonts", OUT / "fonts", dirs_exist_ok=True)
    shutil.copy(ROOT / "build/felucca.fwsc", OUT / package_name)
    installer = OUT / "install"
    installer.mkdir()
    for name in ("app.js", "core.js"):
        shutil.copy(ROOT / "web/snow/install" / name, installer / name)
    for name in ("fm1pkg.js", "fm1ota.js", "fm1backup.js"):
        shutil.copy(ROOT / "web" / name, installer / name)
    install_html = (ROOT / "web/snow/install/index.html").read_text()
    for token, value in (("@VERSION@", "0.6"), ("@PACKAGE@", package_name), ("@SHA256@", package_hash), ("@COMMIT@", commit), ("@SHORT_COMMIT@", commit[:12])):
        install_html = install_html.replace(token, value)
    (installer / "index.html").write_text(install_html)
    (installer / "config.json").write_text(json.dumps({"version":VERSION,"product":"FM-1_700","package":"../"+package_name,"sha256":package_hash,"source_commit":commit},indent=2)+"\n")
    run("node", "tests/snow_install.mjs", str(installer))
    shutil.copytree(ROOT / "LICENSES", OUT / "LICENSES", dirs_exist_ok=True)
    for name in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / name, OUT / name)
    source = Path(os.environ.get("SNOW_SOURCE_DIR", ROOT))
    metadata = {"stage":"Snow role-aware sound designer, generative groovebox and backup-first installer", "version":VERSION, "source_commit":subprocess.check_output(["git","rev-parse","HEAD"],cwd=source,text=True).strip(),
                "source_dirty":bool(subprocess.check_output(["git","status","--porcelain"],cwd=source,text=True).strip()),
                "package_sha256":package_hash}
    (OUT / "build.json").write_text(json.dumps(metadata,indent=2)+"\n")
    run("node", "tests/snow_page.mjs", str(OUT / "index.html"))
    print(f"Snow prototype: {OUT}")


if __name__ == "__main__":
    main()
