# STATTIC 0.8.0-rc5

Experimental FM-1 candidate, previously named Snow. The five user HOME
mockups in `assets/stattic-home/` and the confirmed control captions are
the interface reference. See [the recorded agreement](STATTIC_WORK_PLAN.md).

## Changes in rc5

- All HOME bottom helpers use the same 8×14 pixel face. OCT actions, page
  navigation and every knob assignment retain their full wording. Comparison
  against rc4 confirms identical upper pixels on all five HOME pages.
- Other editors, dialogs, NAME and SYSTEM use enlarged pixel type: 14-pixel
  helper/compact-label capitals, 18-pixel menu labels, 20-pixel values and
  28-pixel large values. Spacing adapts to available width without dropping
  source glyph columns. Settings show three tall rows with labels above values;
  PRESETS scrolling, tabs and all settings remain available. No larger atlas,
  framebuffer or audio-thread drawing is added.
- The compiled oscillator renderer and waveform helper run in fast executable
  SRAM; every renderer call stays in SRAM. DSP mathematics, gains, effects and
  voice budgets are unchanged. Two of eight Undo snapshots move to the existing
  data pool to make room; all eight entries and redo remain available.
- Before each two-core split, newly active voices add estimated work using the
  measured per-voice peak. Measured costs rise immediately and decay slowly,
  avoiding the old smoothing delay at a new KK/BS/CH attack. Split weights affect
  scheduling only; equal-budget one/two-core PCM remains identical.
- SYSTEM's L now subtracts the retained counter at application boot. L therefore
  counts this boot's missed DMA deadlines; raw crash diagnostics remain retained.

## Repair in rc4

- HOME's empty lower visualizer strip/divider is removed. Bottom helpers now
  use larger type in two columns/two rows (three columns for pattern selectors),
  without moving the upper layout. Other editor graphs remain available.
- HOME 3 removes its small icon, moves the selected field left and displays
  VOL/PAN/FLR/WDT's value as a percentage, not a live audio meter.
- All four HOME 2 generators prepare off the audio thread and enter at the next
  shared bar during PLAY. They preserve the existing algorithms, random streams,
  parameter masks, detent counts and full synthesis/effect surface. Repeated
  knobs coalesce into one undoable candidate; STOP applies it immediately.
  Undo before the bar cancels it while retaining redo. Other tracks' locks and
  live mixer changes are retained; event-pool overflow rejects a whole batch.
- The real loader display reads STATTIC UPDATER. Only equal-length display
  literals changed: decompressed executable bytes are identical to Skiff 2.2
  after normalizing that brand string; headers/CRCs and all blocks are checked.

## Audio investigation and changes

The load-dependent crackle remains a physical-device report, not a fault that
host playback has reproduced. Source research included SLOOP 2.8.5
(`isod89/sloop-fm1`, `bac9c05`), Skiff issue 14/41 and Tonitor issue 11.
SLOOP uses 256-frame halves and volatile interrupt state. Its texture-related
issue 122 is not a STATTIC diagnosis: STATTIC has no SLOOP TEXTURE generator.

STATTIC now uses 256-frame hardware halves (5.80 ms, previously 2.90 ms).
This reduces interrupt overhead and gives short scheduling spikes more room;
it does not halve DSP work or prove a physical overload has gone away.
Control blocks remain 32 samples, so synth/envelope/generator resolution is
unchanged. Audio DMA memory increases by 2048 B; it remains within RAM budgets.
Unused HOME scope capture stops; non-HOME scope/editor functionality remains.

The two-core split now measures each track's actual synthesis and complete
rack work, instead of estimating only the number of voices. Gray-code mask
search gives the same optimal split/tie choice with less repeated summation.
The in-audio flag is volatile across nested TIMER5 interrupts. CPU display
includes nested interrupt time, since that time also consumes the DMA deadline.
SYSTEM → CPU also shows L: actual DMA deadline misses since boot, capped at 999.
Exact reciprocal arithmetic replaces hot per-sample /127 operations; sample
products, full-range int32 cases and immutable factory/FX PCM hashes are checked.

