# STATTIC USB additions (0.9.0-S1)

The native Felucca/SLOOP MIDI SysEx framing is retained. Requests are
`F0 7D 46 4C cmd args… F7`; replies echo `F0 7D 46 4C cmd` before response data.
All payload bytes are seven-bit. Internal track order is BS=0, SY=1, CH=2, DR=3;
HOME display order is DR, BS, SY, CH. Editor protocol version is 13.

Native legacy INFO parameter counts are seven-bit fields; do not use them to
enumerate STATTIC's extended parameters. New clients must positively identify
STATTIC using command 78 before reading or writing its distinct storage format.
The native generic web editor is not offered for this extended personality.

## Command 78: capabilities

No arguments. Response data, in order:

- schema=1, tracks=4, patterns=3, steps=64;
- user slots=4, slot KiB=60, zones per slot=16, category marker=0x53;
- parameter count low7/high7, engine parameter base low7/high7;
- playing flag, flash-available flag;
- version as a null-terminated string, at most 24 characters (`STATTIC 0.9.0-S1`).

## Command 79: full-width parameters

Arguments: track (4=globals), parameter ID low7/high7, set flag, optional value.
Values use native signed v14: encode `value+8192` as low7/high7. Response:
track, ID low7/high7, current value v14, minimum v14, maximum v14, null-terminated
label of at most 16 characters. Values clamp to their descriptor. Mute writes queue for the next
bar during playback. This command addresses ordinary parameters; engine-role
changes and other structural operations are not supported through it.

## Native sample and backup transfers

Retain SLOOP's existing commands and seven-bit binary packing. In particular,
SMP_WRITE offsets are **three seven-bit bytes**, whereas backup object offsets
are five. Sample slots are 60 KiB here; never reuse an 80 KiB SLOOP address map.
Sample BEGIN/WRITE/END/ERASE return busy during PLAY. The manager reads the bank,
preserves other zones, exports the prior bank, uploads, then checks exact readback.

Bank headers retain the native smp_slot structure, with rsv=0x53. Per-zone lo/hi
are equal category anchors: KK=36, SD=38, HH=42, PR=60. User catalog IDs are
64 + slot*16 + zone. A two-sample silent zone is a tombstone, not a random choice.
Factory IDs 0–39 are ten shots per category, stored separately in the application.

Full backup format `STATTIC-SLOOP-BACKUP`, schema 1, stores object IDs, lengths,
base64 data and CRC32. Restore validates the entire file before starting, requires
the same firmware version, and restores the working project last. Native SLOOP
and Skiff project/preset bytes are not interchangeable with STATTIC bytes.

CDC diagnostics use `diag_schema=3`, with explicit track/FX/event/master-bus timing
fields and a snapshot of the latest missed audio half. Enable MENU → SYSTEM →
USB SERIAL, then replug/power-cycle to enumerate CDC. L counts missed DMA halves
since cold boot; a warm reset can preserve it. Use its **delta**, not only its
absolute value. The console polls counters and cannot prove hardware stability
from an empty or short log.
