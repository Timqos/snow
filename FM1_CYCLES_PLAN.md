# Snow / FM-1 — current project brief

A sample-free, six-channel groovebox on Skiff/Felucca. The user's feedback
on Snow 0.1 supersedes the former fixed KICK/SNARE/HAT/PERC/BASS/CHORD roles
and the former exclusion of generative sequencing.

Every channel is a complete, freely configurable synthesizer: algorithms,
oscillator waveforms, ratios, feedback, noise, tuning, full ADSR, separate
pitch/FM envelopes, filters, two independent LFOs and modulation. Kick,
snare, hat, bass and FX are starting sounds available on **any** channel.
Retain the expressive FM synth character. Give kicks a usable low-frequency
body with an independently controlled transient, instead of a mandatory long
laser sweep. EDIT must expose extensive pages of actual sound parameters.

Develop original rhythm and melody generation: seed/variation, density,
syncopation, accent, scale, span, contour and optional live evolution.
Tessera is a workflow reference, not code or algorithm to clone. Each knob
on the pattern pages selects its channel's pattern independently; two pages
cover six channels with four physical knobs. Preserve locks, per-track
lengths, probability, ratchets, microtiming, swing, mute/solo and quantized
pattern launching.

Control All uses a channel mask and preserves locks. RAM Temp Save/Reload
returns all or one channel while playback continues; permanent project,
sound and pattern storage remains separate. Aim for independent per-channel
FX; shared sends do not satisfy the full FX goal. Benchmark hardware limits.

Use GitHub source, verified cloud toolchains and GitHub Pages for testing.
No tools need installing on the user's Mac. Work on a branch/PR, retain
licenses, build a real .fwsc and test the firmware's C code through WASM.
Do not flash hardware or call it stable without physical FM-1 validation.

Current capabilities and outstanding work: [docs/SNOW_PREVIEW.md](docs/SNOW_PREVIEW.md).
Authoritative detailed goals: [docs/SNOW_SPEC.md](docs/SNOW_SPEC.md).
