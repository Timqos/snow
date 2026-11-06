# STATTIC — agreed interface and sound revision

Source of truth: the user's five 954×954 HOME mockups (1.jpg–5.jpg),
their bottom control captions, and the clarifications in this conversation.
Native FM-1 output is 240×240. Implement the supplied layout on that grid;
do not retain the crowded split-panel HOME or add substitute artwork.

## Execution order

1. Record these agreements and retain the supplied reference images.
2. Implement five HOME pages, native-size large text and the supplied graphics.
3. Wire the exact page-specific physical controls below; exercise real input.
4. Rename the visible firmware, splash, update identity, simulator, installer,
   downloadable package and current manual to STATTIC. Preserve model IDs,
   update protocol, upstream attribution and compatibility with old projects.
5. Reproduce/investigate CH's intermittent ring-like distortion and truncated
   two-bar release through the actual mixer, allocator and long-run playback.
6. Give all ten kick factory presets a clearly audible attack; retain variety
   and the full synthesizer architecture and manual parameter ranges.
7. Strengthen dedicated FX randomisation without unstable feedback/clipping.
8. Broaden role-aware sequence generation, especially kick/bass/chord. Bass
   has melodic articulation; chords have harmonic movement and longer notes.
9. Update the English manual, recovery instructions and release audit. Run
   native, sanitizer, dual-core, WASM, visual, firmware-budget and update checks.
10. Publish the tested candidate and verify GitHub Actions with the connector.
    Physical-device CPU/audio qualification still requires the actual FM-1.

## HOME controls — confirmed by the user

SELECT browses pages 1–5; HOME SELECT never changes tempo. In GLO, the
existing unheld SELECT tempo control remains. HOME hold retains system menu.
ALGORITHM and PRESETS are physical encoders (abbreviated ALG and PRS).

| Page | Physical controls |
| --- | --- |
| 1 Sound | K1 TONE; K2 SHAPE; K3 SWEEP; K4 DECAY; OCT± octave |
| 2 Generate | K1 G/SEQ; K2 G/PRM (sound); K3 G/LFO; K4 G/FX; one detent generates once in either direction. OCT− CLEAR PT; OCT+ CLEAR ALL |
| 3 Channel mixer | K1 VOL; K2 PAN; K3 FLR; K4 WDT; OCT± octave |
| 4 Pattern switch | K1 KK; K2 SD; K3 HH; K4 PR; ALG BS; PRS CH. OCT± RANDOM PATT |
| 5 Mute/solo & volume | First six WHITE keys toggle MUTE; first six BLACK keys toggle SOLO; K1 SLT CH; K2 VOL |

Exactly THREE patterns per channel: P1–P3, all visible, all SIX channels on
page 4. RANDOM PATT selects already saved patterns across all six channels;
it never generates notes. CLEAR PT clears the selected current pattern;
CLEAR ALL clears patterns of all six channels. Avoid deleting saved sound
settings as a side effect. Retain the bank/storage format for old backups.
All six channels share page 5, with separate selection and mute/solo states.

The user supplied a new STATTIC logo: nine stepped horizontal ribbons
forming a circular mark. Use it on startup and web/installation branding.

## Graphics

Use the exact supplied page 1/2/3 static artwork, large labels, positions,
theme colour, white graphic elements and black graphic backgrounds. Replace
the mockup's old SNOW word with STATTIC; preset is the actual sound name.
Retain all bottom control captions and page numbers. The page-5 bars use the
supplied pixel/dither texture and ordinary horizontal level fill.

On pages 1/2/3, each encoder detent visibly advances the static artwork by
several vertical rows, with a seamless cyclic vertical scroll. It is driven
by knob interaction, not a horizontal GIF playback or a single-pixel blink.
Keep the graphics inside their fixed rectangle, with no shadows or drifting
icon transforms. Rendering stays in the main loop, dirty regions, <=20 Hz,
no animation/audio ISR allocation, decoding or additional framebuffer.

## New hardware report

CH distortion occurs during playback on 2 cores, up to 80% displayed CPU;
after STOP the distortion disappears. Investigate peak deadlines, voice
retrigger/tail pressure and per-step parameter locks, not only static patches.

## Sound invariants

Six fixed roles KK/SD/HH/PR/BS/CH; unrestricted manual synthesis surface,
algorithms, waveforms, effects and polyphony remain. CH boots with 2 BAR
release, random sound preserves the selected release, and the actual audible
release must use that timing. Keep audio deadline/voice guards and update
recovery protections; do not call a browser result a hardware measurement.

## Implementation checkpoint

Five HOME pages, physical controls, native art, branding/new logo, kick attack,
stronger dedicated FX and expanded melodic/rhythmic generation are implemented.
Native, WASM, one-minute playback and real Chromium preflight checks pass.
Final clean-source update/rollback audit and publication are the remaining
software steps. Physical CH peak-load/release retesting remains separate.
