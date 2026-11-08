/* SPDX-License-Identifier: GPL-3.0-only */
#define ST_FACTORY_SHOTS 40u
#define ST_USER_SHOT 64u
static const char *const ST_SHOT_NAMES[ST_FACTORY_SHOTS]={
    "ROUND KICK",
    "HARD KNOCK",
    "FM THUD",
    "909 DRIVE",
    "808 SUB",
    "DUB PILLOW",
    "BREAK PUNCH",
    "IDM CLICK",
    "SOFT LOW",
    "SHORT TECHNO",
    "SNAPPY",
    "PICCOLO",
    "DEEP WOOD",
    "DRY 808",
    "TIGHT 909",
    "BREAK SNARE",
    "DNB CRACK",
    "NOISE SNAP",
    "BRUSH",
    "INDUSTRIAL",
    "CLOSED AIR",
    "OPEN BRASS",
    "PEDAL CHICK",
    "808 CLOSED",
    "909 OPEN",
    "DUST HAT",
    "DNB TICK",
    "SHUFFLE TOP",
    "DARK OPEN",
    "SIZZLE HAT",
    "FM BELL",
    "WOOD HIT",
    "ALIEN PERC",
    "RIM",
    "LOW TOM",
    "COWBELL",
    "CLAVE",
    "SHAKER",
    "CONGA",
    "CLICK PERC",
};
/* Fixed lane roles; randomization changes the shot, never the instrument role. */
static const uint8_t ST_LANE_CATEGORY[16]={0,0,1,1,2,2,2,3,1,3,3,3,3,3,3,3};
static uint8_t stattic_drum_pick[16]={0,1,10,11,20,21,22,30,12,31,32,33,34,35,36,37};
static uint8_t stattic_user_catalog[4];
