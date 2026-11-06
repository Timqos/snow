# STATTIC firmware for M-VAVE FM-1

Six dedicated instruments: **KK / SD / HH / PR / BS / CH**, ten factory
sounds each, three oscillators and six synthesis algorithms per instrument.
Built on Skiff 2.2 and Felucca. Previously named Snow.

[**Simulator and manual**](https://timqos.github.io/snow/snow/)
· [Install STATTIC](https://timqos.github.io/snow/snow/install/)
· [Factory firmware](https://timqos.github.io/snow/snow/install/factory.html)

The 0.8.0-rc4 candidate retains five HOME pages: sound, generation,
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
ranges and polyphony remain. Two cores are enabled by default, with the
existing fault fallback and audio deadline protection. Hardware scheduling
uses measured track costs and a 256-frame buffer. CH uses its existing eight
voice slots for overlapping release tails, within the unchanged shared budget.
The reported physical crackle still requires a device retest.

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
