/* SPDX-License-Identifier: GPL-3.0-only */
/* (2026-11-09) STATTIC DR loudness: every micro drum archetype (stattic_micro.h), or with OLD=1 on a build
 * before them every factory shot, one hit at velocity 110 through the DR mix. Prints per sound the peak and
 * the loudest 10 ms RMS (tools/gen_stattic_micro.py takes the levels from it).
 *   cc -O2 -w -fwrapv -Ibuild/gen -Ifirmware/src -Ifirmware/hal tests/stattic_micro_level.c -lm -lpthread */
#define STATTIC_RUNTIME_HARNESS_ONLY
#include "stattic_runtime_test.c"
#include <math.h>
static void measure(const char *name)
{
    static float buf[FS];
    uint32_t n = 0, b, i;
    for (b = 0; b < FS / CTL; b++) {
        int32_t l[CTL] = {0}, r[CTL] = {0}, rv[CTL] = {0};
        drums_render(l, r, rv, rv, CTL);
        for (i = 0; i < CTL; i++)
            buf[n++] = (float)((l[i] + r[i]) / 2) / 32768.f;
    }
    float pk = 0, best = 0;
    uint32_t w = FS / 100u;
    double acc = 0;
    for (i = 0; i < n; i++) {
        float a = fabsf(buf[i]);
        if (a > pk) pk = a;
        acc += (double)buf[i] * buf[i];
        if (i >= w) acc -= (double)buf[i - w] * buf[i - w];
        if (i + 1 >= w && acc > 0) {
            float rms = (float)sqrt(acc / w);
            if (rms > best) best = rms;
        }
    }
    printf("%-12s peak %6.1f dB  rms10 %6.1f dB\n", name, 20 * log10f(pk + 1e-9f), 20 * log10f(best + 1e-9f));
}
int main(void)
{
    init();
    TDRUM->p[P_LEVEL] = 100;
    TDRUM->p[P_E1] = 127; TDRUM->p[P_E2] = 0; TDRUM->p[P_E3] = 64; TDRUM->p[P_E4] = 64;
    memset(snow_offset, 0, sizeof snow_offset);
#ifdef ST_MICRO_N
    if (!getenv("OLD")) {
        static const char *cat[4] = {"KICK", "SNARE", "HAT", "PERC"};
        for (uint32_t c = 0; c < 4u; c++)
            for (uint32_t a = ST_MICRO_START[c]; a < ST_MICRO_START[c + 1u]; a++) {
                char nm[16];
                memset(&drums, 0, sizeof drums);
                drums.v[0].active = 1; drums.synth[0] = 1; drums.v[0].note = 36; drums.kit[0] = (uint8_t)DRUM_STATTIC;
                ds_on_one(&drums.ds[0], &ST_MICRO[a], 0, 0, 36, 110, 0, 4);
                snprintf(nm, sizeof nm, "%s/%u", cat[c], a - ST_MICRO_START[c]);
                measure(nm);
            }
        return 0;
    }
#endif
    static const uint8_t first[4] = {0, 2, 4, 7};
    for (uint32_t shot = 0; shot < 40u; shot++) {
        char nm[16];
        uint32_t lane = first[shot / 10u];
        memset(&drums, 0, sizeof drums);
        stattic_drum_pick[lane] = (uint8_t)shot;
        drum_on(LANE_NOTE[lane], 110);
        snprintf(nm, sizeof nm, "OLD %u", shot);
        measure(nm);
    }
    return 0;
}
