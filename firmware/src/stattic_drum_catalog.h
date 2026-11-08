/* SPDX-License-Identifier: GPL-3.0-only */
#define ST_FACTORY_SHOTS 40u
#define ST_USER_SHOT 64u
/* (2026-11-08) the owner's sample set, assets/stattic-drums (tools/gen_stattic_drums.py), ten a category */
static const char *const ST_SHOT_NAMES[ST_FACTORY_SHOTS]={
    "KICK 1",
    "KICK 2",
    "KICK 3",
    "KICK 4",
    "KICK 5",
    "KICK 6",
    "KICK 7",
    "KICK 8",
    "KICK 9",
    "KICK 10",
    "SNARE 1",
    "SNARE 2",
    "SNARE 3",
    "SNARE 4",
    "SNARE 5",
    "SNARE 6",
    "SNARE 7",
    "SNARE 8",
    "SNARE 9",
    "SNARE 10",
    "HAT 1",
    "HAT 2",
    "HAT 3",
    "HAT 4",
    "HAT 5",
    "HAT 6",
    "HAT 7",
    "HAT 8",
    "HAT 9",
    "HAT 10",
    "PERC 1",
    "PERC 2",
    "PERC 3",
    "PERC 4",
    "PERC 5",
    "PERC 6",
    "PERC 7",
    "PERC 8",
    "PERC 9",
    "PERC 10",
};
/* Fixed lane roles; randomization changes the shot, never the instrument role. */
static const uint8_t ST_LANE_CATEGORY[16]={0,0,1,1,2,2,2,3,1,3,3,3,3,3,3,3};
static uint8_t stattic_drum_pick[16]={0,1,10,11,20,21,22,30,12,31,32,33,34,35,36,37};
static uint8_t stattic_user_catalog[4];
