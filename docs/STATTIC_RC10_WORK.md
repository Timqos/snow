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

Completed software validation:

- The full local native/WASM pipeline passes. Immutable factory, FX, low-note
  onset and 2,592 operator PCM/final-state references remain identical.
- Nine one/two-core worker modes, including all-worker and IRQ-aware auto,
  produce identical audio in all three scenes. Existing CH4, shared budgets,
  BAR changes, generation, actual two-bar release and eight Undo entries pass.
- Primary IRQ accounting, bounded duty prediction, oracle and snapshot tests
  pass ASan/UBSan. All existing target budgets are retained. New scalar
  helper budgets are separately checked; no old limit was increased.
- Linked oscillator/envelope code and exact sine/filter/saturation table
  bytes remain in SRAM. App 575,052 / 581,564 B (6,512 free); data/BSS
  91,752 / 98,304 B (6,552 free); pool 348,148 / 356,352 B (8,204 free);
  executable RAM + ramx 24,008 / 24,576 B (568 free).
- Real Chromium/AudioWorklet controls, five HOME pages, queued BAR changes,
  CH hold, menus, mobile layout and factory return pass. USB transport is
  tested with a deterministic CDC model, including rc10 detail and unknown
  clock reporting; it does not claim a physical USB test.
- All 28 exact-package update/rollback checks pass, including rc9 both
  directions, V15/V16 matching heads and OTA/loader ASan/UBSan. Critical HAL,
  flash, storage, USB and loader execution remain unchanged.

Firmware source: `967530f069a38db0b5d4c01d4c7ec69d0ea77d15`.
Audit/map source: `319babd91b64400b68de2d68558349ce6eb91ea0`.
File: `stattic-0.8.0-rc10.fwsc`, 610,430 bytes.
SHA-256: `19143e04d33405079e03dbc97ac15450d9dd124ba9c8e3aec10d7787adf27f47`.
The rc9 package remains available. Source PR #3 remains unmerged.

No physical FM-1 is attached here. Reliable live playback is still unverified;
compare hardware L/voice-fade counters and detailed snapshots after installation.
Published: https://timqos.github.io/snow/snow/?v=080rc10
USB console: https://timqos.github.io/snow/snow/console/?v=2

- gh-pages: `34efcc2a92a463ae8ebca5c4b48111ed0c8e7fe5`.
- Successful Pages run: `42391108232`.
- All 20 normal artifact URLs and the root landing match published local
  hashes; JavaScript-module and WASM MIME types are correct. The previous
  rc9 package also retains its exact hash. Verified 2026-11-07 18:26:05 Europe/Kyiv.
- Machine-readable verification: `build/stattic-live-verification-rc10.json`.
- Independent GitHub source CI is running; the source PR remains unmerged.
