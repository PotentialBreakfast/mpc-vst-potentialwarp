/* PotentialWarp (PotentialBreaks): a tempo-locked time and volume mangler for MPC OS standalone devices, in the spirit
 * of Gross Beat. It keeps the last few seconds of its input and plays them back through a 16-step PATTERN locked to the
 * song position: each step plays straight through, slows down (half / quarter speed), runs down like a stopped tape or
 * spins back up, repeats a slice (a stutter or roll), plays backwards, spins back, or goes silent.
 *   PATTERN  one of 16 (the pads on the page, and MPC's PRESET menu)
 *   LENGTH   1/2 BAR, 1 BAR or 2 BARS: how long the 16 steps last
 *   WARP     ON / OFF: OFF passes the sound straight through (automate it to drop the effect in and out)
 *   FADE     TIGHT / SOFT: how quickly it crossfades at each jump (short = more click and bite)
 *   MIX, OUTPUT
 * EDIT page: tap a step (EDIT STEP), then what it does (MOVE). Every pad's pattern can be edited, is saved with the project,
 * and RESET puts the pad's own pattern back. DICE rolls a new pattern for the pad; WILD says how far it goes.
 * CHAIN page: up to 8 slots, each a pad's pattern, played in turn (one LENGTH each) while CHAIN is ON; the pads follow.
 * 44.1 kHz, interleaved int16 stereo, 128-frame blocks (wrapper/engine.h); the song position comes from song_pos(). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "../shared/looks/potential_theme.h"   /* the LOOK switch (parameter "theme") */

#define FS 44100.0
#define BUF_BITS 19                      /* 524288 frames (11.9 s) of history per channel */
#define BUF (1 << BUF_BITS)
#define MASK (BUF - 1)
#define MAX_DELAY ((double)BUF - 4096.0) /* never read closer to the write head's far side than this */
#define NSTEP 16
#define NCHAIN 8
#define LAT 2.0                          /* frames: the read heads stay this far behind so the cubic never reads ahead */

/* ---- parameters: same order and keys as vst/params.json (step and theme sit between OUTPUT and EDIT STEP) ---- */
enum { P_RESERVED, P_PATTERN, P_LENGTH, P_WARP, P_FADE, P_MIX, P_OUT, P_EDIT, P_MOVE, P_DICE, P_WILD, P_RESET, P_CHAIN,
       P_CPOS, P_CPICK, NP };
static const char *KEYS[NP] = {"reserved", "pattern", "length", "warp", "fade", "mix", "output", "edit_step", "move", "dice",
                               "wild", "reset", "chain", "chain_pos", "chain_pick"};
/* saved in the state: the settings, not the triggers or what the EDIT / CHAIN pages show */
static const int SAVED[] = {P_PATTERN, P_LENGTH, P_WARP, P_FADE, P_MIX, P_OUT, P_EDIT, P_WILD, P_CHAIN, P_CPOS};

/* the MOVE options, in params.json's order, as pattern characters */
#define NMOVE 12
static const char MOVES[NMOVE + 1] = ".-hqturbXxyz";

/* ---- the 16 patterns: one character per step ----
 *   .  straight through          h  half speed           q  quarter speed
 *   t  tape stop (slows to 0)    u  tape start (spins up from 0)
 *   z  repeat 1/4 step   y  repeat 1/2 step   x  repeat 1 step   X  repeat 2 steps   (from the start of the run)
 *   r  backwards                 b  spin back (fast backwards, slowing down)
 *   -  silence
 * A run of the same character is one move: "tttt" is one tape stop four steps long. */
