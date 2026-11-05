# Snow channel animation sources

The five GIFs were supplied by the project owner with channel names: KK, SD,
HH, PR and BS. Original files are kept intact for reference and reproducibility.
CH/cassette and the right-hand generator motifs are outside this replacement.

Run `python3 tools/gen_snow_channel_icons.py` (Pillow) to reproduce the checked
header, manifest and test fixture. A single union crop is measured across all
source frames of each icon. Thirty-three ordered source poses are adapted to a
33×33 three-role grid: background, light contour, theme accent. A one-pixel
empty perimeter is enforced in every pose. HOME renders at integer 2× inside
the former KK bounds, (13,115) to (79,181), exclusive right/bottom. The source
GIFs contain antialiasing; the adapted grids are deliberately pixel art.

The generated header contains only zero-run/literal XOR deltas. The device
replays them into 273 stack bytes in the UI main loop. No original GIF, image
codec, timer-driven playback or theme-specific duplicate frames is flashed.
The checked `reference.frames` file is a host-test fixture, not firmware data.
`manifest.json` records source hashes, chosen frames, fit geometry and bounds.

The earlier sprite reference is retained separately in
`assets/snow-icons.json`; only CH and the four approved generator motifs from
that file are now packed by `tools/gen_snow_icons.py`.
