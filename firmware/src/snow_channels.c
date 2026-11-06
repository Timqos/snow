/* SPDX-License-Identifier: GPL-3.0-only
 * GLO/held-SEQ reuse the large six-channel STATTIC views. Different footers
 * describe their existing controls; no duplicate tiny channel/icon renderer. */
static uint8_t snow_channels_active;
static void snow_channels_draw(int patterns)
{
    uint8_t mode=patterns?1u:2u;
    if(!snow_channels_active||stattic_overview!=mode)snow_home_active=0;
    snow_channels_active=1;stattic_overview=mode;snow_home_draw();
}
