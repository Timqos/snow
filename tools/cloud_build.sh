#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The same build and checks locally in a cloud executor and on GitHub Actions.
set -euo pipefail
cd "$(dirname "$0")/.."
export EMU_PREVIEW=0
command -v emcc >/dev/null || { echo "cloud build: Emscripten is required" >&2; exit 1; }
command -v node >/dev/null || { echo "cloud build: Node.js is required" >&2; exit 1; }
mkdir -p build
sh build.sh 2>&1 | tee build/firmware-build.log
# The upstream suite builds and tests both WebAssembly modules as well.
sh tests/run_tests.sh 2>&1 | tee build/host-tests.log
python3 tools/cloud_site.py
cp build/felucca.fwsc build/baseline-for-snow.fwsc
# Keep the checked baseline on /try/; publish the Snow synthesis laboratory separately on /snow/.
FELUCCA_SNOW=1 sh build.sh 2>&1 | tee build/snow-build.log
python3 tools/snow_preview.py 2>&1 | tee build/snow-tests.log
python3 tools/snow_update_audit.py \
  --snow-package build/felucca.fwsc --baseline-package build/baseline-for-snow.fwsc \
  --metadata build/snow-preview/build.json --snow-wasm build/snow-preview/felucca.wasm \
  --baseline-wasm build/emu/felucca.wasm --fetch-stock --sanitizers \
  --output build/snow-update-audit 2>&1 | tee build/snow-update-audit.log
cp build/snow-update-audit/update-audit.json build/snow-preview/update-audit.json
mkdir -p build/site/snow
cp -a build/snow-preview/. build/site/snow/
