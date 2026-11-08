# STATTIC on SLOOP

Status: STATTIC 0.9.0-S1 candidate; host, target and browser checks complete.
Hardware audio qualification is pending. No click-free claim is made.

## Sources and decision

The base is SLOOP 2.8.5, `isod89/sloop-fm1`, commit
`bac9c05ea7627449405aa10e48da21d75b6c8c56`. Its GPL notices and third-party
licences remain in place. This branch starts at that commit, not at Skiff.

The user approved four logical channels in this order: **DR, BS, SY, CH**.
Use SLOOP's native three synth parts plus drum track (internal tracks BS, SY,
CH, DR), with a display/input mapping for the HOME order. BS/SY/CH retain
STATTIC synthesis, all six algorithms, waveforms, presets, modulation, editable
parameters and effects. DR plays sampled STATTIC drum presets on the keys.
DR HOME 2 K2 replaces sound synthesis randomization with category-preserving
sample selection: kick from kicks, snare from snares, hat from hats, percussion
from percussion. Categories never swap positions. Add a web sample manager
with import, replacement and automatic conversion.
Do not infer a measured CPU saving from the name “drum machine”.

The old Skiff-based rc11 work stays in `/workspace/snow`, commit `b0cb3a5`;
it is not the next firmware release. USB logs establish missed audio deadlines,
not a master-volume clipping diagnosis. A desktop render is not proof of a
hardware deadline. A source port alone is not a fix for the reported rasp.

## Work order and acceptance

1. [x] Record an unmodified SLOOP target build and native test baseline. Keep
   failures visible; never refresh upstream sound or CPU baselines to hide them.
2. [x] Port the STATTIC oscillator, filter, envelope and preset data to the SLOOP
   engine API. Compare against frozen STATTIC renders before performance changes.
3. [x] Integrate the three STATTIC synth parts, sampled drums, modulation and channel effects with SLOOP's
   audio scheduling, worker boot/self-test, interrupt handling and memory layout.
   Measure worst cases and retain per-stage USB diagnostics. Do not transplant
   the old Skiff scheduler/HAL into the new base or silently remove features.
4. [x] Preserve the five reference HOME pages, pixel fonts/art, theme and logo;
   adapt channel labels/selection to DR/BS/SY/CH, and increase helper line spacing
   without reducing text significantly or moving the upper design. Keep native
   SLOOP non-HOME windows and font sizes; style their colours and fonts to match HOME.
5. [x] Port 64-step role-aware generation, three patterns per channel, shared-bar
   pattern/mute/solo/generation changes, undo, long OCT+ N/CHO (short = octave),
   CH default 2BAR release and maximum four active CH voices including tails.
6. [x] Implement persistence with an explicit new format and compatibility
   checks. Never interpret Skiff or native SLOOP project bytes as the new format.
   Preserve backup and return-to-factory paths.
7. [x] Build the device and browser simulator from the same sources; exercise
   controls, generated audio, long concurrent render, limits and update recovery.
   Update the installer, manual, technical claims and source attribution.
8. [ ] Publish a clearly labelled candidate with its exact source/hash and audit.
   Hardware validation must include CH+BS+DR, parameter randomization, all four
   channels, worst-half time, missed-buffer delta and cold boot. Do not label it
   click-free or stable before those measurements exist.

## Architectural boundary

SLOOP without `FELUCCA_STATTIC` remains a reproducible reference configuration.
STATTIC-specific code is selected explicitly. That separation makes native
SLOOP regression hashes useful throughout the port and prevents a cosmetic
rebranding of the old firmware from being called a SLOOP migration.

The sampling tradeoff is accepted: drum sound randomization becomes selection
of recorded shots within each category. This does not remove synth randomization
on BS/SY/CH, sequence generation, LFO/effect generation, control-all, temporary
save/reload, undo/redo, MIDI, backup, diagnostics or bar-aligned performance changes.
Audit the old STATTIC controls/tests/manual as a feature inventory throughout the port.

## Recorded checks (2026-11-07)

Unmodified base target: app 571820 bytes; data+BSS 59340/98304; lowram
22772/29384; pool 333900/344064. The full upstream host suite completed with
exit 1. Its 182 golden audio renders matched, with no health, voice/routing or
crash failures; all second-core split modes and ThreadSanitizer passed.
Pre-existing target-budget failures: sample_render +371%, grain_render +38%,
drums_mix +11%, fm1_alnk0_irq +54% relative to the repository's stored budgets.
Web tests also encountered environment failures (Node spawnSync EPERM and a
missing expected Playwright browser). These are not an all-green test result,
and the budgets have not been rewritten. Logs: `/tmp/stattic-sloop-baseline-*.log`.

