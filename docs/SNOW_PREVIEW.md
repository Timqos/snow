<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Snow 0.6 — sound design with an instrument body

[Open the simulator](https://timqos.github.io/snow/snow/?v=06) ·
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
