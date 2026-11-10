/* SPDX-License-Identifier: GPL-3.0-only */
#define ST_PATTERNS 3u
#define ST_PARAM_WORDS ((P_COUNT+31u)/32u)
enum { ST_CHANGE_PATTERN=1,ST_CHANGE_SOUND=2,ST_CHANGE_SHOTS=4,ST_CHANGE_FADE=8 };
/* (2026-11-08, the owner) a new sound from HOME 2 K2 enters softly at its bar: the channel's synth goes down in
 * ~3 ms, then back up over ~150 ms (stattic_mix.c stattic_fade_run). 0 = no fade, 1 = going down, 2 = coming up */
static volatile uint8_t stattic_fade_stage[4];
static int32_t stattic_fade_gain[4]={32767,32767,32767,32767};
typedef struct {
    union { step_t step[NSTEP];dstep_t drum[NSTEP]; };
    int8_t micro[NSTEP];
    uint8_t fill[NSTEP/4];
    plock_t lock[NLOCK];
} stattic_pattern_t;
typedef struct {
    int16_t p[P_COUNT];
    stattic_pattern_t pattern;
    uint8_t shots[16];
    int16_t offset[4];
    uint8_t slot;
} stattic_snapshot_t;
typedef struct {
    stattic_snapshot_t value;
    uint32_t mask[ST_PARAM_WORDS],bar,serial,at16;   /* at16: a sound's change on the START grid (16ths), when fine */
    uint8_t fine,glide;                              /* glide: a HOME 2 K2-K4 change (MORPH); undo, loads... at once */
    uint8_t what,slot;
    volatile uint8_t armed;
} stattic_change_t;
/* Explicitly initialized before audio starts; native SLOOP's noinit project
 * mirrors moved to the pool, leaving this area for banks and queued copies. */
static stattic_pattern_t stattic_banks[NTRK][ST_PATTERNS] __attribute__((section(".noinit")));
static stattic_change_t stattic_change[NTRK] __attribute__((section(".noinit")));
static uint8_t stattic_current[NTRK],stattic_home_page,stattic_focus[3],stattic_k2_lofi[3];   /* (k2_lofi: 1 tape, 2 crush, brought by K2) */
static uint32_t stattic_origin[NTRK],stattic_scroll[3],stattic_revision;
static uint8_t stattic_scroll_push[3];
/* (2026-11-09, the owner) HOME 6 PROJECTS: the slot chosen, an action waiting for its second turn, a note */
static uint8_t stattic_proj_sel,stattic_proj_arm;static uint32_t stattic_proj_arm_ms,stattic_proj_msg_ms;static char stattic_proj_msg[24];   /* (rows still to run quickly after a knob turn) */
#include "stattic_art.h"
static uint8_t stattic_art_pick[3]={0,1,2};
/* (2026-11-08) ST_ART_COUNT pictures in a shuffled bag; each page shows 84 / 78 / 78 rows of one */
static struct { uint32_t random;uint8_t bag[ST_ART_COUNT],left,last,page,knob; } stattic_art_shuffle;
static const uint8_t stattic_art_rows[3]={84,78,78};
static volatile uint8_t stattic_home_ms,stattic_home_ms_reset,stattic_ms_pending;
static uint8_t stattic_ms_mute,stattic_ms_solo;
static uint32_t stattic_ms_bar;
static uint8_t stattic_ready,stattic_ui_dirty;
/* (2026-11-10, the owner) a K2-K4 change gliding in (MORPH) on BS, SY or CH: a note that would end before the next
 * one goes on until the glide is over (or the next note comes), so the glide is heard on a sounding note */
static volatile uint8_t stattic_morph_live[3],stattic_morph_held[3];
/* (2026-11-10, the owner) BS, SY, CH: the amp attack never under 40 ms (TIME_MS_X10[51] = 40.4 ms) from a preset or K2 */
#define ST_ATK_MIN 51
static void stattic_key_set(int32_t root,int32_t scale);static void stattic_key_unify(void);   /* (the song's key: stattic_runtime.c) */
static volatile uint8_t stattic_staging;
static uint8_t stattic_macro[NTRK];
static uint32_t stattic_role_track(uint32_t home) { static const uint8_t map[4]={3,0,1,2};return map[home%4u]; }
static uint32_t stattic_home_role(uint32_t track) { return (track+1u)%4u; }
static int16_t stattic_base_value(const track_t *t,uint32_t id)
{
    for(uint32_t i=0;i<t->lk_n;i++)if(t->lk_param[i]==id&&t->p[id]==t->lk_set[i])return t->lk_base[i];
    return t->p[id];
}
static void stattic_runtime_init(void);
static void stattic_runtime_tick(void);
static void stattic_runtime_poll(void);
static void stattic_runtime_start(void);
static void stattic_undo_mark(const track_t *t,uint32_t session);
static int stattic_undo(int redo);
static void stattic_ms_key(uint32_t key,int down);
static void stattic_mute_set(uint32_t track,uint32_t value);
static void stattic_solo_set(uint32_t bits);
static int stattic_prepare(uint32_t track,uint32_t knob);
static void stattic_prepare_scope(uint32_t track,uint32_t knob);
static void stattic_pattern_random(void);
static int stattic_pattern_select(uint32_t track,uint32_t slot);
static void stattic_clear(int all);
static void stattic_temp_save(void);
static int stattic_temp_load(uint32_t mask);
static void stattic_control_all(uint32_t knob,int32_t delta);
static void stattic_touch(track_t *t,uint32_t id);
#ifndef STATTIC_GUARD_SAVE
#define STATTIC_GUARD_SAVE() 0u
#define STATTIC_GUARD_RESTORE(f) ((void)(f))
#endif

static uint32_t fol_on(track_t *t,uint32_t note);
static uint32_t fol_off(track_t *t,uint32_t note);
static void fol_drop(const track_t *t);
static void stattic_duck_note(const track_t *t,uint32_t note);
static void stattic_midi_drop(const track_t *t);
