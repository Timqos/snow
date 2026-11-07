# STATTIC firmware for M-VAVE FM-1

**Known hardware audio fault in rc9:** physical testing reports about 250
missed buffers in 3–5 minutes of mixed playback involving CH. It is not
verified for reliable live use. [USB diagnostic console](https://timqos.github.io/snow/snow/console/)
collects logs from the existing rc9 without reflashing; see [the guide](docs/STATTIC_USB_CONSOLE.md).

Six dedicated instruments: **KK / SD / HH / PR / BS / CH**, ten factory
sounds each, three oscillators and six synthesis algorithms per instrument.
Built on Skiff 2.2 and Felucca. Previously named Snow.

[**Simulator and manual**](https://timqos.github.io/snow/snow/)
· [Install STATTIC](https://timqos.github.io/snow/snow/install/)
· [Factory firmware](https://timqos.github.io/snow/snow/install/factory.html)

The 0.8.0-rc9 candidate retains five HOME pages: sound, generation,
channel mixing, six-channel P1–P3 patterns, and MUTE/SOLO with volume.
SELECT browses pages; the lower captions identify the physical controls.
All six channels start at 64 steps. Four-bar generation varies bars 2 and 4;
bar 1 may repeat in bar 3. During PLAY, patterns and HOME mute/solo wait for
the next shared transport bar; HOME 2 generators now enter on that same bar.
Stopped changes apply immediately. Larger bottom helpers replace the empty
HOME visualizer area; HOME 3 shows the selected parameter’s percentage.
Large native-pixel text, an enlarged transport strip and the supplied
vertically scrolling static artwork replace the crowded HOME layout.

On HOME 2/5, K1–K4 generate phrases/locks, sounds, LFOs and effects.
On HOME 4/5, K1–K4 plus ALGORITHM and PRESETS select six independent patterns.
Bass and chord phrases use held/tied notes, harmonic movement and
instrument-specific articulation. Manual synthesis, algorithms, parameter
ranges remain. Two cores are enabled by default, with the
existing fault fallback and audio deadline protection. Hardware scheduling
uses measured track costs and a 256-frame buffer. CH has a strict four-voice limit including
release tails. Triads and four-note chords remain available; new notes replace
old tails when all four slots are occupied. Other channel caps and the shared
8/12-voice budget remain unchanged.
The oscillator loop executes in fast RAM and the split anticipates new voices
before their first rendered block. Common sine/triangle waveforms now avoid
repeated helper dispatch, and synchronized LFO increments are reused until
their period changes. Low-note attack captures remain sample-identical to
rc5. rc8 fixes repeated false onset costs from silent held voices, removes
a discarded FM CHAIN waveform evaluation and puts the synth envelope in RAM.
Rising L during the reported rasp confirms missed audio deadlines; physical
retesting is still required, across channels and sound settings. rc9 also
moves the exact sine/filter/saturation lookup tables into data SRAM and adds
read-only SYSTEM peak/last-missed-half timing to separate CH synthesis,
effects/mixing, worker wait and nested interrupt time on the actual device.

HOME bottom helpers share one enlarged 8×14 pixel face, retaining their full
wording and the exact upper layout. Other editors and system menus use larger
pixel labels and values; settings show three tall rows with the same controls.
All eight Undo entries, synthesis, effects, gains and generators remain.

This is experimental custom firmware, supplied without warranty to the
extent permitted by law. Installation and use are at your own risk and
responsibility. Back up music/settings and retain the matching original
V15/V16 firmware before installation. Software checks do not establish
physical FM-1 CPU headroom or audio quality.

- [Agreed design and execution plan](docs/STATTIC_WORK_PLAN.md)
- [Current revision and validation](docs/STATTIC_RELEASE.md)
- [Installation and rollback audit](docs/SNOW_HARDWARE_AUDIT.md)
- [Build instructions](BUILDING.md) · [Cloud build](docs/CLOUD_BUILD.md)
- [Upstream Skiff documentation](docs/UPSTREAM_README.md)
- [Licences and credits](LICENSING.md) · [GPL-3.0](LICENSE)

Build the FM-1 fork with `FELUCCA_SNOW=1 sh build.sh`, then
`python3 tools/snow_preview.py` with Emscripten available. Historical flags,
model IDs, backup formats and repository URLs remain stable for compatibility;
visible firmware identity is STATTIC. Non-fork builds retain upstream Skiff.
