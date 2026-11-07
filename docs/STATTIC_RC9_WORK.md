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

No physical FM-1 is connected here. Table relocation removes flash accesses;
its physical timing benefit and whether it cures rasp remain unmeasured.
