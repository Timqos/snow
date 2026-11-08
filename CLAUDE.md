# Working on STATTIC (instructions from the owner)

- **Every new firmware build goes onto the site's install page.** The owner installs edited
  firmware only through the installer page (`snow/install/` on gh-pages). After a firmware change,
  push the branch: `.github/workflows/stattic-firmware.yml` builds the `.fwsc`, puts it in
  `snow/firmware/`, points `snow/install/config.json` (version, SHA-256, package) and
  `snow/firmware/manifest.json` at it, pushes gh-pages and runs `publish-simulator.yml` to deploy.
  Check that the run succeeded and tell the owner the version label shown on the install page
  (`STATTIC <version> · <commit>`). The firmware cannot be built in this sandbox (no JieLi
  toolchain access); the workflow is the build.
- The owner writes in Ukrainian; answer in Ukrainian.
- Do not break existing behaviour: randomisation, bar-quantised pattern/mute/solo changes,
  parameters. Check DSP changes against `tests/stattic_check.py` (engine and FX vectors, mix in
  all core modes); `tests/stattic_prof.c` is a host CPU proxy (callgrind), not FM-1 time.
- Audio faults: read the USB console log (`audio_late` delta, per-channel times, `racks_awake`).