#define NPAT 16
static const char *PAT[NPAT] = {
    "hhhhhhhhhhhhhhhh",   /* Half Time */
    "........tttttttt",   /* Tape Stop */
    "ttttttttuuuuuuuu",   /* Stop & Start */
    "............xxxx",   /* Stutter */
    "....xxxxyyyyzzzz",   /* Roll Up */
    "XXXXXXXX........",   /* Beat Repeat */
    "....rrrr....rrrr",   /* Reverse */
    "............bbbb",   /* Spinback */
    ".-.-.-.-.-.-.-.-",   /* Gate 16ths */
    "..--..--..--..--",   /* Gate 8ths */
    "..-..-.-..-..-.-",   /* Chop */
    "........hhhhhhhh",   /* Half End */
    "qqqqqqqqqqqqqqqq",   /* Slow Mo */
    ".x.-yy..r.zz-.hh",   /* Glitch */
    "hh..hh..hh..hhhh",   /* Wobble */
    "XX..XX..rrrr-.zz",   /* Mangle */
};
static const char *PAT_NAMES[NPAT] = {"HALF TIME", "TAPE STOP", "STOP START", "STUTTER", "ROLL UP", "BEAT REPEAT",
                                      "REVERSE", "SPINBACK", "GATE 16THS", "GATE 8THS", "CHOP", "HALF END",
                                      "SLOW MO", "GLITCH", "WOBBLE", "MANGLE"};
static const double LEN_BEATS[3] = {2.0, 4.0, 8.0};
static const float FADE_SEC[2] = {0.0025f, 0.010f};

typedef struct { uint8_t start, len; char mode; } run_t;
static const int CHAIN_DEFAULT[NCHAIN] = {0, 0, 3, 1, -1, -1, -1, -1};   /* Half Time, Half Time, Stutter, Tape Stop */

typedef struct {
    float p[NP];
    int pending;
    float *buf;                  /* stereo, interleaved, BUF frames */
    uint32_t w;                  /* write index (frames written so far, wraps) */
    double beat, bpm;            /* song position (beats) of the next frame, tempo */
    int playing, have_pos, force_jump;
    /* each pad's pattern (editable, saved), and as runs (rebuilt when it changes) */
    char pat[NPAT][NSTEP + 1];
    run_t runs[NPAT][NSTEP]; int nrun[NPAT]; int run_of[NPAT][NSTEP];
    int chain[NCHAIN];           /* pad per chain slot, -1 = empty (the chain ends there) */
    int chain_slot;              /* the slot playing now, -1 = none */
    uint32_t rnd;
    /* playback */
    double src_prev; int have_prev; double speed_prev;
    double tail_src, tail_speed; int tail_left, tail_len;   /* the old read head, fading out */
    float g_vol, g_on, s_mix, s_out;                        /* smoothed */
    int step, step_shown;        /* step under the playhead (0..15), what the screen last read */
    unsigned rev;
    pt_theme_t theme;   /* which look MPC loads next time: not part of the saved state */
} warp_t;

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

static void build_runs(warp_t *s, int p) {
    int n = 0;
    for (int i = 0; i < NSTEP; ) {
        int j = i;
        while (j < NSTEP && s->pat[p][j] == s->pat[p][i]) j++;
        s->runs[p][n] = (run_t){(uint8_t)i, (uint8_t)(j - i), s->pat[p][i]};
        for (int k = i; k < j; k++) s->run_of[p][k] = n;
        n++; i = j;
    }
    s->nrun[p] = n;
}

static int move_index(char c) { const char *m = strchr(MOVES, c); return m && c ? (int)(m - MOVES) : 0; }
static int valid_pattern(const char *p) {
    if (strlen(p) != NSTEP) return 0;
    for (int i = 0; i < NSTEP; i++) if (!p[i] || !strchr(MOVES, p[i])) return 0;
    return 1;
}
static void set_pattern(warp_t *s, int p, const char *src) {
    memcpy(s->pat[p], src, NSTEP); s->pat[p][NSTEP] = 0;
    build_runs(s, p);
}