CH's old four-voice cap was separately reproduced cutting old release notes
when a new four-note chord arrives. It now uses all eight already allocated
CH slots. No new voice storage or shared-budget increase: 8 voices on one core,
12 on two cores as before. A real sequencer/mixer test retains four old released
notes alongside the next four-note chord. More simultaneous held/released notes
can still exhaust a finite shared budget; deadline guards are retained.

Research links:
- https://github.com/isod89/sloop-fm1
- https://github.com/illuminus5/skiff/issues/14
- https://github.com/illuminus5/skiff/issues/41
- https://github.com/nzimas/tonitor/issues/11
- https://github.com/isod89/sloop-fm1/issues/122

## Shared bar and four-bar phrases

HOME pattern launches and page-5 mute/unmute/solo/unsolo now wait for the next
shared transport bar during PLAY. The audio transport commits them before
any channel ticks, including tracks with unequal lengths, divisions and swing.
Incoming patterns start at their first step in the same audio block. RANDOM
PATT prepares outside the interrupt and publishes its group in one short guard.
USB/TRS clock uses the same externally clocked transport. STOP applies pending
commands immediately; repeating a pending M/S press cancels it. Outlined M/S
cells show pending state; filled cells continue showing the active state.
Leaving HOME retains the queue; project loading clears it and transient solo.

All six channels boot with LENGTH 64, including restored older sessions.
64-step generation creates four varied bars; only bar 1 may repeat in bar 3.
Bars 2 and 4 differ rhythmically, with melodic/harmonic development across the
phrase. Explicit 0/100% density and melody-only rhythm preservation remain.
Native tests cover 128 phrases per role across five densities, shared-bar
launches at 90–180 BPM, external USB clock, replacement/cancel, stopped
commands and project restore. WASM tests exercise the physical controls and
an actual restored-short-session reboot.

## Interface and identity

- Five native 240×240 HOME pages; SELECT changes page, never HOME tempo.
- Enlarged transport/BPM/position/brand/battery strip on all five pages.
- Page 1: four macros, large active parameter/percentage, preset and pattern.
- Only CH: A (chords) / N (notes) beside the preset, synchronized with SEL.
  On HOME 1/5, hold OCT+ alone for 700 ms to toggle once; a short tap changes
  octave on release. The helper reads `OCT+ HLD N/CHO`; HOME hold retains settings.
- Page 2: K1 phrase+locks, K2 sound, K3 LFO, K4 FX; OCT− current pattern clear,
  OCT+ current patterns of all six channels clear. Saved other positions and
  synthesis settings are retained. One physical detent performs one generation.
- Page 3: VOL/PAN/FLR/WDT; width includes the channel’s stereo effects.
- Page 4: P1–P3 for all six channels. K1/K2/K3/K4/ALGORITHM/PRESETS address
  KK/SD/HH/PR/BS/CH. OCT± chooses existing saved patterns, without generation.
- Page 5: K1 selects channel, K2 adjusts its volume. The first six WHITE keys
  toggle MUTE, the first six BLACK keys toggle SOLO. Both are latched toggles,
  not sounding notes; selected-channel state is separate from mute/solo state.
- The supplied static art is packed as deduplicated one-bit rows. One detent
  moves the whole image six vertical rows, wrapping in its fixed rectangle.
  No image decoder, additional framebuffer or audio-thread animation.
- Page-5 volume fills use the supplied pixel texture. GLO/held SEQ reuse the
  large six-channel views with captions matching their existing controls.
- STATTIC replaces the visible firmware/about/editor identity, site header,
  installation UI, backup download name and firmware download name. Startup
  uses the user’s new nine-ribbon circular logo with a 1.5-second fade/hold.
  Historical internal API/format names and model ID FM-1_700 stay compatible.

## OCT command precedence

The explicit held-SAVE undo window takes priority over HOME page-2 clear
and page-4 random-pattern commands. OCT+ redoes the undone action rather
than clearing all channels. Native tests exercise redo/undo on all six roles.
Pending launches and missing saved alternatives produce explicit messages.

## Sound and playback

