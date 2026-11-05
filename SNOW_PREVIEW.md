<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Snow 0.6.3 — six instruments, one Snow identity

[Open the simulator](https://timqos.github.io/snow/snow/?v=063) ·
[Install & back up](https://timqos.github.io/snow/snow/install/)

Snow is custom firmware for the M-VAVE FM-1. Its browser simulator runs the
same C synthesis and interface through WebAssembly. Physical FM-1 validation
is still required; the package has passed software and memory-budget checks.

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

Every CH factory sound defaults to 2BAR release. The actual amplitude tail
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

All native pages/menus use pixel fonts and ten system colours. HOME shows six
cards and a bottom scope. Existing icon motifs derive their geometry from the
last touched parameter’s 0–100% position. Bounds/centres are fixed, reverse
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