static uint32_t rnd(warp_t *s) { s->rnd ^= s->rnd << 13; s->rnd ^= s->rnd >> 17; s->rnd ^= s->rnd << 5; return s->rnd; }
static int pick(warp_t *s, const int *w, int n) {   /* an index, weighted */
    int t = 0; for (int i = 0; i < n; i++) t += w[i];
    int r = (int)(rnd(s) % (uint32_t)(t > 0 ? t : 1));
    for (int i = 0; i < n; i++) { if (r < w[i]) return i; r -= w[i]; }
    return 0;
}

/* DICE: a new pattern of runs that sit on the beat. wild 0 = mild (mostly straight, gentle moves), 1 = medium,
 * 2 = wild (little straight, short runs, every move). Never all straight. */
static void dice(warp_t *s, int p, int wild) {
    /*                               .   -   h   q   t   u   r   b   X   x   y   z   (MOVES order) */
    static const int W[3][NMOVE] = {{60, 6, 10, 0, 6, 0, 3, 2, 5, 6, 2, 0},
                                    {30, 9, 10, 4, 7, 3, 6, 4, 8, 9, 6, 4},
                                    {10, 10, 8, 6, 7, 6, 9, 7, 9, 10, 9, 9}};
    static const int LW[3][3] = {{2, 4, 6}, {4, 5, 3}, {6, 4, 2}};   /* run of 1, 2 or 4 steps */
    char out[NSTEP + 1];
    for (int tries = 0; tries < 20; tries++) {
        int i = 0, moves = 0; char last = 0;
        while (i < NSTEP) {
            int lw[3] = {LW[wild][0], i % 2 == 0 ? LW[wild][1] : 0, i % 4 == 0 ? LW[wild][2] : 0};   /* runs start on the beat */
            int len = (int[]){1, 2, 4}[pick(s, lw, 3)];
            if (i + len > NSTEP) len = NSTEP - i;
            int w[NMOVE]; memcpy(w, W[wild], sizeof w);
            if (i == 0) w[0] += 20;                              /* the bar usually starts straight */
            if (len < 2) { w[4] = w[5] = w[7] = 0; }              /* tape stop / start and spinback need room */
            for (int k = 0; k < NMOVE; k++) if (MOVES[k] == last && k) w[k] /= 3;   /* fewer repeats of the same move */
            char m = MOVES[pick(s, w, NMOVE)];
            if (m == 'X' && len < 2) m = 'x';
            for (int k = 0; k < len; k++) out[i + k] = m;
            if (m != '.') moves++;
            last = m; i += len;
        }
        out[NSTEP] = 0;
        if (moves && strcmp(out, s->pat[p])) break;
    }
    if (!strpbrk(out, "-hqturbXxyz")) memcpy(out + 12, "xxxx", 4);   /* never all straight */
    set_pattern(s, p, out);
}

static int chain_count(const warp_t *s) { int n = 0; while (n < NCHAIN && s->chain[n] >= 0) n++; return n; }

static void *create(const char *data_dir) {
    warp_t *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->buf = calloc((size_t)BUF * 2, sizeof(float));
    if (!s->buf) { free(s); return NULL; }
    pt_theme_init(&s->theme, data_dir);
    for (int p = 0; p < NPAT; p++) set_pattern(s, p, PAT[p]);
    for (int c = 0; c < NCHAIN; c++) s->chain[c] = CHAIN_DEFAULT[c];
    s->chain_slot = -1;
    s->rnd = 0x9e3779b9u ^ (uint32_t)(uintptr_t)s;
    s->p[P_PATTERN] = 0; s->p[P_LENGTH] = 1; s->p[P_WARP] = 1; s->p[P_FADE] = 0; s->p[P_MIX] = 100; s->p[P_OUT] = 0;
    s->p[P_EDIT] = 0; s->p[P_WILD] = 1; s->p[P_CHAIN] = 0; s->p[P_CPOS] = 0;
    s->bpm = 90.0; s->step_shown = -1;
    s->g_vol = 1; s->g_on = 1; s->s_mix = 1; s->s_out = 1;
    return s;
}
static void destroy(void *inst) { warp_t *s = inst; if (s) { free(s->buf); free(s); } }
static void midi(void *inst, const uint8_t *msg, int len) { (void)inst; (void)msg; (void)len; }

