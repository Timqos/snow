# STATTIC rc10 — interrupt-aware scheduling

The user's rc9 USB captures establish 96 new DMA-half misses in 115.049 s,
458 overload voice fades and a 6.826 ms maximum against a 5.805 ms interval.
Both cores stayed enabled with zero mailbox faults; USB audio was inactive.
These aggregate facts are recorded without republishing the raw user logs.
The exact full critical-path cause is not yet established.

rc10 changes execution scheduling, not synthesis:

- Measure primary render work without nested TIMER5 time. Never subtract
  core 0's IRQ counter from work measured on core 1. Keep diagnostic wall
  times separately.
- Learn bounded IRQ overhead once per completed half. Minimise the predicted
  critical path: max(worker work, primary work plus estimated interruptions),
  including handoff cost. A monotone heavier-worker orientation represents
  each complementary pair once, reducing 62 mask visits to 31.
- Allow an all-worker render if its saved primary interruptions beat handoff.
  Event handling, joining and master work remain on core 0. Existing worker
  validation/fallback, voice shedding and deadlines remain intact.
- Add a separate 48-byte retained detail snapshot: each track's wall time,
  serial mixer work, primary/worker work, ownership masks and IRQ model.
  Publish it with the original five-word snapshot after all joins. Preserve
  latest-missed-half retention and an independent maximum.
- Extend official ELF maps and the browser to bracket/double-read both
  snapshots. Keep rc9 maps/decoding. Show voice-shedding counts and treat
  CPU MHz 0 as an unrecognised clock source. Existing read-only clock probe
  and flash-divider HAL helpers provide additional status; no PLL change.

Unchanged: all six engines, three oscillators, algorithms, parameters,
effects/ranges/independent racks, gains, presets, random states, generators,
strict CH4 including tails, other voice budgets, 256-frame buffer and sample
rate, five HOME pages, fonts, controls and shared-BAR changes. HAL, loader
execution, memory maps, storage layout and USB transport are unchanged.

Validation includes an independent exhaustive 63-mask oracle over 20,000
cost/duty combinations, primary-only IRQ accounting and bounded decay,
coherent detail snapshots, immutable PCM/operator/onset/FX references,
one/two-core equivalence, existing target budgets, full native/WASM/browser
checks and exact-package update/rollback validation. Physical improvement
must be measured on the FM-1; rc10 is not advertised as a confirmed cure.

Build/publication results will be appended after verification.
