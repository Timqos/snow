# FM-1 Cycles — project brief

A sample-free, six-track FM groovebox for M-VAVE FM-1, using the open-source Skiff codebase as a starting point.

## Fixed tracks
KICK / SNARE / HAT / PERC / BASS / CHORD. Each needs rich, editable FM synthesis (pages of four parameters), and two independently assignable LFOs.

## Essential workflow
- 16–64-step patterns, per-track lengths, full step parameter locks.
- Control All: adjust compatible sound macros across six tracks simultaneously; allow excluding tracks. Do not destroy recorded parameter locks.
- Temporary Save / Reload for instantaneous return to a performance snapshot while playback continues; separate from permanent project saves.
- Probability, ratchet, microtiming, swing, mute/solo.
- Project/bank/pattern management via SAVE, borrowing the *interaction principles* of Tessera.
- Crisp pixel-styled high-contrast interface inspired by Digitakt/Digitone, mapped to four hardware knobs.

## Per-track effects
Independent filter, drive, chorus, delay, reverb and other appropriate effects with their own track settings, bypass and wet/dry. Do not silently substitute shared-send-only effects. Benchmark real hardware limits: six simultaneous independent reverbs may not be feasible.

## Quality requirements
Sound quality and underrun-free playback first. No sample playback, no unnecessary generative MIDI engines. Benchmark CPU and memory. Browser emulator tests are useful but cannot establish hardware audio reliability. Preserve applicable upstream GPL/license notices.

## No-local-install development workflow
GitHub source + cloud CI build using pinned, verified JieLi SDK/toolchain + browser emulator on GitHub Pages. Do not require Docker or other toolchains on the user's Mac.

## Milestone 1 (before changing synthesis)
1. Audit Skiff audio/voice code, six-track sequencer, parameter locks, web emulator, and build dependencies.
2. Establish a reproducible cloud build of **unchanged** firmware yielding .fwsc, or report specific blocking dependencies.
3. Run existing tests and create a browser preview.
4. Work in a separate development branch/PR. Do not flash the hardware.
5. Propose implementation phases and resource budgets for the six new engines and effects.

No first firmware is considered ready until build and emulator checks pass; physical FM-1 testing is required before calling it stable.
