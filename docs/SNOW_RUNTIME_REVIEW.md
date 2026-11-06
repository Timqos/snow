<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Earlier Snow 0.7.4 runtime repair — historical analysis

Current STATTIC changes and the newer 2-core/80%-playback report are recorded
in [STATTIC_RELEASE.md](STATTIC_RELEASE.md). The earlier deferred HOME redesign
is implemented there; the analysis below records the earlier candidate.

The first physical-device report for Snow describes distorted audio even
with STOP, low MASTER and one note, sluggish menus, and an unexpectedly short
CH release despite a displayed 2 BAR. The device reports **1 CORE, 99% CPU**.
The user did not give a separate idle and held-note reading. These symptoms
must not be labelled resolved based on browser tests.

Snow is a fork of Skiff 2.2 (`76b5f9c9f6d523466f5c3910f9baf91110133b7b`).
The `Felucca` filenames and `FELUCCA_*` macros also occur in that upstream.
Snow's synthesis, per-channel effects and HOME/MIXER UI are additional code;
upstream hardware qualification does not qualify those additions.

## Confirmed code defects and repairs

- All six independent effect racks previously computed chorus, delay and
  reverb even with every effect amount at zero or BYPASS enabled. The revised
  code skips disabled effects and the completely dry rack. Re-enable uses
  history ages, so old audio cannot return, and there is no large memory
  clear in the interrupt. Enabled effects retain their own tails.
- An enabled rack also sleeps while its input and all histories are exactly
  zero. It wakes on the next nonzero sample block. After a note it must
  complete a full longest-buffer turn of zero writes with zero scalar state
  before sleeping: no amplitude threshold or fixed timeout truncates tails.
  Modulation clocks, delay smoothing and bitcrusher hold phase keep advancing.
- BYPASS previously leaked the already calculated stereo return for up to
  one interpolation segment. It now returns the exact dry input immediately.
- Snow's renderer resolved the same feedback/drive parameters every sample.
  Those reads now occur once per control block. An unused third oscillator
  is skipped while its phase still advances. Exact signed shifts replace
  power-of-two divisions that the target compiler kept as hardware divides.
- The delay width product fits uint32_t; it no longer uses 64-bit division
  in the quarter-rate effect loop.
- Snow enables the already-built second core before audio starts, once for
  new devices and for settings saved with the old default. A saved marker
  preserves later explicit 1 CORE choices and the existing fault fallback.
  Settings reset also chooses two cores. Skiff's default, hardware startup
  routine, watchdogs and fault recovery are unchanged.
- GLO selected hidden shared-FX pages before the drawing code redirected
  them to GLOBAL with a "not for this track" notice. Navigation now selects
  visible pages directly, including remembered pages. Short GLO presses
  cycle MIXER/GLOBAL. SELECT changes tempo in those windows with either BPM
  lock preference. HOME SELECT continues to choose panels.

## CH release and the overload mechanism

The CH envelope's tempo coefficient is exercised through note-on, note-off,
voice allocation and the actual mixer. At 90, 120, 150 and 180 BPM its
amplitude reaches about 1% after two bars and the tail remains audible.
The original isolated `snow_amp()` test did not cover voice eviction.

The existing audio overload guard requests voice shedding after two
consecutive halves above 85% of the deadline. Released voices are its first
victims, so a 2 BAR envelope can be cut short despite the correct parameter.
The new regression reproduces that eviction explicitly. The guard is
preserved; disabling it would leave audio deadlines unprotected. The 99%
reading supports CPU overload as a cause, but the repaired binary still
needs measurements on the same physical device.

## Validation and limits

`tests/snow_runtime_test.c` stores hashes captured from unmodified Snow
0.7.3 (`25007e2`). All 60 factory sounds and nine dry/active-FX scenarios
match exactly after optimisation. Other tests cover exact BYPASS, no stale
sound after individual/rack re-enable, GLO navigation and physical encoder
gestures. `tools/snow_preview.py` includes these regressions.

Native/sanitizer tests, WebAssembly and a target firmware build do not
measure FM-1 CPU headroom. The browser's virtual clock advances outside
the audio calculation: its simulated DMA does not miss a physical deadline
when C synthesis takes too long. A clean browser render is insufficient.

Before calling a candidate hardware-qualified, measure the same FM-1 on
one core at STOP/idle, with a held CH note and its release, and during
six-track playback with the intended effects. Record CPU, audio late-half
count/maximum render time where available, responsiveness and audible
distortion. A half-buffer deadline is 128 / 44100 seconds, about 2.90 ms;
the pre-existing shedding threshold is 85% of that. Reducing a host
benchmark time is not a substitute for these measurements.

Repeat those measurements in the new default two-core mode and confirm
that the second core actually takes track jobs. A menu label alone cannot
prove the work was distributed.

No bootloader, audio hardware registers, flash map, clock switching,
firmware update protocol, sound-generation parameter ranges, factory
presets, or channel artwork are changed by this repair. No physical device
was flashed by the development tools.

## Deferred screen work

The user reports the physical screen text is unreadably small except the
large TONE label. A future interface pass should divide HOME content into
readable views and test text at native screen size. That redesign is
explicitly deferred; this runtime repair does not shrink, replace or
rearrange the channel artwork or parameter pages.
