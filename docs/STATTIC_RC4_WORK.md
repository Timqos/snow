# STATTIC next repair — active brief

User reports physical FM-1 crackle/ring distortion as channels accumulate,
regardless of activation order; CPU shows 2 cores, up to 80%. Notes played
over the sequencer can retain the glitch for 5–6 seconds. Generated CH steps
lose audible release although manual notes release. Do not cut synthesis,
effects, roles or the shared voice budget to disguise the fault.

Required changes:

- Remove the empty bottom visualizer area/divider from all HOME pages.
  Enlarge/reflow bottom control helpers into that space, leaving upper
  content fixed. Keep status/undo feedback readable without hiding controls.
- HOME 3: remove the small icon, shift VOL/PAN/FLR/WDT field left and show
  its selected parameter value in percent (user clarified: not a live VU).
- All four HOME 2 generator knobs must prepare changes off the audio thread
  and commit on the next shared BAR while playing. STOP remains immediate;
  preserve undo, seeds, note/parameter isolation and repeated detents.
- Replace SKIFF on the device's update-loader screen with STATTIC, while
  proving that loader protocol/flash behavior is unchanged.
- Research and compare open-source FM-1 firmware, explicitly SLOOP.

Research evidence (read Nov 6, 2026):

- SLOOP `isod89/sloop-fm1` at `bac9c05ea7627449405aa10e48da21d75b6c8c56`
  uses HALF_FRAMES 256; STATTIC currently uses 128. Source checked out at
  `/workspace/snow-research/sloop`. SLOOP measures worker busy/wait time.
- Skiff issue 14 documents real second-core use, variable hardware and the
  fallback. Issue 41 fixed block-stepped PHASE parameters; it is evidence
  for checking continuity, not proof of STATTIC's cause.
- SLOOP issue 122's comment points to TEXTURE for one sequence-only hiss;
  STATTIC has no such SLOOP texture generator. Do not call that our fix.
- Tonitor issue 11 describes load-dependent glitches despite two cores.
- STATTIC declares 8 physical voice slots per track but caps CH at 4.
  A generated 3/4-note chord can replace almost/all of the previous tail.
  Test overlapping chords and use existing CH slots without raising the
  shared 8/12-voice budget.
- Target disassembly still has hardware divisions inside Snow oscillator
  and effect sample loops. Test exact reciprocal arithmetic against `/127`
  and immutable PCM hashes before replacing them.
- CPU display excludes TIMER5 nesting; 80% is not a deadline guarantee.

State at start: source b713784 (rc3), published gh-pages 2cd24e4. Existing
rc3 tests and 25-check rollback audit passed; physical qualification pending.
No subagents authorized. Use GitHub connector for Actions/research; normal
git push and CLI workflow dispatch are already authorized. Preserve old
packages and backup/recovery checks. Final binary needs a fresh audit,
native/WASM/browser checks, budget checks and published hash verification.

Implemented: larger/reflowed HOME footers and HOME 3 parameter %, shared-bar
HOME 2 out-of-place candidates, unchanged random/design contracts, existing
8-slot CH overlap, measured per-track split/Gray search, exact hot arithmetic,
256-frame halves, volatile nested-IRQ flag, real deadline CPU accounting/L
counter, visible STATTIC loader branding with strict executable equivalence.
Non-HOME editor graphs remain; HOME alone stops unused scope capture.
Native repair contracts and ASan/UBSan passed, including generated-output
identity, coalescing, undo/cancel/redo, group-lock capacity, actual 4+4 CH
sequence overlap and exact split tie selection. Final full build, WASM,
browser, exact-binary rollback audit and publication are still pending.
