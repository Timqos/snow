# STATTIC rc6 — residual low-note onset clicks

Hardware feedback: rc5 substantially reduced clicks. Some remain when CH
plays with BS, with rarer clicks when CH plays with KK; generated kick sounds
differ in susceptibility. The low-note attack clicks even with low physical
MASTER. The user has not yet checked L during these remaining clicks.

Scope: preserve the entire rc5 interface/fonts, generators, seeds, sound
parameters, presets, architecture, effects, gains and 8/12 voice budget.

Investigation:

- Review note/retrigger allocation, FM feedback/phase state, mixer/limiter,
  two-core scheduling and the target's SRAM assembly. A host comparison of
  feedback-state restoration did not establish a consistent onset repair;
  no feedback, envelope, phase or limiter behavior is changed speculatively.
- Sine and triangle currently pay a helper call and waveform switch for
  every sample/operator. Inline their identical arithmetic in the SRAM loop.
  Other shapes still use the complete original path. Target budgets retain
  their existing limits; no limit is raised to admit this change.
- Both synchronized LFOs repeat integer division every control block at an
  unchanged period. Cache the exact increment independently for six tracks
  and two clocks. Tempo/division changes replace the cached increment; phase,
  trigger, fade, range and modulation remain untouched. Adds 96 bytes BSS.
- Fix STATTIC's diagnostic initializer being referenced in the non-STATTIC
  emulator build. The remaining broad host suite now finishes successfully,
  including actual browser/native equality and converter tests. This build
  guard does not change the STATTIC target path.

Verification before release:

- Capture eight transition hashes from unmodified rc5 `27d4554` and require
  exact equality: 512 repeated KK/BS attacks under held four-note CH, factory
  and generated sounds, FX, quiet MASTER, USB fixed level and tempo/LFO changes.
- Check 300,000 common/general waveform inputs, independent LFO clock-cache
  updates/hits, ASan/UBSan, immutable factories/FX, full STATTIC native/WASM
  suite, real host worker splits and target RAM/HAL/loop budgets.
- Fresh exact-package update/rollback audit versus Skiff and previous rc5,
  factory-head checks, browser controls and published artifact hashes.

No physical FM-1 is connected here. Reduced redundant work is an additional
performance candidate, not proof the remaining click has been reproduced or
cured. Retest CH + BS/KK and whether L increases at the residual attack click.

Completed validation:

- Entire STATTIC native/WASM/audio pipeline passed, including immutable
  factories/FX, eight new rc5 onset-capture hashes, 512 retriggers, 300,000
  waveform comparisons, LFO clock cache changes/hits, ASan/UBSan, preserved
  two-bar release, generators, shared-BAR controls and seven real worker splits.
- Actual target placement/HAL checks and existing loop-work limits passed.
  `snow_render`'s static loop score is 2054 against the unchanged 1914 +10%
  limit: inlining exposes work previously counted in its callee. The common
  dynamic path avoids helper dispatch; no physical CPU saving is claimed.
- Real AudioWorklet/Chromium desktop/mobile controls and factory return passed.
- Exact new binary passed 28 OTA/loader/rollback tests, including previous rc5
  in both directions, original V15/V16 heads and OTA/loader sanitizers.
- The broad suite's previously failing generic-emulator stage now passes:
  native song/reboot, WASM/native equality and converter checks complete with
  ALL HOST TESTS PASSED; earlier stages were already completed in the rc5 run.

Final static regions:

| Region | Used | Limit | Free |
| --- | ---: | ---: | ---: |
| Application | 575,000 B | 581,564 B | 6,564 B |
| Data + BSS | 88,760 B | 98,304 B | 9,544 B |
| Data pool | 348,148 B | 356,352 B | 8,204 B |
| Executable RAM + ramx | 23,740 B | 24,576 B | 836 B |

The 4 KiB application and 8 KiB pool minimum reserves remain enforced.

Published candidate: https://timqos.github.io/snow/snow/?v=080rc6

- Firmware source: `198eee5bd585f62004619d874ce264b4e06c0481`.
- File: `stattic-0.8.0-rc6.fwsc`, 610,430 bytes.
- SHA-256: `38f551d861c58946378247b8534c72b23432965a0e06afe4cb134c4673b6ab8c`.
- gh-pages: `050e237`; successful Pages run: `42309635383`.
- Previous packages, including rc5, are retained. PR #3 remains unmerged.

All 12 normal site artifact URLs plus the root landing match local hashes;
WASM MIME is correct. Verified 2026-11-07 01:13:19 Europe/Kyiv.