First DSP adapter: 276 frozen old-STATTIC oscillator/envelope renders and
terminal states match exactly on the SLOOP engine API. Coverage: all 60 factory
sounds, all 6 algorithms x 6 primary waves for each role, filter/feedback/noise
paths, and five-second CH release renders. Run
`python3 tests/stattic_engine_vectors.py`; the immutable reference provenance is
in `tests/fixtures/stattic-engine-vectors.json`. This does not test the full
instrument, graphics, live modulation, persistence or hardware deadlines.

## Final candidate layout and checks (2026-11-08)

To store all twelve 64-step patterns, the STATTIC layout reserves four **60 KiB**
user sample slots rather than native SLOOP's four 80 KiB slots. Each has a 512-byte
header/reserve and at most sixteen zones. At 22050 Hz mono IMA ADPCM, a 0.5 s shot
takes 5513 bytes: **11 per slot, 44 user shots total**, plus 40 separate factory
shots. Shorter files can reach sixteen per slot. This change is explicit; native
SLOOP/Skiff storage is not compatible with STATTIC.

Flash: sample slots A0000–DBFFF; presets DC000–DFFFF; OTA E0000–E4FFF;
project copies in 97000–9FFFF and E5000–F9FFF; settings FC000/FD000.
Each of four projects plus the working autosave has two 12 KiB A/B copies.
CRC validation precedes header publication. Erase/program interruptions were
tested at all 216 cut points. Sample writes are refused during PLAY and verified
by exact full-bank readback. Replacement and deletion preserve other zone IDs;
silent tombstones are excluded from the random sample catalog.

Target checks: app 506468 bytes; data+BSS 95308/98304; lowram 27424/29384;
pool 332928/344064 (11136 bytes reserve, exceeding the build's 8 KiB minimum).
Loader image 9756 bytes, packaged loader 8489 bytes; no compiler warnings.
The RAM interrupt section has 925 instructions, no calls; worker RAM loop 160 bytes.
The final build record contains the exact package/source hashes and these checks.
Independent STATTIC FX histories account for much of the fixed RAM footprint;
DSP does not allocate heap memory. Memory occupancy alone is not a CPU measurement.

`python3 tests/stattic_check.py` passes: 276 frozen synth renders, 64 frozen FX
scenes including terminal histories, all 35 old scale names/masks and twelve
roots, role-aware 64-step phrases, three patterns, next-bar changes and atomic ALL
publication, queued-change composition, undo/redo, MIDI ownership, temporary
restore, art shuffle/wrap, project/storage tests, all available UI pages and the
STATTIC loader screen. Real JS/C integration covers category banks, conversion,
full 14-bit parameter IDs, backup/restore and firmware-side STOP protection.

The 16-second full four-channel mix has hash `5279625b726e216c`, peak 17994 in
one-core, two-core, delayed-worker and timeout-fallback modes. Address/undefined
sanitizers and ThreadSanitizer also pass. LeakSanitizer is disabled because this
environment's ptrace mode is unsupported; fixed DSP storage has no heap allocation.
These are deterministic correctness checks, not measurements of FM-1 deadlines.

The ST-only LFO path has independent per-track random state and cached clock
increments, avoiding a shared RNG race and keeping CH on the worker at LFO wraps.
Fast sine/filter/tanh tables and per-part envelope/filter coefficients use bounded
RAM caches. SLOOP's native audio ISR, worker startup, limiter and HAL remain the
base; the HAL addition is an IRQ save/restore guard for atomic parameter changes.
All STATTIC algorithms, modulation destinations and four independent FX racks remain.

Native SLOOP checks with ST disabled: 182 original render hashes unchanged,
zero health/voice/routing/crash failures, and project/storage/user-preset tests pass.
The earlier target-budget failures above remain part of the unmodified baseline.

`node tests/stattic_browser_test.cjs` passes in Chromium using the real AudioWorklet:
five HOME pages, art bag and scroll, long/short OCT+, PLAY/panic, native menus,
mobile layouts, WAV conversion/replacement/export and installer/console loads.
The simulator uses the same DSP/UI/project/editor sources, with browser-only
clock, display, input and RAM-backed flash adapters. It generates demo phrases at
startup; the device restores its own project instead. Browser speed cannot be
used to estimate the FM-1's audio deadline.

Still required on the actual device: cold boot, at least five minutes each of
CH+BS+DR and all four channels, strong random FX/parameters, CH release tails,
and console capture of late-buffer deltas and worst-half per-track/event times.
Do not pronounce the reported rasp fixed until those results exist.
