<!-- SPDX-License-Identifier: GPL-3.0-only -->
# STATTIC 0.8.0-rc7 — installation and rollback audit

The current candidate is **STATTIC 0.8.0-rc7** (formerly Snow). See
[the current revision](STATTIC_RELEASE.md) for interface/sound changes and
physical qualification limits. The current published package's machine-readable
`update-audit.json` supplies its exact hash and update/rollback evidence.
The protocol/flash-map analysis below remains applicable.


**Software checks passed. Physical FM-1 qualification is still pending.**
No Snow-specific boot-area overwrite or firmware lock-in was found in the
reviewed paths. This is evidence about code and simulated updates, not a
guarantee that every physical unit, power interruption or third-party loader
will recover without tools. Snow remains experimental.

The reported 0.7.3 device ran at 99% CPU on one core, with distorted sound,
sluggish UI and short release. This candidate changes runtime scheduling and
idle work; it needs physical retesting. See the [runtime review](SNOW_RUNTIME_REVIEW.md).

The [release evidence](https://timqos.github.io/snow/snow/update-audit.json)
records the exact firmware source, package SHA-256 and audit-source commit for
the current published release. Its version must match the installer/package.
A later build needs a new audit; a passed report from an older binary is not
sufficient.

## What is preserved from Skiff

The comparison base is Skiff source `815358c56ce9307ed1cb4085351cd0fd94c15ee4`
and the published Skiff 2.2 package, SHA-256
`1caf971e6eae8f9bb9b1391c98bde62a2b1c49cda2ece7b0c25808db6b7e489e`.

- All hardware abstraction files, flash maps/drivers, startup assembly, linker
  map, OTA entry/hardware hooks, storage driver, USB, package maker and
  established updater implementations are unchanged from that base. Loader
  source has one reviewed exception: equal-length STATTIC display literals.
- The **package's first 0x4000 bytes of flash.bin remain byte-identical to
  the published Skiff package**. The decompressed OTA loader is byte-identical
  after normalizing STATTIC UPDATER to SKIFF INSTALLER; the audit checks every
  executable byte and both wrapper CRCs. The bundled bootstrap
  head is not written to the device by this loader; the unit keeps its own.
- The running application can write its data/staging windows, not the boot
  head or application code. Whole-sector, cross-boundary and overflowing range
  checks cover the FM-1 map. BTIF at 0xE9000 and the final reserved sector are
  outside the application's write list.
- The custom loader writes application code in **0x4000–0x92FFF**. It checks
  package integrity and the device's chip key before erasing code, invalidates
  the application's header first and restores that header last. Update records
  remain through a failed write and are cleared after completion. Repeating a
  completed installation does not erase unchanged code sectors.
- Flash JEDEC checks, watchdogs, crash/boot-loop recovery, hardware memory guards
  and update entry remain. OCT− + OCT+ held for approximately five seconds in
  the running UI enters the inherited UBOOT recovery path. Two failed warm or
  watchdog restarts also enter that path through the retained boot guard. UBOOT is distinct from
  the ordinary MIDI OTA loader; follow its matching recovery instructions.
- No Snow-specific changes to voltage control, clock switching or GPIO register
  programming were introduced. The same build-time RAM/ROM/HAL guards pass.

Current rc7 region sizes, remaining reserves and exact package verification
are recorded in [the release work log](STATTIC_RC7_WORK.md). The builder
enforces at least 4096 bytes of application-slot reserve and 8192 bytes of
pool reserve. Two of the eight Undo entries move into the data pool, allowing
the oscillator loop to execute in SRAM; Undo depth stays eight. These are
static budgets, not measurements of physical stack or CPU headroom.

The published [build metadata](https://timqos.github.io/snow/snow/build.json)
and [binary audit](https://timqos.github.io/snow/snow/update-audit.json) identify
the current version, source commit and package SHA-256. The rc7 audit must use
the exact rc7 binary and test previous rc6 in both directions. Hardware,
flash-map, loader, protocol and bootstrap-head behavior remain unchanged
from the rc4 audit.

## Tested transitions

| Path | Software evidence | Physical result |
| --- | --- | --- |
| Skiff 2.2 → STATTIC 0.8.0-rc7 | Real packages; OTA staging and C loader tests | Pending |
| STATTIC 0.8.0-rc7 → Skiff 2.2 | Real packages; C loader, preserved boot head, interrupted-write/resume and unchanged-sector checks | Pending |
| STATTIC 0.8.0-rc6 ↔ 0.8.0-rc7 | Real packages in both directions; version compatibility checks | Pending |
| Factory V15/V16 → STATTIC | Real vendor package bootstrap heads used by the C loader tests; head remains intact | Pending |
| STATTIC → factory V15/V16 | Real vendor package validation/staging and matching-head checks passed | Vendor loader execution on a unit is pending |
| Saved STATTIC data → Skiff → STATTIC | Released WASM builds boot, respond to PLAY and render finite audio with persisted flash | Music-format compatibility is not asserted |

The loader tests include corrupt payloads, changed bytes during transfer,
foreign chip keys, writes cut short, the next session completing them, protected
ranges and header ordering. Storage A/B records are tested through torn writes.
OTA tests cover a lost host, missing completion, damaged loaders and wrong-board
sizes. OTA and rollback tests also pass ASan/UBSan.

The official files were obtained from the manufacturer, verified against the
existing updater's pinned hashes, and kept out of the repository/site:

- V15: [M-VAVE fm1_open](https://github.com/m-vave/fm1_open/blob/main/sdk/firmware/FM-1_015.fwsc),
  SHA-256 `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`.
- V16: linked from [M-VAVE Downloads](https://www.m-vave.com/download),
  SHA-256 `d017fb65e70aca97ee29d944470c3515a74581df5c0576bf70de917c7b2b61d5`.

Host tests do not execute the manufacturer's loader binary or emulate the SPL,
USB hardware, flash electronics or power supply. They are not a physical
power-loss certification.

## Returning to another firmware

**Skiff:** save a complete Snow backup first, use a verified FM-1 Skiff package
with its matching installer, then restore the pre-Snow Skiff archive using
Skiff's tools if needed. The STATTIC installer writes STATTIC; its separate factory-return page accepts
only the pinned matching V15/V16 package. Its successful
backup/export tests do not turn it into a general firmware selector.

**Factory:** return to the version matching the device's preserved startup
head, usually the version used before custom firmware. A unit with a V15 head
must use the matching V15 package; one with a V16 head must use V16. The official
loader can write the boot head, so OTA checks all first 0x4000 bytes for equality
before staging it. Actual V15/V16 cross-head mismatches were rejected before
erase/reset in both directions. Keep this protection in place. A factory
version change can be done later through the manufacturer's supported path.

**Other forks:** use their FM-1-specific package, loader and instructions after
checking their compatibility. This audit cannot certify arbitrary future or
third-party packages. It found no new restriction that locks the unit to Snow.

Firmware replacement and music portability are separate. Snow adds engine IDs,
parameters and pattern features. Another firmware may ignore or reject that
music. Preserve an untouched pre-install archive and original firmware, and
restore each archive with the firmware that created it. A user-data backup is
not a replacement for a full flash image or physical recovery hardware.

## Physical release qualification still required

On an FM-1 with a known original version and working recovery route:

1. Save and verify a complete musical backup and the correct original firmware.
   Record the unit's original version and flash variant. A read-back of the boot
   head requires suitable tools; this session did not read any physical unit.
2. Install the hash-identified Snow package using direct USB and stable power.
   Check normal/cold starts, UI, MIDI, USB audio, saves and reloads.
3. Stress all six voices and effect racks while using the UI. Observe audio
   underruns, watchdog resets, USB reliability and memory/stack guards.
4. Perform Snow → Skiff → Snow and the matching factory rollback; verify USB
   enumeration and restore the appropriate pre-install backup.
5. On a recoverable test unit only, validate the documented interrupted-update
   recovery. Do not cut power on an unprepared user's instrument as a test.

Until those results exist, the installer and page retain the experimental and
hardware-unvalidated notices. No device was flashed during this audit.

## Repeat the software audit

After building Snow and the Skiff baseline:

```sh
python3 tools/snow_update_audit.py \
  --snow-package build/felucca.fwsc \
  --baseline-package build/baseline-for-snow.fwsc \
  --metadata build/snow-preview/build.json \
  --snow-wasm build/snow-preview/felucca.wasm \
  --baseline-wasm build/emu/felucca.wasm \
  --fetch-stock --sanitizers
```

The script only reads files and runs simulated NOR/MIDI tests. It never opens
MIDI ports or writes a physical device. `--stock-dir` can supply locally saved
hash-matching vendor files. The cloud build now runs this gate before packaging
its Snow site; failures prevent a successful build/publication. Only logs and
the JSON report are published, never the vendor firmware files.
