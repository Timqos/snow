# STATTIC rc8 — deadline work and track-cost correction

User's rc7 hardware result: CH + KK + BS sometimes rasps; particular random
kick sounds alter occurrence. BS + CH alone now seems clean, though uncertain.
Two cores, average load up to 78%. L rises with each rasp, from 66 to 92.
The user then clarifies any channel may provoke it, depending on parameters.
Treat missed DMA deadlines as confirmed; a specific channel or polyphony alone
is not established as the cause. Keep the requested strict four-voice CH cap.

Concrete changes, without altering the sound or interface:

- A silent held voice remains allocated but can render zero samples. The
  estimator compared allocated count with rendered count, adding a nonexistent
  onset each block and accumulating a misleading track cost. Store the prior
  allocated count separately; retain actual rendered counts for diagnostics.
- In FM CHAIN, the first modulator waveform was immediately replaced by a
  second evaluation. Build the final phase first, then evaluate once. This
  retains all three operators and all six algorithms on every channel.
- Place snow_amp and its inlined helpers in fast executable SRAM, alongside
  snow_render and snow_wave, to avoid envelope calls into flash during audio.

Evidence before/following implementation:

- New silent-held regression fails on unchanged rc7 at part_cost equality.
  The corrected implementation passes all 256 blocks without repeated onset.
- Before source edits, capture six immutable hashes from clean source 2497b53
  for 2,592 manual configurations: six roles, six algorithms, six waveform
  combinations, three feedback routes, four filters; include sub/noise/drive,
  LFO, tempo/ENV SYNC, held/released notes and final oscillator/voice state.
  The changed engine matches all six hashes exactly.
- The initial target build passes its unchanged memory/HAL guards. Compiled
  snow_render has three waveform-helper call sites instead of four; snow_amp
  has no calls. All three compiled functions/callees execute in SRAM.

Completed validation:

- Complete STATTIC native/WASM/worker/audio suite passes, including strict
  CH4, eight Undo entries, full 64-step generation, shared-BAR controls,
  minute-long six-channel strong-FX playback, real two-bar release at
  90/120/150/180 BPM and unchanged factory/FX/onset PCM captures.
- New estimator regression and 2,592-sound operator reference both pass
  ASan/UBSan. The regression was observed failing on rc7 before the fix.
- All existing target loop budgets and memory/HAL guards pass without raised
  limits. snow_render has three waveform call sites, snow_amp has none; all
  oscillator/envelope compiled calls stay in executable SRAM. Static loop
  scores are layout estimates, not elapsed physical CPU measurements.
- Real Chromium/AudioWorklet desktop/mobile controls pass, including five
  HOME pages, CH OCT+ hold, six pattern encoders, BAR mute/solo and factory
  validation with no unsolicited MIDI request.
- The exact rc8 package passes all 28 update/rollback checks, including rc7
  upgrade/downgrade, V15/V16 matching heads and OTA/loader sanitizers.

| Region | Used | Limit | Free |
| --- | ---: | ---: | ---: |
| Application | 574,848 B | 581,564 B | 6,716 B |
| Data + BSS | 88,760 B | 98,304 B | 9,544 B |
| Data pool | 348,148 B | 356,352 B | 8,204 B |
| Executable RAM + ramx | 24,036 B | 24,576 B | 540 B |

- Firmware/audit source: `0dfb8f113121790352b68c835be455e3bedd67cf`.
- File: `stattic-0.8.0-rc8.fwsc`, 610,430 bytes.
- SHA-256: `a0f5b1711fba5b818e02933e411a3b00b140422b507da0710a22596e7d3e2232`.
- Previous package rc7 is retained; source PR #3 remains unmerged.

Publication and normal-URL hash verification remain pending.

No physical FM-1 is connected here. Host/static checks cannot prove the
remaining rasp fixed, nor quantify the unit's peak CPU saving.
