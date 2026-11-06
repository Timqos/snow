# STATTIC rc5 — readable pixel type and CH onset investigation

User requirements: enlarge text outside custom HOME; give all HOME bottom
helpers one maximum practical pixel face with complete wording; preserve the
upper HOME layout and all synthesis, effects, generators and sound controls.

Hardware report after rc4: Ring/resonator distortion is gone. CH alone and
CH + HH/PR are clean; adding BS or KK causes a short click on note/chord
attacks. Two cores, 76–80% average CPU. L rises on the attacks. MASTER at
20% makes the click quieter without removing it. Physical hardware is not
connected to this development environment.

Implemented:

- HOME lower helpers: one 8×14 face, complete captions. All five upper
  regions compare pixel-identical to rc4's native frames (180 rows on pages
  1–4, 190 rows on page 5). Upper art, icons, labels and controls remain.
- Other editors: 14/18/20/28-pixel capitals, adaptive horizontal spacing,
  full-width settings labels and values on separate lines, three visible
  settings rows with unchanged scrolling/tab controls. Reuse the old 5×7
  binary source atlas; no new framebuffer, cache or audio-thread drawing.
- Oscillator render/waveform code in executable SRAM. Target disassembly
  checks both functions and every render call remain there. No DSP math,
  gain, architecture, parameter, effect or voice-budget reduction.
- First-block work prediction before the core split for newly active
  voices; measured costs rise immediately and decay slowly. Eight-entry
  Undo retained by relocating two cold snapshots to the existing data pool.
- L is truly per application boot, subtracting the initial retained count
  instead of displaying missed halves inherited across warm restarts.

Diagnosis boundaries:

- SLOOP uses the same free-half transition to count late DMA work. A rising
  L is evidence of deadline misses; average load cannot exclude those peaks.
- A loud coherent four-note CH plus KK/BS can engage the existing limiter.
  USB LEVEL FIXED puts physical MASTER after that limiter, so the MASTER
  test alone cannot exclude internal limiting. The pair test exercises
  default/full track levels and pre/post-master attenuation, and checks
  bounded PCM and four-voice continuity in all 800 combinations. Default
  track levels at 20% pre-master do not activate the limiter in this test.
- No physical claim of a cured click until the new package is tested on
  FM-1. Retest CH + BS and CH + KK while observing the change in L.

Release validation and exact published package details will be appended
after the final build and matching-binary audit.
