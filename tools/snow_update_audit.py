#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read packages and exercise simulated NOR/MIDI only. Never opens a device.

Software evidence is deliberately separate from physical FM-1 qualification.
Vendor firmware is supplied locally and hash-checked, never redistributed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import urllib.request

import fm1_install as pkg

ROOT = Path(__file__).resolve().parents[1]
BASE = "815358c56ce9307ed1cb4085351cd0fd94c15ee4"
CRITICAL = ["firmware/hal", "firmware/loader", "firmware/crt0.S", "firmware/app.ld",
            "firmware/src/ota.c", "firmware/src/ota_hw.c", "firmware/src/storage.c",
            "firmware/src/storage_hw.c", "firmware/src/usb.c", "tools/fm1pkg_make.py",
            "tools/fm1_install.py", "web/fm1ota.js", "web/fm1pkg.js", "web/fm1backup.js"]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def image(path):
    raw = path.read_bytes()
    logical = pkg.deinterleave(raw)
    assert pkg.files_ok(logical), f"Invalid package CRC/bounds: {path}"
    count = int.from_bytes(pkg._dec(logical[:64])[8:10], "little")
    files = {}
    for i in range(count):
        entry = pkg._dec(logical[64+i*80:144+i*80])
        kind = int.from_bytes(entry[:2], "little")
        off = int.from_bytes(entry[8:12], "little")
        size = int.from_bytes(entry[12:16], "little")
        files[kind] = logical[off:off+size]
    assert pkg.product_of(raw).startswith("FM-1_"), "FM-1 packages only"
    return {"product": pkg.product_of(raw), "sha256": digest(raw), "files": files}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--snow-package", type=Path, required=True)
    p.add_argument("--baseline-package", type=Path, required=True)
    p.add_argument("--metadata", type=Path, required=True)
    p.add_argument("--snow-wasm", type=Path, required=True)
    p.add_argument("--baseline-wasm", type=Path, required=True)
    p.add_argument("--previous-snow-package", type=Path)
    p.add_argument("--stock-dir", type=Path)
    p.add_argument("--fetch-stock", action="store_true", help="Fetch two hash-pinned official packages into ignored build output")
    p.add_argument("--output", type=Path, default=ROOT / "build/snow-update-audit")
    p.add_argument("--sanitizers", action="store_true")
    a = p.parse_args()
    out = a.output.resolve();out.mkdir(parents=True, exist_ok=True)
    checks = []

    def run(label, *command, env=None):
        result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (out / f"{label}.log").write_text(result.stdout)
        if result.returncode:
            raise RuntimeError(f"{label} failed; see {out / (label+'.log')}")
        checks.append(label);print(label+": passed", flush=True)

    snow, base = image(a.snow_package), image(a.baseline_package)
    meta = json.loads(a.metadata.read_text())
    assert snow["sha256"] == meta["package_sha256"] and not meta["source_dirty"]
    for entry in (0, 100):
        assert entry in snow["files"] and entry in base["files"]
    assert len(snow["files"][0]) == len(base["files"][0]) == 0x93000
    assert snow["files"][0][:0x4000] == base["files"][0][:0x4000], "Bootstrap review required"
    assert snow["files"][100] == base["files"][100], "Loader review required"
    assert not subprocess.check_output(["git", "diff", BASE, "--", *CRITICAL], cwd=ROOT)
    assert not subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "--", *CRITICAL], cwd=ROOT)
    # Tests must correspond to the firmware source recorded in the shipped build.
    assert not subprocess.check_output(["git", "diff", meta["source_commit"], "--", "firmware", "tools/build.py"], cwd=ROOT)

    compiler = [os.environ.get("CC", "cc"), "-O1", "-w", "-DFELUCCA_SNOW=1"]
    for name, source, defines in [
        ("ota", "ota_test.c", ["-DOWN_PKG=1", f'-DFELUCCA_ID="{snow["product"]}"']),
        ("loader", "ldr_test.c", [f'-DFELUCCA_ID="ota-{snow["product"]}"']),
        ("map", "fm1_flash_map_test.c", []), ("storage", "storage_test.c", []),
        ("stock-head", "snow_stock_head_test.c", ["-DOWN_PKG=1", f'-DFELUCCA_ID="{snow["product"]}"'])]:
        run("compile-"+name, *compiler, *defines, "-o", out/name, ROOT/"tests"/source)
    run("flash-map", out/"map");run("storage-torn-writes", out/"storage")
    run("ota-entry-snow", out/"ota", a.snow_package.resolve())
    run("ota-exit-skiff", out/"ota", a.baseline_package.resolve())
    run("skiff-to-snow", out/"loader", a.baseline_package.resolve(), a.snow_package.resolve())
    run("snow-to-skiff", out/"loader", a.snow_package.resolve(), a.baseline_package.resolve())
    if a.previous_snow_package:
        run("snow-downgrade", out/"loader", a.snow_package.resolve(), a.previous_snow_package.resolve())
        run("snow-upgrade", out/"loader", a.previous_snow_package.resolve(), a.snow_package.resolve())
    run("persisted-cross-boot", "node", ROOT/"tests/snow_cross_boot.mjs", a.snow_wasm.resolve(), a.baseline_wasm.resolve())

    stock = {}
    if a.fetch_stock:
        a.stock_dir = a.stock_dir or out/"vendor"
        a.stock_dir.mkdir(parents=True, exist_ok=True)
        for version, url, expected in [
            ("015", "https://raw.githubusercontent.com/m-vave/fm1_open/main/sdk/firmware/FM-1_015.fwsc", pkg.STOCK_V15_SHA256),
            ("016", "https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/FM-1.fwsc", pkg.STOCK_V16_SHA256)]:
            path = a.stock_dir/f"FM-1_{version}.fwsc"
            if not path.exists():
                with urllib.request.urlopen(url, timeout=30) as response:
                    data = response.read(1024*1024)
                assert digest(data) == expected, "Vendor package changed; compatibility review required"
                path.write_bytes(data)
    if a.stock_dir:
        for version, expected in [("015", pkg.STOCK_V15_SHA256), ("016", pkg.STOCK_V16_SHA256)]:
            path = a.stock_dir / f"FM-1_{version}.fwsc"
            if path.exists():
                item = image(path);assert item["sha256"] == expected, f"Unrecognised stock V{version}"
                assert item["product"] == "FM-1_"+version
                stock[version] = {"sha256": expected, "path": path.resolve()}
                run("ota-stock-v"+version, out/"ota", path.resolve(), "official")
                run("stock-v"+version+"-to-snow", out/"loader", path.resolve(), a.snow_package.resolve())
        if len(stock) == 2:
            run("v15-rejects-v16-head", out/"stock-head", stock["015"]["path"], stock["016"]["path"])
            run("v16-rejects-v15-head", out/"stock-head", stock["016"]["path"], stock["015"]["path"])
            factory_page = a.metadata.resolve().parent / "install"
            if (factory_page / "factory.html").exists():
                run("factory-installer", "node", ROOT/"tests/snow_factory_install.mjs", factory_page, a.stock_dir.resolve())
    if a.sanitizers:
        for name, source, defines in [("ota-asan", "ota_test.c", ["-DOWN_PKG=1"]), ("loader-asan", "ldr_test.c", [])]:
            run("compile-"+name, *compiler, "-fsanitize=address,undefined", "-fno-sanitize-recover=all", *defines,
                "-o", out/name, ROOT/"tests"/source)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
        run("ota-sanitizers", out/"ota-asan", a.snow_package.resolve(), env=env)
        run("rollback-sanitizers", out/"loader-asan", a.snow_package.resolve(), a.baseline_package.resolve(), env=env)
    report = {"software_checks": "passed", "physical_fm1_validation": "pending",
              "audit_source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
              "audit_source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
              "firmware_version": meta["version"], "firmware_source": meta["source_commit"],
              "snow_sha256": snow["sha256"], "baseline_sha256": base["sha256"],
              "previous_snow_sha256": digest(a.previous_snow_package.read_bytes()) if a.previous_snow_package else None,
              "bootstrap_head_identical_to_baseline": True, "loader_identical_to_baseline": True,
              "loader_sha256": digest(snow["files"][100]), "critical_sources_unchanged_from": BASE,
              "critical_source_paths": CRITICAL, "checks": checks,
              "official_packages": {v: x["sha256"] for v, x in stock.items()},
              "official_rollback_checks": "passed" if len(stock) == 2 else "not_run",
              "limits": ["No physical flash/write/USB/CPU/SPI validation was performed.",
                         "Factory rollback requires the version matching the device's preserved bootstrap.",
                         "The vendor's loader binary is not executed by these host tests.",
                         "Another fork's updater and loader require their own compatibility review.",
                         "Musical data is not automatically portable; retain a complete pre-install backup."]}
    (out/"update-audit.json").write_text(json.dumps(report, indent=2)+"\n")
    print("Software audit passed. Physical FM-1 qualification remains pending.")


if __name__ == "__main__":
    main()
