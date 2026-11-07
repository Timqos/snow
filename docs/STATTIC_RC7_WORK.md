# STATTIC rc7 — strict four-voice CH

The user reports an onset rasp lasting up to roughly half a second: rarer
when CH plays single notes with BS/KK, more frequent with CH chords; BS/KK
alone are clean. The user then explicitly requests a lower CH-only voice
limit so a triad plus three old tails cannot make six active CH voices.
Choose four rather than three to retain four-note seventh chords.

Implementation scope:

- Define CH's engine voice cap as four. Count held notes, release tails and
  fading voices together. Existing allocator reuses slots at capacity; no
  extra shadow/fade voices or pending audio tails are added.
- Preserve the underlying eight-slot track representation, other five engine
  caps, shared eight/twelve voice budget, core scheduling, synthesis, gains,
  effects, presets, random generation, interface and fonts.
- Keep the CH release parameter at its selected value, including startup
  2BAR. At capacity, a newly played note can replace an old tail before its
  nominal release ends. Explain this requested tradeoff in the manual.

Validation plan:

- Check actual note allocation immediately after every note-on/off and every
  rendered block: CH active count and actual rendered count must never exceed
  four, including release and fade reuse. Exercise successive triads/sevenths,
  NOTES, UNISON and five other channels, on one/two real host workers.
- Replace the earlier eight-voice-overlap regression with the requested
  four-voice behavior; retain real tempo-derived release timing below capacity.
- Keep existing factory/FX PCM references, generator/transport/keyboard tests,
  native/WASM/worker checks, sanitizers and target memory/loop budgets.
- Audit the exact rc7 binary, previous rc6 upgrade/downgrade and factory
  return; verify browser controls and published file hashes.

The cap reduces maximum CH voice work; per-channel rack/effect work still
runs as configured. It does not by itself prove the hardware rasp cured.
No physical FM-1 is connected to this environment.