static int find(const char *key) {
    for (int i = 0; i < NP; i++) if (!strcmp(key, KEYS[i])) return i;
    return -1;
}
static void clamp_all(warp_t *s) {
    s->p[P_RESERVED] = 0;
    s->p[P_PATTERN] = clampf(roundf(s->p[P_PATTERN]), 0, NPAT - 1);
    s->p[P_LENGTH] = clampf(roundf(s->p[P_LENGTH]), 0, 2);
    s->p[P_WARP] = clampf(roundf(s->p[P_WARP]), 0, 1);
    s->p[P_FADE] = clampf(roundf(s->p[P_FADE]), 0, 1);
    s->p[P_MIX] = clampf(s->p[P_MIX], 0, 100);
    s->p[P_OUT] = clampf(s->p[P_OUT], -12, 6);
    s->p[P_EDIT] = clampf(roundf(s->p[P_EDIT]), 0, NSTEP - 1);
    s->p[P_WILD] = clampf(roundf(s->p[P_WILD]), 0, 2);
    s->p[P_CHAIN] = clampf(roundf(s->p[P_CHAIN]), 0, 1);
    s->p[P_CPOS] = clampf(roundf(s->p[P_CPOS]), 0, NCHAIN - 1);
}

static void set_param(void *inst, const char *key, const char *val) {
    warp_t *s = inst;
    if (pt_theme_set(&s->theme, key, val)) return;
    if (!strcmp(key, "state")) {   /* "key=value;key=value;...", plus pats=<16 patterns, '/' between> and chainlist=a,b,.. */
        char tmp[2048]; snprintf(tmp, sizeof tmp, "%s", val);
        for (int p = 0; p < NPAT; p++) set_pattern(s, p, PAT[p]);   /* a state without them (1.0.x) has the pads' own */
        for (int c = 0; c < NCHAIN; c++) s->chain[c] = CHAIN_DEFAULT[c];
        char *save = NULL;
        for (char *tok = strtok_r(tmp, ";", &save); tok; tok = strtok_r(NULL, ";", &save)) {
            char *eq = strchr(tok, '=');
            if (!eq) continue;
            *eq = 0;
            const char *v = eq + 1;
            if (!strcmp(tok, "pats")) {
                for (int p = 0; p < NPAT && strlen(v) >= NSTEP; p++) {
                    char one[NSTEP + 1]; memcpy(one, v, NSTEP); one[NSTEP] = 0;
                    if (valid_pattern(one)) set_pattern(s, p, one);
                    v += NSTEP; if (*v == '/') v++;
                }
            } else if (!strcmp(tok, "chainlist")) {
                for (int c = 0; c < NCHAIN; c++) {
                    int n = atoi(v); s->chain[c] = n >= 0 && n < NPAT ? n : -1;
                    const char *comma = strchr(v, ','); if (!comma) { for (c++; c < NCHAIN; c++) s->chain[c] = -1; break; }
                    v = comma + 1;
                }
            } else {
                int i = find(tok);
                if (i > 0) s->p[i] = (float)atof(v);
            }
        }
        clamp_all(s); s->chain_slot = -1; s->rev++;
        return;
    }
    int i = find(key);
    if (i <= 0) return;            /* parameter 0 is a spare: MPC flicks it on STOP and Overdub */
    float v = (float)atof(val);
    const int cur = (int)s->p[P_PATTERN];
    switch (i) {
    case P_MOVE: {                 /* what the EDIT STEP of the pad's pattern does */
        int m = (int)clampf(roundf(v), 0, NMOVE - 1);
        if (s->pat[cur][(int)s->p[P_EDIT]] != MOVES[m]) {
            s->pat[cur][(int)s->p[P_EDIT]] = MOVES[m];
            build_runs(s, cur);
        }
        s->rev++;
        return;
    }
    case P_DICE: if (v > 0.5f) { dice(s, cur, (int)s->p[P_WILD]); s->rev++; } return;          /* triggers: each press */
    case P_RESET: if (v > 0.5f) { set_pattern(s, cur, PAT[cur]); s->rev++; } return;
    case P_CPICK: {                /* the pad for the chain slot being edited: options 0..15, 16 = END */
        int n = (int)clampf(roundf(v), 0, NPAT);
        s->chain[(int)s->p[P_CPOS]] = n == NPAT ? -1 : n;
        s->rev++;
        return;
    }
    default: break;
    }
    s->p[i] = v;
    clamp_all(s);
    s->rev++;
}

