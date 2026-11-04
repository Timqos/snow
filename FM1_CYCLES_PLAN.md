# Snow / FM-1 — current brief

Latest user clarification after Snow 0.2: keep its deep synthesis controls,
but **fix channel identities and factory libraries** to KICK, SNARE, HAT,
PERC, BASS, CHORD. Ten presets of its own instrument per channel, 60 total.
Do not switch a channel's role/library to another instrument. Manual waveform,
algorithm, envelope and modulation edits remain free to radically transform
the sound; the role does not restrict these approved controls.

Provide genuinely independent per-channel delay, reverb, chorus and their
settings/state/tails, plus Tape and Bitcrush with dedicated controls. Keep
existing synth, filters, drives, inserts, two LFOs, locks and live RAM reload.

Holding SEQ opens a visible pattern grid. K1–K4 independently select their
channels' patterns; a second page covers channels 5–6. Eight visible slots,
quantized launching, and clear current/queued states. Tessera is a workflow
reference; don't copy its generators or sequencer implementation.

Generate varied rhythms/melodies **for the instrument and selected style**:
IDM, electronica, breakbeat, drum & bass, deep house, ambient and other styles.
Use seed, density, syncopation, complexity, variation, accents, humanization,
scale and contour. Support both one channel and a coordinated ensemble.
Preserve locks and independent pattern editing.

Develop in a branch/PR, build a genuine .fwsc in the cloud, test real C via
WASM and publish on GitHub Pages. No tools required on the user's Mac and
no hardware flashing. Retain upstream licenses. Physical FM-1 testing is
required before claiming hardware stability.

Current implementation and limits: [docs/SNOW_PREVIEW.md](docs/SNOW_PREVIEW.md).
