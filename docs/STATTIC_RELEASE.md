# STATTIC 0.8.0-rc3

Experimental FM-1 candidate, previously named Snow. The five user HOME
mockups in `assets/stattic-home/` and the confirmed control captions are
the interface reference. See [the recorded agreement](STATTIC_WORK_PLAN.md).

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

The newest user report is **2 cores, up to 80% displayed CPU**: CH distortion
appears during playback and is absent after STOP. This does not identify a
single proven cause. Average load does not exclude brief deadline overruns,
and dense new chords can replace release tails within the unchanged four-voice
CH cap. Retain the existing two-core 12-voice shared budget and overload guards.

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

The target build fits all enforced budgets (pre-publication build):

| Region | Used | Limit | Free |
| --- | ---: | ---: | ---: |
| App slot | 572,300 B | 581,564 B | 9,264 B |
| Data + BSS | 86,600 B | 98,304 B | 11,704 B |
| Pool | 333,680 B | 356,352 B | 22,672 B |
| RAM code + ramx | 24,176 B | 24,576 B | 400 B |

The loader, startup head, flash map, USB/model checks, update protocol and
hardware register code are unchanged. The software update/rollback audit
must be rerun for the published binary, including original V15/V16 recovery.
No physical device is flashed by the development tests. After installing,
check actual peak/average load, UI response and CH playback/release on the FM-1;
do not mark the intermittent physical fault resolved on host evidence alone.