/* the playing pattern: the pad, or with CHAIN ON the chain's slot for this pass (it moves the pad along) */
static void follow_chain(warp_t *s, double plen) {
    int n = chain_count(s);
    if (s->p[P_CHAIN] < 0.5f || n == 0) { s->chain_slot = -1; return; }
    double pass = floor(s->beat / plen);
    int slot = (int)fmod(pass, (double)n); if (slot < 0) slot += n;
    if (slot != s->chain_slot) {
        s->chain_slot = slot;
        if ((int)s->p[P_PATTERN] != s->chain[slot]) {
            s->p[P_PATTERN] = (float)s->chain[slot];
            s->pending |= 1u << P_PATTERN;   /* MPC's pads and PRESET follow */
        }
        s->rev++;
    }
}

int mpc_engine_changed(void *inst, char *key, int len) {
    warp_t *s = inst;
    for (int i = 0; i < NP; i++)
        if (s->pending & (1 << i)) { s->pending &= ~(1 << i); snprintf(key, len, "%s", KEYS[i]); return 1; }
    return 0;
}

static int get_param(void *inst, const char *key, char *buf, int buf_len) {
    warp_t *s = inst;
    { int pt_n = pt_theme_get(&s->theme, key, buf, buf_len); if (pt_n) return pt_n; }
    if (!strcmp(key, "state")) {
        int n = 0;
        for (size_t k = 0; k < sizeof SAVED / sizeof SAVED[0] && n < buf_len; k++)
            n += snprintf(buf + n, buf_len - n, "%s=%g;", KEYS[SAVED[k]], (double)s->p[SAVED[k]]);
        if (n < buf_len) n += snprintf(buf + n, buf_len - n, "chainlist=");
        for (int c = 0; c < NCHAIN && n < buf_len; c++) n += snprintf(buf + n, buf_len - n, c ? ",%d" : "%d", s->chain[c]);
        if (n < buf_len) n += snprintf(buf + n, buf_len - n, ";pats=");
        for (int p = 0; p < NPAT && n < buf_len; p++) n += snprintf(buf + n, buf_len - n, p ? "/%s" : "%s", s->pat[p]);
        if (n < buf_len) n += snprintf(buf + n, buf_len - n, ";");
        return n < buf_len ? n : 0;   /* never a cut-off state */
    }
    if (!strcmp(key, "display_rev")) return snprintf(buf, buf_len, "%u", s->rev);
    if (!strcmp(key, "step")) {     /* the playhead picture: 0 = none, 1..16 = that step */
        s->step_shown = s->step;
        return snprintf(buf, buf_len, "%d", s->p[P_WARP] > 0.5f ? s->step + 1 : 0);
    }
    const int cur = (int)s->p[P_PATTERN];
    if (key[0] == 's' && key[1] == 't' && key[2] >= '1' && key[2] <= '9') {   /* st1..st16: the pad's steps, for the strips */
        int k = atoi(key + 2) - 1;
        if (k >= 0 && k < NSTEP) return snprintf(buf, buf_len, "%d", move_index(s->pat[cur][k]));
        return 0;
    }
    if (key[0] == 'c' && key[1] >= '1' && key[1] <= '8' && !key[2]) {        /* c1..c8: the chain, as names */
        int c = key[1] - '1', n = chain_count(s);
        return snprintf(buf, buf_len, "%s", c < n ? PAT_NAMES[s->chain[c]] : c == n ? "END" : "");
    }
    if (!strcmp(key, "pat_name"))
        return snprintf(buf, buf_len, "%s%s", PAT_NAMES[cur], strcmp(s->pat[cur], PAT[cur]) ? "  (EDITED)" : "");
    if (!strcmp(key, "chain_now")) {
        int n = chain_count(s);
        if (s->p[P_CHAIN] < 0.5f) return snprintf(buf, buf_len, "CHAIN IS OFF: THE PAD YOU TAP PLAYS");
        if (n == 0) return snprintf(buf, buf_len, "THE CHAIN IS EMPTY: PICK A PATTERN FOR SLOT 1");
        if (s->chain_slot < 0) return snprintf(buf, buf_len, "STARTS ON THE NEXT PASS  (%d SLOT%s)", n, n > 1 ? "S" : "");
        return snprintf(buf, buf_len, "PLAYING SLOT %d OF %d:  %s", s->chain_slot + 1, n, PAT_NAMES[s->chain[s->chain_slot]]);
    }
    if (!strcmp(key, "move")) return snprintf(buf, buf_len, "%d", move_index(s->pat[cur][(int)s->p[P_EDIT]]));
    if (!strcmp(key, "chain_pick")) {
        int c = s->chain[(int)s->p[P_CPOS]];
        return snprintf(buf, buf_len, "%d", c < 0 ? NPAT : c);
    }
    if (!strncmp(key, "pattern:", 8)) {
        int n = atoi(key + 8);
        if (n >= 0 && n < NPAT) return snprintf(buf, buf_len, "%s", PAT_NAMES[n]);
        return 0;
    }
    int i = find(key);
    if (i < 0) return 0;
    if (i != P_MIX && i != P_OUT)
        return snprintf(buf, buf_len, "%d", (int)lroundf(s->p[i]));
    return snprintf(buf, buf_len, "%g", (double)s->p[i]);
}

