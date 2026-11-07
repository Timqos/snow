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

Completed validation:

- Strict CH-cap allocation and actual render-count contracts pass on one/two
  host workers, including triads/sevenths with BS/KK, NOTES, UNISON, all other
  roles and reuse of fading slots. Every count stays at or below four.
- The same contract passes ASan/UBSan. Full STATTIC native/WASM/audio tests,
  immutable factories/FX/onset captures, existing target SRAM/RAM/HAL/loop
  guards, real two-bar release below capacity, generator and transport tests
  all pass. GUI and synthesis/generator implementation are unchanged.
- Real Chromium/AudioWorklet desktop/mobile controls and factory return pass.
- Exact rc7 package passes 28 update/rollback checks, including rc6 both ways,
  factory-head protection and OTA/loader sanitizers.
- The loader test's fixed interruption after 100 writes missed this small
  downgrade entirely: the complete run does 64 writes (48 body + 16 header).
  The test now cuts halfway through the measured body phase, or at 100 for
  larger changes. Header-erased, retained-record and successful-resume checks
  remain required. Loader/flash/protocol source is unchanged.

Static region use is unchanged from rc6:

| Region | Used | Limit | Free |
| --- | ---: | ---: | ---: |
| Application | 575,000 B | 581,564 B | 6,564 B |
| Data + BSS | 88,760 B | 98,304 B | 9,544 B |
| Data pool | 348,148 B | 356,352 B | 8,204 B |
| Executable RAM + ramx | 23,740 B | 24,576 B | 836 B |

Published: https://timqos.github.io/snow/snow/?v=080rc7

- Firmware source: `3b46f321bbd72c27f4a58e4256901e7732ff6a7e`.
- Audit source: `bc43ae281b9ab43c4f1f0e3d04d22eee302e9d15` (test-only fix).
- File: `stattic-0.8.0-rc7.fwsc`, 610,430 bytes.
- SHA-256: `07780cb6b09f4bb8ddac2531f009566b750765de5e32c13b4756508cb6898f37`.
- gh-pages: `9561541`; successful Pages run: `42363812010`.
- Previous packages, including rc6, are retained. Source PR #3 is unmerged.

All 12 normal site artifact URLs and the root landing match local hashes;
WASM MIME is correct. Verified 2026-11-07 12:26:13 Europe/Kyiv.
