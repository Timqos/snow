# STATTIC 0.8.0-rc9

Experimental FM-1 candidate, previously named Snow. The five user HOME
mockups in `assets/stattic-home/` and the confirmed control captions are
the interface reference. See [the recorded agreement](STATTIC_WORK_PLAN.md).

## Changes in rc9

Hardware rc8 result: five channels without CH stay clean at up to 60% and
L does not rise; adding CH gives rasp, up to 80%, and rising L. This happens
during ordinary live playback, with no USB audio recording. USB-recording
remarks concerned the user's earlier SLOOP test, not this STATTIC failure.

- Place the exact SINE (2,050 B), SVF_G (256 B) and TANH_Q15 (514 B) lookup
  tables in ordinary data SRAM on the STATTIC target. The code was already
  in executable SRAM but every sine interpolation still fetched two samples
  through flash/XIP. Startup copies the unchanged 2,820 B before either core
  or audio starts. No new boot hook, HAL change or audio-time buffer copy.
- Add read-only SYSTEM timing rows after CPU CLOCK: AUDIO PEAK, AUDIO LAST,
  CH SYNTH, CH FX/MIX, CORE1 WAIT and AUDIO IRQ. Keep the largest measured
  half duration separately; stage times belong to the latest missed half,
  or to the busiest half before any miss. Clean halves cannot overwrite the
  failure's stage snapshot. Measurements are wall times and can overlap.
- Preserve CH4, all synthesis/FX/parameter/generator behavior, gains, presets,
  random seeds and the full HOME layout. Default/non-STATTIC generated table
  declarations preprocess identically to rc8. Verify actual linked SRAM
  addresses and startup-image bytes against immutable pre-move table hashes.

This removes hot table reads from flash, with no claim yet about physical
CPU savings or a cured rasp. The timing rows provide hardware evidence if
the problem persists. SLOOP's three synth parts, drum unit and shared effects
are a different workload from six STATTIC instruments with independent racks;
SLOOP does support synthesized chords. No such architecture is removed here.

## Changes in rc8

Physical rc7 testing correlates each residual attack rasp with an increase
in SYSTEM's L counter (66 to 92), at up to 78% averaged CPU on two cores.
Random kick sounds change its occurrence; other channels may also provoke it.
This establishes missed audio deadlines during the fault, without identifying
one channel as its sole cause or proving that the four-voice cap was enough.

- Fix a reproduced scheduling error: an active zero-sustain voice can render
  no samples. Comparing active allocation against last rendered count adds a
  fictitious onset every block, inflating that track's scheduling weight.
  Compare against the previous allocation count instead. The regression fails
  on rc7 and verifies no accumulating false cost over 256 silent-held blocks.
- FM CHAIN computes the modulator phase first and evaluates its waveform once.
  The previous first evaluation was discarded. All algorithms, oscillators,
  phases, feedback and noise state retain exact integer results.
- Run the synth envelope in executable SRAM alongside oscillator rendering,
  including its inlined helpers. Target assembly must show no flash calls
  from these three audio functions. No data-pool allocation is added.
- Preserve strict CH4, release settings, other voice budgets, sound parameters,
  presets, random streams, effects, gains, interface, fonts and controls.
  An immutable rc7 reference checks 2,592 manual sound configurations and
  their final voice state; existing audio and worker-equivalence checks remain.

These remove unnecessary work and a real estimator bug. They do not establish
physical FM-1 timing or promise the rasp is cured before a hardware retest.

## Changes in rc7

The user explicitly requests fewer CH voices, so three notes plus three
release tails must not make six active CH voices. Set CH's engine cap to
four, counting held, released and fading voices, in NOTES/CHORDS and manual
UNISON. Triads and four-note chords remain available. At capacity, existing
allocation reuses an old slot rather than adding a hidden release voice;
old tails may end before their selected 2BAR duration. The underlying array,
other five channel caps and global 8/12 voice budget remain unchanged.

