#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Reproducible cloud compiler: retain the upstream helper for other local builds.
set -euo pipefail
destination=${1:?Usage: cloud_toolchain.sh DESTINATION}
version=jieli-linux-toolchains-20260730.1
expected=a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd
mkdir -p "$destination"
archive="$destination/$version.tar.xz"
if ! [ -f "$archive" ] || ! echo "$expected  $archive" | sha256sum --check --status; then
  curl --fail --location --retry 3 --output "$archive.download" \
    "https://jl-update.oss-cn-shenzhen.aliyuncs.com/$version.tar.xz"
  echo "$expected  $archive.download" | sha256sum --check
  mv "$archive.download" "$archive"
fi
echo "$expected  $archive" | sha256sum --check
tar -xJf "$archive" -C "$destination"
ln -sfn "$version" "$destination/toolchain"
echo "JIELI_TOOLCHAIN=$destination/toolchain"
