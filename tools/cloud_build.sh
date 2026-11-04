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
# Keep the checked baseline on /try/; publish the Snow synthesis laboratory separately on /snow/.
FELUCCA_SNOW=1 sh build.sh 2>&1 | tee build/snow-build.log
python3 tools/snow_preview.py 2>&1 | tee build/snow-tests.log
mkdir -p build/site/snow
cp -a build/snow-preview/. build/site/snow/
