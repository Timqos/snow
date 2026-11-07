#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the verified Skiff baseline, its emulator and Snow's project page."""
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
import make_site  # noqa: E402


def main():
    match = re.search(r'^#define FELUCCA_VERSION "v([^"]+)"',
                      (ROOT / "firmware/src/felucca.c").read_text(), re.MULTILINE)
    if not match:
        raise SystemExit("Cannot read the baseline firmware version")
    version = match[1]
    package = ROOT / "build/felucca.fwsc"
    site = ROOT / "build/site"
    # The upstream builder checks the package's model, loader and emulator version,
    # and includes the installer, editor, converter, samples' attribution and licences.
    make_site.main(package, version, site)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip())
    html = (ROOT / "web/snow/index.html").read_text()
    html = html.replace("@VERSION@", version).replace("@REVISION@", revision[:12])
    (site / "index.html").write_text(html)
    (site / ".nojekyll").touch()
    shutil.copy(ROOT / "docs/SNOW_SPEC.md", site / "SNOW_SPEC.md")
    metadata = {
        "project": "Snow", "stage": "Skiff baseline", "baseline_version": version,
        "source_commit": revision, "source_dirty": dirty,
        "package": f"firmware/skiff-{version}.fwsc",
        "sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
    }
    (site / "build.json").write_text(json.dumps(metadata, indent=2) + "\n")
    published = site / metadata["package"]
    (site / "firmware/SHA256SUMS").write_text(f"{metadata['sha256']}  {published.name}\n")
    # Keep original credits; direct this fork's source link to its actual repository.
    emulator = site / "try/index.html"
    emulator.write_text(emulator.read_text().replace(
        'href="https://github.com/illuminus5/skiff">Source</a>',
        'href="https://github.com/Timqos/snow">Source</a>'))
    print(f"Snow site: {site}, Skiff {version}, commit {revision[:12]}, dirty={dirty}")


if __name__ == "__main__":
    main()
