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

Remaining release work: complete the STATTIC native/WASM/worker/audio suite,
sanitizers and target budgets; build with rc8 identity; audit the exact binary
and previous rc7 update/return; check browser controls and publish normal URL
hashes. Retain previous packages, installation protections and source PR.

No physical FM-1 is connected here. Host/static checks cannot prove the
remaining rasp fixed, nor quantify the unit's peak CPU saving.
