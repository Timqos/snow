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
