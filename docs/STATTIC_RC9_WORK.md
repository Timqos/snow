# STATTIC rc9 — hot data in SRAM and hardware timing evidence

Hardware rc8: all five non-CH channels stay clean at up to 60%; L stays fixed
while their parameters are varied. Enabling CH produces rasp, up to 80% and
increasing L. The user explicitly confirms ordinary live play without any
USB audio recording; the recording remark was about an earlier SLOOP test.
The CH path exceeds the available deadline in this combination; the exact
stage is not measured yet. Do not conclude the hardware cannot play chords.

Reference review: SLOOP bac9c05ea7627449405aa10e48da21d75b6c8c56 has three
synth parts (`firmware/src/core.h` NPART), a separate drum unit, and shared
chorus/delay/reverb buses (`fx.c`). Its FM6 core preselects operator loops
outside the sample loop. It supports real synthesis and chords; sample-based
drums alone cannot explain its success. STATTIC keeps its six dedicated
instruments and independent racks.

Concrete execution change:

- The oscillator/envelope code is in SRAM already, but SINE/SVF_G/TANH_Q15
  remained in flash. Most CH factories use sine. Move these exact tables,
  2,820 bytes total, to normal initialized data SRAM on STATTIC targets only.
- Use the existing `.data` startup copy, before either core/audio. No HAL,
  memory map, loader, storage layout or runtime waveform-copy change.
- Keep generated host/default declarations equivalent when the placement
  macro is absent; all numeric values and synthesis arithmetic are unchanged.

Hardware diagnostics, read-only in SYSTEM after CPU CLOCK:

- AUDIO PEAK: maximum measured half duration since boot, as percent of the
  256-frame/~5.8 ms deadline, including nested interrupts.
- AUDIO LAST: most recent missed half's duration; before the first miss,
  use the busiest half. CH SYNTH / CH FX/MIX / CORE1 WAIT / AUDIO IRQ belong
  to this same half and are displayed in microseconds.
- Keep the last missed snapshot even if a later clean half took longer.
  Stages overlap across cores and nested interrupts; never add them as
  independent CPU fractions. Reset only at application startup.
- Time only CH's synth separately; use existing per-track timing for its
  remaining FX/mixer work. Wait is written by core 0, CH fields by its owner;
  reset/snapshot only while the second core is idle or after all joins.
- Preserve the original L sampling point, overload guard, voice cap and
  audio output. No audio-thread allocation, drawing or buffer clearing.
- Foreground diagnostic reads use volatile completed snapshots. Keep menu
  value-count dispatch out of line. Replace the timer's hardware modulo-5
  with masks for its proven 0..9 phase domain: USB still uses 0/5, UART 2/7,
  at the same 10 kHz scan and 2 kHz service cadence. Equivalence holds for
  every reachable phase. No USB recording is required for this timer to run.

Initial target validation passes all unchanged loop budgets, including both
interrupt handlers. Linked SRAM/startup-image table hashes pass. Data/BSS is
91,624 / 98,304 B, leaving 6,680 B; pool and executable-RAM reserves remain
8,204 B and 540 B. The app uses 575,536 / 581,564 B, leaving 6,028 B.
Worst-width menus, including all timing values at their display limits, have
zero clipped/shortened draws. Diagnostic snapshot/read-only contracts pass
ASan/UBSan. Default generated tables preprocess identically to rc8.

Validation required: linked table SRAM/startup-image hashes, unchanged PCM
including 2,592 manual operator configurations, coherent missed-half timing
and read-only settings, worst-width menu labels/values, sanitizers, existing
target budgets, full STATTIC native/WASM/worker/browser suite, exact rc9 binary
audit with previous rc8 both ways, then published URL/hash verification.

Completed validation:

- All three target table addresses and exact startup-image byte hashes pass;
  oscillator/envelope code and all their compiled callees stay in SRAM.
- The complete local STATTIC native/WASM/worker/audio suite passes, including
  all 2,592 immutable operator configurations and final state, factory/FX/onset
  references, strict CH4, one-minute six-channel strong-FX playback, actual
  two-bar release at 90/120/150/180 BPM, 64-step generation, shared-BAR changes
  and eight Undo entries. No reference hashes or target budgets were raised.
- Snapshot selection, peak retention, last-missed stages, read-only rows and
  boot-reset contracts pass, including ASan/UBSan. Every menu timing value
  at its width limit passes with zero clipped/shortened text.
- Real Chromium/AudioWorklet desktop/mobile controls, read-only diagnostic
  interaction and factory return pass. SYSTEM timing frames were visually
  inspected. Browser values are zero because hardware timing is not emulated.
- All 28 exact-binary update/rollback checks pass, including rc8 both ways,
  matching V15/V16 heads, preserved loader behavior and OTA/loader sanitizers.

Firmware/audit source: `676c7090c30831133eddaedf1e8f5e5afa66bf03`.
File: `stattic-0.8.0-rc9.fwsc`, 610,430 B.
SHA-256: `6c65c5167fc5b9a3ed7eda0151e81ff0fe42ea7ef915eb9ca11831ed6dba17a8`.
Previous packages, including rc8, remain available; source PR #3 is unmerged.

Remaining hardware follow-up: track-level splitting keeps all CH voices on
one core. Symmetric mask ties prefer the lower integer, hence CH (bit 5)
stays on core 0; average load is not evidence that both cores are saturated.
Do not change this policy on an unmeasured assumption in this candidate.
Use the new CH/worker/IRQ stage snapshot to assess the next scheduling step.

Published: https://timqos.github.io/snow/snow/?v=080rc9

- gh-pages: `238588f`; successful Pages run: `42379576313`.
- All 12 normal artifact URLs and the root landing match local hashes; WASM
  MIME is correct. Verified 2026-11-07 16:02:30 Europe/Kyiv.
- Complete local STATTIC release checks and the exact-binary audit pass.
  Independent GitHub source CI is running; source PR remains unmerged.

No physical FM-1 is connected here. Table relocation removes flash accesses;
its physical timing benefit and whether it cures rasp remain unmeasured.
