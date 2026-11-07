# STATTIC on SLOOP

Status: development; no STATTIC package from this branch is qualified for hardware.

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
2. [ ] Port the STATTIC oscillator, filter, envelope and preset data to the SLOOP
   engine API. Compare against frozen STATTIC renders before performance changes.
3. [ ] Integrate the three STATTIC synth parts, sampled drums, modulation and channel effects with SLOOP's
   audio scheduling, worker boot/self-test, interrupt handling and memory layout.
   Measure worst cases and retain per-stage USB diagnostics. Do not transplant
   the old Skiff scheduler/HAL into the new base or silently remove features.
4. [ ] Preserve the five reference HOME pages, pixel fonts/art, theme and logo;
   adapt channel labels/selection to DR/BS/SY/CH, and increase helper line spacing
   without reducing text significantly or moving the upper design. Keep native
   SLOOP non-HOME windows and font sizes; style their colours and fonts to match HOME.
5. [ ] Port 64-step role-aware generation, three patterns per channel, shared-bar
   pattern/mute/solo/generation changes, undo, long OCT+ N/CHO (short = octave),
   CH default 2BAR release and maximum four active CH voices including tails.
6. [ ] Implement persistence with an explicit new format and compatibility
   checks. Never interpret Skiff or native SLOOP project bytes as the new format.
   Preserve backup and return-to-factory paths.
7. [ ] Build the device and browser simulator from the same sources; exercise
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

Sample capacity from the unchanged SLOOP layout: four 81920-byte user slots,
each with a 512-byte header and at most 16 zones. A mono 0.5-second IMA ADPCM
shot at 22050 Hz takes 5513 bytes, so 14 fit per slot, 56 across four separate
banks. At 44100 Hz it takes 11025 bytes, 7 per slot, 28 total. SLOOP's stock
upload tools convert to 22050 Hz. This is the user-slot budget, not the device's
entire flash; a kit has at most 16 mapped lanes, independent of library size.
