# STATTIC cloud build

[Current simulator and manual](https://timqos.github.io/snow/snow/)
· [Installer](https://timqos.github.io/snow/snow/install/)
· [Factory return](https://timqos.github.io/snow/snow/install/factory.html)

The **STATTIC firmware and simulator** workflow builds the upstream Skiff
baseline and its test suite, then the STATTIC FM-1 fork, native/dual-core
checks, actual audio auditions, WebAssembly/AudioWorklet, website and
backup-first installer tests. It also runs the update/rollback audit with
hash-pinned original V15/V16 packages. Manufacturer files remain build inputs;
they are not redistributed with the site.

The fork retains `FELUCCA_SNOW=1` as its build flag, historical API/storage
identifiers and the `/snow/` URL so existing projects, archives and links
remain compatible. Visible firmware identity and current site branding are
STATTIC. The upstream baseline is explicitly labelled Skiff on `/try/`.

Use Python with Pillow/fontTools, Node, the pinned JieLi compiler, checked
AC79 SDK and Emscripten 3.1.64. `bash tools/cloud_build.sh` runs the same
pipeline as Actions. `tools/cloud_toolchain.sh` and `tools/cloud_sdk.py`
prepare the pinned dependencies; BUILDING.md describes custom paths.

Pages is configured to deploy through GitHub Actions. Builds from the working
branch/PR validate and upload artifacts; they do not replace the live site.
The repository’s existing `publish-simulator.yml` on main publishes the checked
`gh-pages` contents. Keep the previous published candidate in an archive
before replacing `/snow/`; update package/hash/build metadata together.

No automated build flashes a physical FM-1. The software audit does not
establish hardware CPU headroom or eliminate an intermittent audio fault.
Read [the current release review](STATTIC_RELEASE.md) and the
[installation/rollback audit](SNOW_HARDWARE_AUDIT.md) before installing.