/* Song position, once per host block. The pattern runs from a frame counter at the host tempo; MPC's position only
 * corrects it when they differ by more than its jitter (docs/MIDI_TIMING.md in the kit). */
static void song_pos(void *inst, double ppq, double bpm, int playing, int frames) {
    warp_t *s = inst;
    (void)frames;
    if (bpm > 0) s->bpm = bpm < 30 ? 30 : bpm > 300 ? 300 : bpm;
    if (ppq >= 0 && playing) {
        double d = ppq - s->beat;
        /* MPC's jitter is under 0.001 beat: ignore that. Anything more (Play, a loop wrap, a locate, the step back after a
         * tempo change) moves to MPC's position, and the jump is crossfaded. */
        if (!s->have_pos || !s->playing || fabs(d) > 0.004) { s->beat = ppq; s->force_jump = 1; }
        s->have_pos = 1;
    }
    s->playing = playing;
}

static void render(void *inst, int16_t *out_lr, int frames) {   /* an effect: silence without input */
    (void)inst;
    memset(out_lr, 0, (size_t)frames * 2 * sizeof(int16_t));
}

/* cubic (Catmull-Rom) read at a fractional frame index into the history */
static inline void read_at(const warp_t *s, double pos, float *l, float *r) {
    double fl = floor(pos);
    float t = (float)(pos - fl);
    uint32_t i = (uint32_t)(int64_t)fl;
    const float *b = s->buf;
    uint32_t i0 = (i - 1) & MASK, i1 = i & MASK, i2 = (i + 1) & MASK, i3 = (i + 2) & MASK;
    for (int c = 0; c < 2; c++) {
        float y0 = b[2 * i0 + c], y1 = b[2 * i1 + c], y2 = b[2 * i2 + c], y3 = b[2 * i3 + c];
        float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        float bb = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float cc = -0.5f * y0 + 0.5f * y2;
        float v = ((a * t + bb) * t + cc) * t + y1;
        if (c == 0) *l = v; else *r = v;
    }
}

