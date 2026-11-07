# USB diagnostics for the installed rc9

The physical test after rc9 still reports approximately 250 missed buffers in
3–5 minutes with all channels, particularly CH+KK+BS and sometimes CH+HH+SD.
rc9 did not resolve the fault. Software checks do not establish reliable live
playback. The site and installer now state this confirmed hardware report.

The shipped rc9 already contains the CDC-ACM console (`FELUCCA_CDC=1`).
Its existing status and bounded `memr` commands supply the missing hardware
evidence. No firmware or protocol change is required for the browser logger.

Open the site's `console/` page in desktop Chrome/Edge, enable HOME held →
SYSTEM → USB SERIAL → ON, and wait for USB to re-enumerate if that setting was just changed.
The browser's port picker requires the user's Connect gesture. The page never
requests MIDI/audio access, changes settings, installs firmware or sends UBOOT.

The logger serializes requests at a default two-second interval, pauses on
errors/timeouts, supports local rasp markers and exports JSON/text. Data stays
in the browser. The export contains raw replies, device/PC times, CPU/core/
underrun/voice-shedding status, per-boot L, deltas and exact rc9 stage snapshots.
It also reads current rendered-voice counts and smoothed per-track estimates;
these current values are not claimed to belong to the saved failed half.

Profile decoding is selected by the reported firmware version and pinned to
the official package's ELF map. The device does not attest its package SHA;
that limitation is explicit in the export/UI. Unknown versions get basic status
only, with no guessed RAM reads. Generate/update maps with:

```
python3 tools/snow_console_map.py --elf build/felucca.elf \
  --package build/felucca.fwsc --metadata build/snow-preview/build.json \
  --maps web/snow/console/maps.json
```

The generator rejects changed bytes/regions under an already mapped version.
It checks symbol sizes and RAM bounds. The existing compiler promotes the
boolean flag to a word; the reader accesses its low byte on this little-endian
target. Snapshot reads compare two copies and bracket them with raw L reads;
unstable snapshots are marked unavailable rather than presented as coherent.

The serial status counter retains older boot history. Reading snow_late_base
reconstructs the displayed per-boot L. Unknown versions report L since this
connection; wraps/restarts are handled separately. The bounded export reports
any dropped raw characters, samples or events. Marking a rasp is local and
does not cause an extra USB polling burst. This is state/counter sampling,
not a complete instruction trace or an audio recording. USB telemetry itself
adds work; pause capture to compare.

Firmware remains `676c7090c30831133eddaedf1e8f5e5afa66bf03`, package SHA
`6c65c5167fc5b9a3ed7eda0151e81ff0fe42ea7ef915eb9ca11831ed6dba17a8`.
The existing software update/rollback audit remains applicable to those bytes.
The separate hardware-status.json records the failed audio qualification.

Validation completed:

- Native JS tests cover fragmented replies/prompts, forbidden command refusal,
  backpressure/timeouts and close races, unknown firmware, exact-map decoding,
  unstable snapshots, counter/uptime wraps, restart detection, markers and
  bounded export with explicit drop counts.
- Chromium uses a deterministic CDC model to exercise real page connection,
  growing L, profile decoding, local marks, JSON download, crash read,
  disconnect/reconnect/unplug, unsupported browsers and desktop/mobile layout.
  No permission is requested before Connect and no MIDI/settings/reset writes
  occur. The main site's USB link and fault notice are also checked on mobile.
- Simulator/manual and installer regressions pass. Firmware package, WASM,
  worklet, firmware metadata and installation target hashes are unchanged.
- All seven regions were generated from the exact official rc9 ELF and
  package metadata. The published firmware package remains SHA 6c65c516…17a8.

No physical FM-1 is connected here: the browser test models CDC transport,
not a real USB driver/device. Publication/hash verification is pending.
