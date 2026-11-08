<p align="center"><img src="web/stattic/logo.svg" alt="STATTIC" width="160"></p>

# STATTIC 0.9.0-S1 — SLOOP-based test candidate

Firmware for the M‑VAVE FM‑1. Four HOME channels: **DR / BS / SY / CH**.
Built on [SLOOP 2.8.5](https://github.com/isod89/sloop-fm1/tree/bac9c05ea7627449405aa10e48da21d75b6c8c56),
with STATTIC synthesis, presets, modulation and independent channel effects.
SLOOP's audio scheduling, second-core startup, limiter and native non-HOME windows
are retained. This is a separate firmware personality, selected by `--stattic`.

[Simulator and downloads](https://timqos.github.io/snow/snow/) ·
[Manual](web/stattic/manual/index.html) · [Migration and validation](STATTIC-MIGRATION.md) ·
[USB protocol additions](STATTIC-PROTOCOL.md) · [Credits](web/stattic/CREDITS.md)

**Hardware audio validation is pending.** Host and browser checks do not establish
real-time performance on the FM‑1. The earlier Skiff-based STATTIC candidate missed
audio deadlines; this candidate requires a cold-boot comparison of CH + BS + DR,
all four channels, randomization and the USB console's late-buffer delta.
It is not advertised as click-free or hardware-qualified.

- Five STATTIC HOME pages; large pixel helper text with more line spacing.
  The first three pages share three static-art patterns, shuffled without repeats
  when changing knobs. Each detent scrolls upward six rows with seamless wrapping.
- Three patterns per channel; 64-step role-aware generation, 35 scales, undo/redo,
  temporary save/reload and Control All. During PLAY, pattern, mute/solo and all four
  HOME 2 generators take effect together at the next bar.
- BS, SY and CH retain all six STATTIC synthesis algorithms. CH has a four-voice cap,
  a real default two-bar release and long OCT+ note/chord switching on HOME 1.
  SLOOP's shared active synth budget is eight voices; drums have six sample voices.
- DR uses 40 factory shots rendered from STATTIC drum presets. DR sound randomization
  selects within KK/SD/HH/PR categories. Four 60 KiB user banks fit 44 half-second
  mono 22.05 kHz ADPCM shots in total, with at most 16 zones per bank.
- A sample import/conversion/replacement manager, full backup/restore, read-only
  USB diagnostics and installers for the candidate and verified original V15/V16
  packages. Original firmware files must be obtained separately.

## Build and verify

Install the JieLi toolchain and SDK as described in [BUILDING.md](BUILDING.md).
Then set `JIELI_TOOLCHAIN` and `AC79_SDK` and run:

```sh
sh build.sh --stattic
python3 tests/stattic_check.py
# Add Emscripten to PATH first:
python3 tools/build_stattic_web.py
node tests/stattic_web_test.mjs
python3 tools/build_stattic_site.py --preview
python3 -m http.server 8765 --directory build/stattic-site
# In a second terminal, with Playwright and Chromium installed:
node tests/stattic_browser_test.cjs
```

The device package is `build/stattic-0.9.0-sloop-rc1.fwsc`.
`build/stattic-build.json` records its source commit, hashes and target checks.
For a public site, commit the source, rebuild both device and browser, run the
checks, then run `python3 tools/build_stattic_site.py` without `--preview`.
Packaging rejects dirty or mismatched source and binary hashes. Nothing deploys
automatically. `build/stattic-site/firmware/source.zip` contains the matching source.

The no-flag build remains the native SLOOP reference. Its original documentation
is retained in [SLOOP-README.md](SLOOP-README.md), [SLOOP.md](SLOOP.md), and
[GUIDE.md](GUIDE.md); its engine counts and native binary format do not describe STATTIC.
STATTIC uses a separate project/settings/preset format: native SLOOP and old Skiff
projects must not be loaded as STATTIC data. Back up with the running firmware's
own tools before installing. Read the installation and recovery instructions and
the custom-firmware risk notice in the manual.

GPL-3.0-only. Original SLOOP, Felucca, dual-core, font, SDK and other third-party
notices remain in the source tree. See [LICENSE](LICENSE) and the credits.