/* Where the move asks the read head to be: the delay behind "now", in beats, x beats into a run that lasts len beats.
 * Every move starts with no delay, so going from straight-through into any move is seamless; *slice says which repeat
 * slice it is in (a new slice is a jump); *vol is 0 for silence. */
static double move_delay(char m, double x, double len, double step, int *slice, float *vol) {
    *slice = 0; *vol = 1;
    switch (m) {
    case 'h': return x * 0.5;
    case 'q': return x * 0.75;
    case 't': return x * x / (2.0 * len);                     /* speed 1 -> 0 over the run */
    case 'u': return x - x * x / (2.0 * len);                 /* speed 0 -> 1: spins up from the start of the run */
    case 'r': return 2.0 * x;                                 /* backwards from the start of the run */
    case 'b': return x + 2.0 * (x - x * x / (2.0 * len));     /* backwards at 2x, slowing to a stop */
    case 'z': case 'y': case 'x': case 'X': {
        double r = step * (m == 'z' ? 0.25 : m == 'y' ? 0.5 : m == 'x' ? 1.0 : 2.0);
        double k = floor(x / r);
        *slice = (int)k;
        return k * r;                                         /* back to the start of the run, every slice */
    }
    case '-': *vol = 0; return 0;
    default: return 0;
    }
}

/* how fast that delay grows (beats of delay per beat): the read head moves at 1 - this */
static double move_slope(char m, double x, double len) {
    switch (m) {
    case 'h': return 0.5;
    case 'q': return 0.75;
    case 't': return x / len;
    case 'u': return 1.0 - x / len;
    case 'r': return 2.0;
    case 'b': return 1.0 + 2.0 * (1.0 - x / len);
    default: return 0.0;
    }
}

static inline float soft_limit(float x) {   /* clean below -1 dBFS, then rounds off, never past 1.0 */
    return x > 0.89f ? 0.89f + 0.11f * tanhf((x - 0.89f) / 0.11f) : x < -0.89f ? -0.89f - 0.11f * tanhf((-x - 0.89f) / 0.11f) : x;
}

