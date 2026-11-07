# Archived GIF references and retained BS chart

The five original GIFs were supplied by the project owner and are retained intact
as historical references. Since Snow 0.7.3 only BS is compiled from these files.
KK, SD, HH and PR use direct filled geometry in firmware/src/snow_pixels.c.
CH/cassette and the right-hand generator motifs remain unchanged.

Run `python3 tools/gen_snow_channel_icons.py` to reproduce the retained BS
header, manifest and host-test fixture. Its crop, 33 poses, palette roles and
273-byte scratch decoder are unchanged from 0.7.2. The original four GIF motifs
are omitted from the firmware atlas; the source GIFs are not firmware data.

The new four icons draw at the actual 66×66 HOME cell resolution, with identical
visible bounds x=14..77, y=116..179 at every parameter position. The corner or
endpoint anchors keep the overall size fixed while parts change inside it.
Everything is filled with the selected system ink; there are no raster frames,
shaded sweeps, antialiasing or animation clocks for these four designs.
