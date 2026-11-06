# STATTIC rc5 — readable pixel type and CH onset investigation

User requirements: enlarge text outside custom HOME; give all HOME bottom
helpers one maximum practical pixel face with complete wording; preserve the
upper HOME layout and all synthesis, effects, generators and sound controls.

Hardware report after rc4: Ring/resonator distortion is gone. CH alone and
CH + HH/PR are clean; adding BS or KK causes a short click on note/chord
attacks. Two cores, 76–80% average CPU. L rises on the attacks. MASTER at
20% makes the click quieter without removing it. Physical hardware is not
connected to this development environment.

Implemented:

- HOME lower helpers: one 8×14 face, complete captions. All five upper
  regions compare pixel-identical to rc4's native frames (180 rows on pages
  1–4, 190 rows on page 5). Upper art, icons, labels and controls remain.
- Other editors: 14/18/20/28-pixel capitals, adaptive horizontal spacing,
  full-width settings labels and values on separate lines, three visible
  settings rows with unchanged scrolling/tab controls. Reuse the old 5×7
  binary source atlas; no new framebuffer, cache or audio-thread drawing.
- Oscillator render/waveform code in executable SRAM. Target disassembly
  checks both functions and every render call remain there. No DSP math,
  gain, architecture, parameter, effect or voice-budget reduction.
- First-block work prediction before the core split for newly active
  voices; measured costs rise immediately and decay slowly. Eight-entry
  Undo retained by relocating two cold snapshots to the existing data pool.
- L is truly per application boot, subtracting the initial retained count
  instead of displaying missed halves inherited across warm restarts.

Diagnosis boundaries:

- SLOOP uses the same free-half transition to count late DMA work. A rising
  L is evidence of deadline misses; average load cannot exclude those peaks.
- A loud coherent four-note CH plus KK/BS can engage the existing limiter.
  USB LEVEL FIXED puts physical MASTER after that limiter, so the MASTER
  test alone cannot exclude internal limiting. The pair test exercises
  default/full track levels and pre/post-master attenuation, and checks
  bounded PCM and four-voice continuity in all 800 combinations. Default
  track levels at 20% pre-master do not activate the limiter in this test.
- No physical claim of a cured click until the new package is tested on
  FM-1. Retest CH + BS and CH + KK while observing the change in L.

Release validation:

- All target RAM/ROM/HAL guards and existing per-loop work budgets passed.
  The bounded onset predictor has a separate 44-unit static loop budget;
  the original ISR limit is retained. Both oscillator functions and all
  renderer calls are verified in SRAM from the final target disassembly.
- Complete STATTIC native/WASM/audio suite passed: all physical controls,
  64-step musical generation, saved-bank handling, shared-bar patterns,
  mute/solo and four generator families, real two-bar CH release, seven
  equal-budget worker splits and a minute of six-channel strong-FX playback.
  Fifty unchanged factories and nine FX captures retain immutable hashes.
- The 800 CH + KK/BS pair attacks remain bounded and retain all four held
  CH voices. Default/minimum/maximum editor-value and full settings/dialog
  sweeps report zero clipped draws and zero shortened fixed fields.
  Repair/onset/eight-Undo and typography suites also pass ASan/UBSan.
- Real AudioWorklet/Chromium, five HOME pages, ENV/menu fonts, OCT+ A/N,
  queued BAR state, desktop/mobile layout and protected factory return pass.
- The exact rc5 package passes 28 update/rollback checks: Skiff and previous
  rc4 in both directions, original V15/V16 validation, startup-head rejection,
  torn writes, persisted cross-boot and OTA/loader sanitizers. No device is
  physically flashed by these tests.
- Broader upstream UI verification retains all 3,458 golden pictures,
  1,917,352 alignment measurements and zero layout findings. Functional
  checks outside STATTIC also pass through UI, storage and protocol stages;
  the remaining broad native audio/CLI suite is running independently.

Final static regions:

| Region | Used | Limit | Free |
| --- | ---: | ---: | ---: |
| Application | 574,596 B | 581,564 B | 6,968 B |
| Data + BSS | 88,664 B | 98,304 B | 9,640 B |
| Data pool | 348,148 B | 356,352 B | 8,204 B |
| Executable RAM + ramx | 23,404 B | 24,576 B | 1,172 B |

The enforced minimum application/pool reserves remain 4 KiB / 8 KiB.
These are static layout reserves, not physical CPU or stack qualification.

Published: https://timqos.github.io/snow/snow/?v=080rc5

- Firmware source: `aa8e5d7c9175362775b770bcd335fc5a2cd7a273`.
- Package: `stattic-0.8.0-rc5.fwsc`, 610,430 bytes.
- SHA-256: `81191404a8166e9e348b64531b983cdbdd87e6b9ac24e9fd6b41c01acefcbcb1`.
- Publication: gh-pages `24e707c`, successful Pages run `42303715908`.
- All 12 normal artifact URLs plus the root landing page match local hashes;
  WASM MIME checked, 2026-11-06 22:19:11 UTC.
- Old packages, including rc4, remain available. Source PR #3 is unmerged.

Physical CH + BS/KK onset-click qualification remains pending. The user
confirmed rc4 removed the Ring/resonator distortion; that does not establish
the remaining click is cured by rc5's scheduling/SRAM changes.