This lowers the maximum CH voice-rendering work. Rack/effect work still runs
as configured; a physical FM-1 test is required to assess the reported attack
noise. The interface, fonts, presets, generation, gains, effects and release
parameter values are unchanged. The manual now explains the tail tradeoff.

## Changes in rc6

- Keep the complete rc5 interface, typography, controls, presets, random
  streams, synthesis architecture, effects, gains and 8/12 voice budget.
- Inline the exact sine/triangle arithmetic in the SRAM oscillator loop,
  avoiding helper calls and waveform dispatch for these common shapes. Every
  other shape retains its original path. The existing target budgets remain.
- Cache each synchronized LFO's exact phase increment until its period
  changes. All six tracks and both clocks remain independent; rate, phase,
  trigger, fade and tempo/division changes retain their previous behavior.
- Compare 512 low-note retriggers under held CH against unmodified rc5 PCM,
  including generated sounds, effects, quiet MASTER, USB fixed level and
  tempo/LFO-division changes. All eight captured scenarios must match exactly.
- Fix a missing conditional around STATTIC's per-boot diagnostic initializer
  in the generic, non-STATTIC browser build. The broad host suite's remaining
  song, browser/native equivalence and converter checks pass after the fix.

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

rc4–rc6 used eight existing CH slots to preserve overlapping releases. In rc7,
the user chooses a hard maximum of four active CH voices to reduce load,
including tails. A new triad cannot grow the working set to six voices.
Incoming notes reuse old slots at capacity. The global budget remains eight
on one core / twelve on two; the other channel caps and overload guards stay.

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

After rc4, the user reported the Ring/resonator distortion gone. After rc5,
the user reports substantially fewer clicks: CH + BS remains more susceptible,
CH + KK rarely clicks and the result varies with the generated kick sound.
Clicks are at the low-note attack and remain audible with very low physical
MASTER. The user has not yet checked whether L rises on those residual clicks.
Earlier hardware reports showed 2 cores, 76–80% average load and L rising on
attacks; SLOOP uses the same free-DMA-half diagnostic.

This supports investigating peak processing deadlines, without proving every
click is a deadline miss. The digital pair test checks 800 factory combinations
at default/full levels and pre/post-master attenuation. A loud coherent chord
can engage the existing protective limiter. With USB LEVEL FIXED, MASTER acts
*after* the limiter, so reducing physical MASTER cannot by itself rule out
internal limiting. Default levels with 20% pre-master do not engage the limiter
in that stress test; the four held CH voices remain intact in every case.
The user now authorizes a strict CH-only four-voice cap to reduce load. Gains,
filters, effects and synthesis parameters retain their existing behavior.
Retest CH attacks with BS/KK and observe L; the physical noise is not yet
confirmed resolved.

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

The rc9 candidate requires the existing target budgets, complete STATTIC
native/WASM/audio suite, layout sweep at default/minimum/maximum values,
eight-entry Undo and onset-prediction contracts, and ASan/UBSan checks.
Desktop/mobile browser checks and 28 exact-package update/rollback checks
must pass, retaining previous rc8. Broader upstream UI checks retain all 3,458
golden pictures with zero layout/alignment findings.
The exact package, source commit and update/rollback results are recorded in
[the published build](https://timqos.github.io/snow/snow/build.json) and
[the binary audit](https://timqos.github.io/snow/snow/update-audit.json).
Final region sizes and release verification are recorded in
[the work log](STATTIC_RC9_WORK.md). Physical audio deadlines require FM-1
qualification; desktop or browser timing is not a substitute.

The target regression gate retains all unaffected Skiff limits and the
byte-reproduced rc3 reference for existing STATTIC paths. Oscillator SRAM
placement adds no per-sample divisions. The 8/12 shared voice budget,
256-frame hardware halves, 32-sample control blocks and all synthesis/effect
parameters retain rc8 behavior and the requested CH-only voice cap. Installation/loader code is unchanged
from rc4; every new binary still requires its own update/rollback audit.
