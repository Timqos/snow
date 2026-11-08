/* SPDX-License-Identifier: GPL-3.0-only */
enum { DT_SOFT, DT_HARD, DT_FOLD, DT_CRUSH, DT_FUZZ, DT_N };
#define DIST_TONE(k, t) clamp((k) + (t)->p[P_DTONE] * 150, 2000, 32700)   /* TONE's move of the low-pass (Q15) */
/* HARD, FOLD, FUZZ: the drive into INSERT's shaper ty (a constant: a loop each, no switch a sample) */
static inline __attribute__((always_inline)) void dist_clip(track_t *t, int32_t *b, uint32_t n, int32_t g, int32_t k,
                                                             int32_t mk, uint32_t ty)
{
    for (uint32_t i = 0; i < n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;
        x = clamp(x - t->dist_hp, -230000, 230000);
        y = ins_shape(ty, ((x >> 2) * g) >> 10, 0);
        t->dist_dc += (y - t->dist_dc + 256) >> 9;      /* (~14 Hz: a clip of an uneven wave has a mean, kept out of */
        y -= t->dist_dc;                                /* the buses' feedback) */
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}
static ST_HOT void dist_hard(track_t *t, int32_t *b, uint32_t n, int32_t g, int32_t k, int32_t mk)
{
    dist_clip(t, b, n, g, k, mk, IT_HARD);
}
static ST_HOT void dist_fold(track_t *t, int32_t *b, uint32_t n, int32_t g, int32_t k, int32_t mk)
{
    dist_clip(t, b, n, g, k, mk, IT_FOLD);
}
static ST_HOT void dist_fuzz(track_t *t, int32_t *b, uint32_t n, int32_t g, int32_t k, int32_t mk)
{
    dist_clip(t, b, n, g, k, mk, IT_FUZZ);
}
static ST_HOT void dist_crush(track_t *t, int32_t *b, uint32_t n, int32_t d, int32_t k, int32_t mk)
{
    uint32_t s = (uint32_t)d / 11u, hm = (1u << ((uint32_t)d >> 5)) - 1u;   /* bits taken 0..11, held 1 2 4 8 */
    int32_t h = 0;
    for (uint32_t i = 0; i < n; i++) {
        int32_t x = b[i];
        t->dist_hp += (x - t->dist_hp + 64) >> 7;
        x = clamp(x - t->dist_hp, -230000, 230000);
        if (!(i & hm))
            h = ins_shape(IT_CRUSH, x, s);              /* (rounded: no offset) */
        t->dist_lp1 += mulq15(h - t->dist_lp1, k);
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}

/* DIST: low cut -> drive (1x..8x, exponential) -> asymmetric soft clip
 * (a little bias = even harmonics) -> tone low-pass that closes with drive ->
 * make-up gain (straight into tanh over the full band, it would sound like a
 * broken digital fuzz). State per part (track_t dist_*). (Skiff phase 9) DRIVE's TYPE and TONE: above */
static ST_HOT void stattic_dist(track_t *t, int32_t *b, uint32_t n)
{
    int32_t d = t->p[P_DIST], i, g, k, mk, bias = 2400, b0;
    if (!d)
        return;                                         /* states kept: switching on does not click */
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = DIST_TONE(32000 - d * 95, t);                    /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = 30000 - d * 120;                                /* make-up */
    switch (t->p[P_DTYPE]) {
    case DT_HARD: dist_hard(t, b, n, g, k, mk); return;
    case DT_FOLD: dist_fold(t, b, n, g, k, mk); return;
    case DT_CRUSH: dist_crush(t, b, n, d, DIST_TONE(32700 - d * 40, t), 32000 - d * 20); return;
    case DT_FUZZ: dist_fuzz(t, b, n, g, k, mk); return;
    default: break;
    }
    b0 = softclip(bias);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y;
        t->dist_hp += (x - t->dist_hp + 64) >> 7;           /* ~55 Hz low cut: keep the bass out of the clipper */
        x = clamp(x - t->dist_hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
        y = ins_shape(IT_SOFT, (((x >> 2) * g) >> 10) + bias, 0) - b0;   /* >> 2 first: no overflow for loud poly */
        t->dist_lp1 += mulq15(y - t->dist_lp1, k);         /* two poles: tames the fizz */
        t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, k);
        b[i] = mulq15(t->dist_lp2, mk);
    }
}
