/* SPDX-License-Identifier: GPL-3.0-only
 * Snow: RAM-only musical snapshot and additive Control All. Included by ui_input.c.
 * Does not copy voices, audio buffers, transport phase or flash storage. */
static struct {
    int16_t p[NTRK][P_COUNT], g[G_COUNT], offset[NTRK][4];
    step_t step[NTRK][NSTEP];
    motion_store_t locks;
    uint8_t valid, mask;
} snow_saved __attribute__((section(".pool")));
_Static_assert(sizeof snow_saved <= 8192u, "Snow snapshot must fit its RAM budget");

static void snow_temp_save(void)
{
    uint32_t k, j, f = motion_guard();
    for (k = 0; k < NTRK; k++) {
        for (j = 0; j < P_COUNT; j++) snow_saved.p[k][j] = motion_base_value(&trk[k], j);
        memcpy(snow_saved.step[k], trk[k].step, sizeof snow_saved.step[k]);
    }
    memcpy(snow_saved.g, song.g, sizeof snow_saved.g);
    memcpy(snow_saved.offset, snow_offset, sizeof snow_offset);
    snow_saved.locks = motion;
    snow_saved.mask = snow_mask;
    snow_saved.valid = 1;
    motion_unguard(f);
}

static int snow_temp_reload(uint32_t mask)
{
    uint32_t k, n = 0, f;
    motion_store_t restored = motion;
    if (!snow_saved.valid) return 0;
    /* Check the shared event pool before changing any musical state. */
    for (k = 0; k < motion.count; k++)
        if (!(mask & (1u << (motion.event[k].place >> 6)))) restored.event[n++] = motion.event[k];
    for (k = 0; k < snow_saved.locks.count; k++)
        if (mask & (1u << (snow_saved.locks.event[k].place >> 6))) {
            if (n == MOTION_MAX) return 0;
            restored.event[n++] = snow_saved.locks.event[k];
        }
    restored.count = (uint8_t)n;
    restored.on = (uint8_t)((motion.on & ~mask) | (snow_saved.locks.on & mask));
    f = motion_guard();
    for (k = 0; k < NTRK; k++) if ((mask >> k) & 1u) {
        memset(motion_active[k], 0, sizeof motion_active[k]);
        memset(motion_locked[k], 0, sizeof motion_locked[k]);
        memcpy(trk[k].p, snow_saved.p[k], sizeof trk[k].p);
        memcpy(trk[k].step, snow_saved.step[k], sizeof trk[k].step);
        memcpy(motion_base[k], trk[k].p, sizeof trk[k].p);
        memcpy(snow_offset[k], snow_saved.offset[k], sizeof snow_offset[k]);
    }
    motion_base_valid = (uint8_t)((motion_base_valid & ~mask) | (song.playing ? mask : 0));
    motion = restored;
    if (mask == (1u << NTRK) - 1u) {
        memcpy(song.g, snow_saved.g, sizeof snow_saved.g);
        snow_mask = snow_saved.mask;
    }
    motion_unguard(f);
    ui.force = 1;
    return 1;
}

static void snow_control_all(uint32_t knob, int32_t delta)
{
    uint32_t k, f = motion_guard();
    if (knob < 4u) for (k = 0; k < NTRK; k++) if ((snow_mask >> k) & 1u)
        snow_offset[k][knob] = (int16_t)clamp(snow_offset[k][knob] + delta, -127, 127);
    motion_unguard(f);
    ui.force = 1;
}

static int snow_buttons(uint32_t pressed)
{
    if (ui.menu || ui.confirm || name_on() || !((fm1_in.buttons >> panel.btn[B_GLO]) & 1u)) return 0;
    if (!(pressed & ((1u << panel.btn[B_SAVE]) | (1u << panel.btn[B_HOME]) |
                     (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP])))) return 0;
    layer_arm(1u << panel.btn[B_GLO], fm1_ticks());
    (void)layer_gesture(fm1_ticks(), 1); /* consume the chord: releasing GLO must not open its page */
    if ((pressed >> panel.btn[B_SAVE]) & 1u) {
        snow_temp_save(); ui_message("TEMP SAVED"); return 1;
    }
    if ((pressed >> panel.btn[B_HOME]) & 1u) {
        ui_message(snow_temp_reload((1u << NTRK) - 1u) ? "TEMP RELOADED" : "NO TEMP SAVE"); return 1;
    }
    if ((pressed >> panel.btn[B_OCTDN]) & 1u) {
        snow_mask ^= (uint8_t)(1u << song.sel);
        ui_message((snow_mask >> song.sel) & 1u ? "CONTROL ALL: IN" : "CONTROL ALL: OUT"); return 1;
    }
    if ((pressed >> panel.btn[B_OCTUP]) & 1u) {
        ui_message(snow_temp_reload(1u << song.sel) ? "TRACK RELOADED" : "NO TEMP SAVE"); return 1;
    }
    return 0;
}