static void process(void *inst, const int16_t *in_lr, int16_t *out_lr, int frames) {
    warp_t *s = inst;
    const int fade = (int)s->p[P_FADE];
    const double plen = LEN_BEATS[(int)s->p[P_LENGTH]], step = plen / NSTEP;
    int pat = (int)s->p[P_PATTERN];
    const double beats_per_frame = s->bpm / 60.0 / FS, frames_per_beat = 1.0 / beats_per_frame;
    const int xf = (int)(FADE_SEC[fade] * FS);
    const float k_s = 1.0f - expf(-1.0f / (0.02f * (float)FS)), k_v = 1.0f - expf(-1.0f / (0.0015f * (float)FS));
    const float t_mix = s->p[P_MIX] / 100.0f, t_out = powf(10.0f, s->p[P_OUT] / 20.0f), t_on = s->p[P_WARP] > 0.5f ? 1.0f : 0.0f;

    for (int n = 0; n < frames; n++) {
        float dl = in_lr[2 * n] * (1.0f / 32768.0f), dr = in_lr[2 * n + 1] * (1.0f / 32768.0f);
        s->buf[2 * (s->w & MASK)] = dl; s->buf[2 * (s->w & MASK) + 1] = dr;
        double now = (double)s->w - LAT;   /* dry and warped both come out LAT frames late (45 us), so MIX never combs */
        float xl, xr;
        read_at(s, now, &xl, &xr); dl = xl; dr = xr;

        /* where in the pattern (a new pass may move the chain on) */
        if (s->p[P_CHAIN] > 0.5f || s->chain_slot >= 0) { follow_chain(s, plen); pat = (int)s->p[P_PATTERN]; }
        double ph = fmod(s->beat, plen); if (ph < 0) ph += plen;
        int st = (int)(ph / step); if (st > NSTEP - 1) st = NSTEP - 1;
        s->step = st;
        const run_t *ru = &s->runs[pat][s->run_of[pat][st]];
        double x = ph - ru->start * step, len = ru->len * step;
        int slice; float vol;
        double d_beats = move_delay(ru->mode, x, len, step, &slice, &vol);
        double d = d_beats * frames_per_beat;
        if (d > MAX_DELAY) d = MAX_DELAY;
        if (d < 0) d = 0;
        if (t_on < 0.5f) { d = 0; vol = 1; slice = 0; }
        double target = now - d;

        /* Any jump of the read head (a new repeat slice, a move that starts somewhere else, a pattern switch, a loop
         * wrap or locate, a tempo change that rescales the delay) hands the old head to a short crossfade. A jump that
         * comes while a crossfade is still running waits for it to finish (at most FADE), so a fade is never cut. */
        double speed = t_on < 0.5f ? 1.0 : 1.0 - move_slope(ru->mode, x, len), src = target;
        if (s->have_prev) {
            double cont = s->src_prev + s->speed_prev;            /* where the old head reads next */
            if (fabs(cont - target) > 0.75 || s->force_jump) {
                if (s->tail_left == 0) {
                    s->tail_src = cont; s->tail_speed = s->speed_prev; s->tail_left = s->tail_len = xf;
                    s->force_jump = 0;
                } else { src = cont; speed = s->speed_prev; }
            }
        }
        if (src > now) src = now;
        s->src_prev = src; s->speed_prev = speed; s->have_prev = 1;

        float wl, wr;
        read_at(s, src, &wl, &wr);
        if (s->tail_left > 0) {        /* equal-gain crossfade from the old head */
            float tl, tr, g = (float)s->tail_left / (float)s->tail_len;
            double ts = s->tail_src;
            if (ts > now) ts = now;   /* a head that was running at normal speed may not read the future */
            read_at(s, ts, &tl, &tr);
            wl = wl * (1.0f - g) + tl * g; wr = wr * (1.0f - g) + tr * g;
            s->tail_src += s->tail_speed;
            s->tail_left--;
        }

        s->g_vol += (vol - s->g_vol) * k_v;
        s->g_on += (t_on - s->g_on) * k_s;
        s->s_mix += (t_mix - s->s_mix) * k_s;
        s->s_out += (t_out - s->s_out) * k_s;
        wl *= s->g_vol; wr *= s->g_vol;
        float m = s->s_mix * s->g_on;
        float ol = (dl + (wl - dl) * m) * s->s_out, orr = (dr + (wr - dr) * m) * s->s_out;
        if (!(ol == ol)) ol = 0;
        if (!(orr == orr)) orr = 0;
        ol = clampf(soft_limit(ol), -1.0f, 1.0f); orr = clampf(soft_limit(orr), -1.0f, 1.0f);
        out_lr[2 * n] = (int16_t)lrintf(ol * 32767.0f);
        out_lr[2 * n + 1] = (int16_t)lrintf(orr * 32767.0f);

        s->w++;
        s->beat += beats_per_frame;
    }
    if (s->step != s->step_shown) s->rev++;
}

static const mpc_engine_t ENGINE = { create, destroy, midi, set_param, get_param, render, process, song_pos };
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }

#ifdef WARP_TEST
/* hooks for tests/test_warp.c */
const char *warp_pattern(int n) { return n >= 0 && n < NPAT ? PAT[n] : NULL; }
const char *warp_pad(void *inst, int n) { return n >= 0 && n < NPAT ? ((warp_t *)inst)->pat[n] : NULL; }
double warp_beat(void *inst) { return ((warp_t *)inst)->beat; }
#endif
