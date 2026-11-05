<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Snow 0.7.1 — six instruments, one Snow identity

[Open the simulator](https://timqos.github.io/snow/snow/?v=071) ·
[Install & back up](https://timqos.github.io/snow/snow/install/)

Snow is custom firmware for the M-VAVE FM-1. Its browser simulator runs the
same C synthesis and interface through WebAssembly. Physical FM-1 validation
is still required; the package has passed software and memory-budget checks.

## HOME: three panels, one selected instrument

ALGORITHM selects KK / SD / HH / PR / BS / CH; PRESETS browses its ten factory
sounds. The preset name occupies one fixed pixel row under the active macro.
SELECT has three clamped positions, with three arrows turning together:

| Arrows | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| Left / SYNTH | Tone | Shape | Sweep | Decay |
| Right / GEN | Sound | Phrase + locks | LFO | FX |
| Up / MIX | Volume | Pan | DJ filter | Stereo width |

Each generator detent, in either direction and without acceleration, generates
one result for the selected channel. GN/SEQ calls the existing phrase and
parameter-lock generators in one undo transaction. Hold SAVE to undo the phrase and its locks together. The original random stream keeps advancing,
so generating after undo does not repeat the cancelled result. OCT± remains octave on HOME (the existing undo
window still uses OCT−/OCT+ for further undo/redo). The original EDIT/LFO/FX
OCT+ commands and separate SEQ GEN RND / GEN PARM remain available.

HOME SELECT never changes tempo. GLO retains its existing tempo controls:
open its normal window and use SELECT with the existing BPM-lock preference;
keeping GLO held is not required. Existing held-GLO shortcuts also remain.
Panel focus stays unchanged when selecting another channel. MIXER and PATTERNS
still show all six channels on one screen.

The ten reference motifs are stored as fixed pixel grids; repeated rows share
storage without losing pixels. The icons use the selected theme colour.
Local details respond to knob position or generator actions, with no idle
animation or moving icon bounds. Pixel progress bars and three arrows identify
the controls without adding panel frames. The bottom scope shows the master
output; its note label belongs to the selected channel.

On CH HOME, hold HOME and press OCT+ once to toggle N/NOTES and CHO/CHORDS.
Keeping OCT+ held does not repeat; octave and panel focus stay unchanged.
Ordinary OCT+ remains immediate octave-up. Releasing HOME after the combination
does not open settings; holding HOME on its own still opens system settings.
The switch under the cassette shares its state with SEL page 1. Held HOME retains its existing system-settings action.

### Performance filter and width

FLR is a per-channel DJ filter after its stereo effect rack: 50% neutral,
below 50% removes highs, above 50% removes lows. WDT includes the full channel
and its effect returns: 0% mono, 50% original stereo, 100% twice the side
component. It preserves the mid component and does not create stereo from a
mono source; SPREAD or stereo effects can supply that content. PAN is applied
independently afterward. Controls are smoothed. Neutral defaults use the exact
previous audio path. The two controls reuse previously unused, zero-default
lane-decay slots; stored IDs, P_COUNT and the project format do not change.
Sound/preset generation preserves these performance settings. The selected
factory preset name stays visible while editing; it is a starting-point label.

## Six instruments, deep synthesis

KICK / SNARE / HAT / PERC / BASS / CHORD retain fixed roles and ten factory
sounds each. Three oscillators, six algorithms, multiple waveforms, full ADSR,
pitch/FM envelopes, filters, two LFOs and modulation remain manually editable
through 27 EDIT pages. Each channel has independent delay/reverb/chorus state
and tails, Tape, bitcrush, drive and insert effects. Manual ranges remain open.

## SEL: scales and your own chords

The first SEL page has MODE, ROOT, SCALE and QUANTIZE on K1–K4. CHORDS
expands one key to the shape and voicing on SEL page 2. NOTES plays each
key as one note: on CH, play up to four simultaneous notes to make your own
chord, and record it live with REC + PLAY or into a stopped, armed SEQ step.
QUANTIZE OFF permits chromatic chords; other quantisation modes retain their
scale mapping. Shape/voicing are remembered when switching modes. Transpose
is on page 2. Existing recorded notes are not rewritten by a mode change.
New generation makes single-note CH phrases in NOTES and diatonic chords
in CHORDS; rhythm-only generation keeps the existing pitches.

There are 35 scales, including every name in the supplied Ableton Scale
screenshot: Spanish 8-tone, Bhairav, Dorian #4, Hirajoshi, Insen, Iwato, Kumoi,
Super Locrian, Lydian Augmented/Dominant, Harmonic Major, Messiaen 3–7,
Hungarian Minor and both melodic-minor directions, plus the existing modes,
pentatonics, blues, whole-tone and diminished scales. Melodic Minor Down
has natural-minor pitches. Original scale indices 0–15 remain unchanged;
new scales are appended. These are musical pitch sets, not imported Ableton
preset files. The full scale name appears below the on-screen keyboard.

## CH: a two-bar release

Every CH factory sound defaults to 2BAR release. Each device/simulator startup
applies 2BAR again after restoring the previous session, so an older autosave
cannot override that startup default. Only CH is affected. The actual amplitude tail
follows BPM (or the measured external clock), independently of ENV SYNC;
attack and decay retain their millisecond controls when ENV SYNC is off.
EDIT → OCT+ preserves the release, including a manual adjustment. LFO/FX
randomisation also leaves it unchanged. Other channels retain their existing
envelope timing. Loading a CH factory sound restores its two-bar default;
saved projects retain their stored release value on the tempo scale.

## Smart RANDOM EDIT

On the first EDIT / MACROS page, OCT+ generates a fresh instrument sound across
all EDIT pages. Hold SAVE to undo. Other pages keep their ordinary OCT+ action.

Generation coordinates register, oscillator structure, envelopes, filters,
modulation and effects instead of drawing unrelated full-range values:

- Kick: low fundamental and short pitch transient.
- Snare: pitched body and coloured noise spring, including softer variants.
- Hat: high-frequency noise with a restrained metallic component.
- Percussion: pitched skin, wood, metal and noisy-body variations.
- Bass: monophonic gated voice, low register and stable note tracking.
- Chord: polyphonic gated voice, harmonic oscillator ratios and sustained body.

Identity-defining settings stay coherent. Modulation is shallow and timbral;
chorus/delay feedbacks are bounded. Sends leave the dry instrument in front.
Only one gentle drive/Tape/bitcrush/insert colour is active at once. Bitcrush
keeps 12–16 bits at low mix. A 32-bit per-channel seed walk creates new parameter
combinations, not a rotation through factory presets. The parameter space is
large; software checks sample thousands of outputs rather than claiming to
have listened to every possible sound.

Notes, rhythm, locks, generator controls, mixer, transport and other channels
are preserved. Undo restores the preceding sound. User-written automation or
Control All offsets remain part of the performance and can still alter it.

## Focused LFO and FX randomisation

OCT+ on the first LFO page randomises 35 modulation controls across both LFOs,
sync/phase/fade/depth and the four MOD slots. LFO 2 and depth pages are available
inside the LFO menu as well as EDIT. The shared MOD page includes the fixed
envelope routing amounts; these also receive small bounded values. Amp ADSR,
oscillators, base tuning, FX settings and sequences are retained.

OCT+ on the first FX page randomises all 40 controls across its pages, including
Tape/Bitcrush, inserts, slicer and ducking. Sends/feedback are bounded, one gentle
colour processor is active at a time, and slicer/duck depth cannot fully mute the
part. A native undo flag restores those two effects along with the sound FX while
leaving mixer/sequence controls alone.

Both commands use fresh generated combinations, change only the selected channel
and retain steps/locks and transport. Hold SAVE for undo. OCT+ on later pages,
held-button layers and modifier combinations retain their usual actions. The
first-page footer advertises the action and keeps it clear of graph updates.

## Phrase generation and performance

SEQ → SELECT to SEQ GEN. TARGET defaults to the selected channel’s short name,
or choose ALL. OCT+: GEN RND; OCT−: GEN PARM; SAVE + OCT−: CLEAR. K4: PARM AMT.
The grid shows generated notes, velocity, locks and retriggers. Pitched parts
respect the scale. Hat/percussion ornaments are sparse and limited to one
ornamented step per second musical bar. Musical influences are internal,
without a genre selector. The generator from 0.5 is retained.

Hold SEQ for six independent pattern columns. K1–K4 control channels 1–4,
ALGORITHM bass and PRESETS chords. OCT±: A–H / I–P. Black keys choose a channel;
white keys its pattern. Launches are quantized during playback.

Hold GLO for six levels, mute and solo. The six encoders have the same mapping.
First six black keys: mute; first six white keys: held solo. E4: unmute all;
F4: fill; G4: tap tempo. GLO + SELECT: tempo. GLO + OCT−: restore mixer snapshot.
GLO + EDIT + K1–K4: Control All; GLO + EDIT + OCT−: selected channel’s inclusion.
GLO + SAVE: RAM snapshot; GLO + HOME: reload all; GLO + OCT+: reload selected.
Use SAVE’s PROJECT pages for permanent storage. RAM snapshots do not survive
power-off. The pool has 64 unique patterns, 16 positions per channel; deferred
writes have finite capacity, so stop when the device asks for space to save.

## Startup and visual identity

The supplied snowflake is a 7×7 pixel symbol with a central negative-space cell.
At startup its blocks grow in place, from the inside out, and the Snow name fades
in beneath it. A readable hold completes a 1.5-second sequence before HOME.
Rendering is capped at 20 fps and stops after the reveal. The existing canvas is
reused; no framebuffer is allocated and no animation runs in the audio IRQ.
The browser uses the same timing and native renderer as the hardware build.

The same symbol brands the English site and installer. A concise introduction
precedes the simulator, followed by installation, feature details and the manual.

## Interface and browser page

All native pages/menus use pixel fonts and ten system colours. HOME shows the
selected channel and a bottom scope. Reference icon details respond to absolute
knob position or generation events while their silhouettes stay recognisable. Bounds/centres are fixed, reverse
motion is reproducible, there is no idle animation clock, and icons use the
selected theme ink.

The English simulator page retains the device panel. The install button and
backup/recovery notes sit immediately below it, followed by a firmware overview,
quick start and button manual. The duplicate parameter/preset builder is removed.
The page disables detailed per-frame editor metadata to reduce worklet allocation.
Browser storage 0.6 copies compatible 0.5 data first, falling back to 0.4; earlier
archives remain untouched. Browser saves are separate from a physical FM-1 backup.

## Installing

Use the Snow-specific installer, not a generic Skiff download. It verifies the
release SHA-256 and FM-1 identity before connecting. A supported device’s complete
backup is validated and downloaded before writing can be enabled. If the old
firmware does not offer this protocol, export with its own tools first. A loader
cannot export data; resume only with a backup saved before the interruption.

The user must confirm their backup and installation notices. Device/model changes,
failed backups/downloads and unconfirmed backup steps block writing. Stock or
other-fork projects are not automatically converted or restored. Keep the original
firmware and its tools for rollback. Direct USB, stable power, a desktop Web MIDI
SysEx browser, no competing MIDI apps, and an uninterrupted tab are required.
Physical recovery may require FM-1-transporter if the loader is unavailable.

The [installation and rollback audit](SNOW_HARDWARE_AUDIT.md) records the exact
package, unchanged boot/update components, tested transitions and the physical
qualification still required. Factory rollback must match the preserved V15/V16
startup head. Snow's musical data is not automatically portable to other forks.

## Verification and limits

Native, sanitizer and WASM tests cover role identity, playable register/envelopes,
parameter bounds, sound diversity, notes/locks/other-channel preservation, undo,
physical controls, generator variation, factory sounds, isolated FX and six-rack
audio. Generated page tests check English-only content, removal of the editor,
installation placement and browser-save migration. Installer tests use the exact
shipped modules with a real firmware package and simulated MIDI; no hardware is
flashed by these tests. Physical installation, CPU/SPI headroom and real device
audio remain unvalidated.

The role-aware designer is sampled with 4096 generated parameter sets per role
(24,576 total), without repeated sets in that sample. Another 64 sounds per role
are rendered through the C oscillators/filter/envelope: 384 distinct dry renders,
bounded peaks, low-frequency kick/bass bodies and broad high-frequency hats.
These measurements detect regressions; they do not replace listening or certify
every possible generated sound.

## Visual refinement in 0.7.1

Channel and active-pattern badges use 14-pixel-high type within the original
header cells (including P10–P16). The first five main icons occupy 72×72
pixels; the cassette keeps its 80×80 cell. All icon animation uses binary
theme ink with no shaded sweep. The HH eye arches move vertically while
keeping their shape, its mouth opens vertically, the cat’s ears follow its
eyes, the planet and globe details turn, and the right-side arrows, flowing
capsules, envelope and sparkle move their own parts. The SELECT:PANEL hint
is removed; the three arrows still indicate panel focus. Audio, generators,
parameters, project format and button mappings are unchanged.

## Risk, installation and factory return

Snow is unofficial, experimental third-party firmware. Install, update, use
and remove it at your own risk and responsibility. It is supplied AS IS without
warranty to the extent permitted by applicable law. To that extent its authors,
maintainers, contributors and distributors are not liable for device damage,
data loss, loss of use or resulting losses. Non-waivable legal rights and
liability are unaffected. See GPL-3.0 and the full installer notice. Physical
FM-1 qualification remains pending. This notice is not a recovery guarantee.

The installer page contains the installation checklist, risk acknowledgement
and matching-factory return procedure. Save a backup and retain the original
firmware/version before writing. This Snow installer only accepts the exact
Snow package. Factory return uses the unmodified package matching the preserved
V15/V16 bootstrap and the established USB-MIDI helper or supported vendor tool.
Do not guess the version or force an unrecognised package. The helper supports
recognised original V15/V16 files:

```sh
python -m pip install mido python-rtmidi
python tools/fm1_install.py --info
python tools/fm1_install.py "/path/to/matching-factory.fwsc"
```

Run from the source repository folder on a desktop computer; replace the path
with the compatible original file. Keep USB/power connected and follow the
helper’s confirmation and status. After factory boot, restore only the original
factory backup with its matching tools. Snow archives belong to Snow. If the
loader no longer answers, use the documented FM-1-transporter recovery route;
physical recovery may be necessary.