All ten kick factory patches have a stronger fast pitch transient, sufficient
filter opening and low body. They retain distinct timbres, mostly short tails
and several longer envelopes. Actual first-20ms peak/RMS and spectral/duration
checks are part of the audio audition, in addition to parameter checks.

Dedicated FX randomisation has a clearly audible lead effect (delay, reverb,
chorus, Tape, crush or insert). Feedback remains bounded. EDIT sound generation
retains its gentler effect budget and instrument-role constraints. Random FX
does not introduce a slicer on bass/chord; that manual control is still available.

Phrases use additional internal rhythmic constructions and fresh variations.
Bass has melodic riffs, sustained/tied notes and occasional slides. Chords use
harmonic progressions, inversions, occasional sevenths and longer tied notes.
Generated bass/chord locks favour filter, pan, ambience and controlled colour;
CH locks do not suddenly replace its FM tone, envelope mode, decay or release.
Manual locks and the complete synthesis surface remain available.

CH retains a real tempo-derived two-bar startup release. Synchronized envelope
coefficients are cached per track, including correct reload/tempo/macro changes.
Zero-weight SUB/NOISE/additive blends skip arithmetic while RNG/phases continue;
unchanged factory sounds retain their original immutable audio hashes.
The always-running HOME scope and old duplicate channel/icon drawing path are
replaced by dirty static strips. The original audio deadline/voice guards remain.

## Physical report and verification limits

After rc4, the user reports the Ring/resonator distortion gone. Remaining
short clicks occur on CH note/chord attacks when BS or KK joins it; CH alone
or with HH/PR is clean. The device shows 2 cores, about 76–80% average load,
and L increases on the attacks. Turning physical MASTER down to 20% reduces
volume but retains the click. SLOOP uses the same free-DMA-half check for L.

This supports investigating peak processing deadlines, without proving every
click is a deadline miss. The digital pair test checks 800 factory combinations
at default/full levels and pre/post-master attenuation. A loud coherent chord
can engage the existing protective limiter. With USB LEVEL FIXED, MASTER acts
*after* the limiter, so reducing physical MASTER cannot by itself rule out
internal limiting. Default levels with 20% pre-master do not engage the limiter
in that stress test; the four held CH voices remain intact in every case.
No automatic bass filtering, gain reduction, voice reduction or effect removal
is used to mask the report. SRAM placement and onset-aware splitting need an
FM-1 retest, observing whether L stops increasing at the audible click.

Native/WASM tests cover actual playback, parameter-lock ownership, finite
stereo output, physical encoder/key routing, and a one-minute six-channel
sequence with generated locks and strong FX. CH’s real mixer release is checked
again after that playback at 90/120/150/180 BPM. Seven real host worker splits
compare one/two-core output. These tests do not measure physical FM-1 deadlines.

The ten kick first-20ms attack measurements pass. Fifty unchanged factory
sounds and nine active/dry FX paths match the old immutable audio hashes;
the redesigned kick sounds are intentionally excluded from that equality test.
Long-gap FX/bypass/history tests, musical-scale/mode checks, startup release,
pattern persistence and browser-save compatibility remain in the suite.

## Current verification

The rc5 build must pass the existing target budgets, complete STATTIC
native/WASM/audio suite, layout sweep at default/minimum/maximum values,
eight-entry Undo and onset-prediction contracts, and ASan/UBSan checks.
The exact package, source commit and update/rollback results are recorded in
[the published build](https://timqos.github.io/snow/snow/build.json) and
[the binary audit](https://timqos.github.io/snow/snow/update-audit.json).
Final region sizes and release verification are recorded in
[the work log](STATTIC_RC5_WORK.md). Physical audio deadlines require FM-1
qualification; desktop or browser timing is not a substitute.

The target regression gate retains all unaffected Skiff limits and the
byte-reproduced rc3 reference for existing STATTIC paths. Oscillator SRAM
placement adds no per-sample divisions. The 8/12 shared voice budget,
256-frame hardware halves, 32-sample control blocks and all synthesis/effect
parameters remain unchanged from rc4. Installation/loader code is unchanged
from rc4; every new binary still requires its own update/rollback audit.
