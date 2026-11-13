/* Computer test for PotentialWarp's engine: build with
 *   gcc -fsanitize=address,undefined -g -O1 -std=gnu11 -DWARP_TEST -I src -I vst/build -I <kit>/wrapper \
 *       tests/test_warp.c src/warp.c -lm -o build/test_warp
 * Drives it like MPC does (song position every block, jitter, loop wraps, tempo changes) and checks: no clicks (the
 * largest step between samples stays near what the input itself has), no NaN, the playhead follows the song position,
 * the moves do what they say (half speed halves the pitch, silence is silent, backwards plays the past backwards),
 * WARP OFF is a clean pass-through, and the saved state round-trips. Also the EDIT page (steps, DICE, RESET), the CHAIN
 * page, an old (1.0.x) state, and that rolled and edited patterns play without clicks. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"

const char *warp_pattern(int n);
const char *warp_pad(void *inst, int n);
double warp_beat(void *inst);

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { double ppq, bpm, phase, hz; int playing; double loop; unsigned rnd; } host_t;

static double jitter(host_t *h) { h->rnd = h->rnd * 1103515245u + 12345u; return ((h->rnd >> 8) % 1000) / 1000.0 * 0.0011 - 0.0009; }

/* one host block of n frames of a sine; returns the largest sample step of the output, writes the peak */
static float block(const mpc_engine_t *e, void *s, host_t *h, int n, float *peak, float amp, int16_t *keep) {
    int16_t in[2 * 512], out[2 * 512];
    for (int i = 0; i < n; i++) {
        float v = amp * (float)sin(h->phase); h->phase += 2 * M_PI * h->hz / 44100.0;
        in[2 * i] = in[2 * i + 1] = (int16_t)lrintf(v * 32767.0f);
    }
    double ppq = h->ppq + (h->playing ? jitter(h) : 0);
    e->song_pos(s, ppq < 0 ? 0 : ppq, h->bpm, h->playing, n);
    static float last = 0;
    float big = 0;
    for (int off = 0; off < n; off += 128) {
        int m = n - off < 128 ? n - off : 128;
        e->process(s, in + 2 * off, out + 2 * off, m);
    }
    for (int i = 0; i < n; i++) {
        float y = out[2 * i] / 32768.0f;
        if (!(y == y)) { fails++; printf("FAIL: NaN\n"); }
        float d = fabsf(y - last); if (d > big) big = d;
        last = y;
        if (fabsf(y) > *peak) *peak = fabsf(y);
        if (keep) keep[i] = out[2 * i];
    }
    h->ppq += n * h->bpm / 60.0 / 44100.0;
    if (h->loop > 0 && h->ppq >= h->loop) h->ppq -= h->loop;
    return big;
}

static void set(const mpc_engine_t *e, void *s, const char *k, const char *v) { e->set_param(s, k, v); }

int main(void) {
    const mpc_engine_t *e = mpc_engine();
    char buf[512];
    const float input_step = 0.5f * 2 * (float)M_PI * 220 / 44100;   /* the sine's own largest step */

    /* 1. every pattern, every length, both fades, with jitter, loop wraps and two tempo changes: no clicks, no NaN */
    for (int len = 0; len < 4; len++)
        for (int fade = 0; fade < 2; fade++)
            for (int p = 0; p < 16; p++) {
                void *s = e->create(NULL);
                char v[16];
                snprintf(v, sizeof v, "%d", p); set(e, s, "pattern", v);
                snprintf(v, sizeof v, "%d", len); set(e, s, "length", v);
                snprintf(v, sizeof v, "%d", fade); set(e, s, "fade", v);
                host_t h = {0, 92, 0, 220, 1, 16, 7u};
                float worst = 0, peak = 0;
                int sizes[3] = {128, 512, 37};
                for (int b = 0; b < 2600; b++) {
                    if (b == 900) h.bpm = 140;
                    if (b == 1700) { h.bpm = 75; h.ppq -= 0.12; }   /* MPC steps back after a tempo change */
                    float st = block(e, s, &h, sizes[b % 3], &peak, 0.5f, NULL);
                    if (b > 4 && st > worst) worst = st;
                }
                float limit = input_step * 2.2f + (fade ? 0.006f : 0.012f);
                CHECK(worst < limit, "click: pattern %d (%s) length %d fade %d: step %.4f (limit %.4f)", p, warp_pattern(p), len, fade, worst, limit);
                CHECK(peak < 0.62f, "too loud: pattern %d peak %.3f", p, peak);
                e->destroy(s);
            }

    /* 2. switching patterns on every block while playing: still no clicks */
    {
        void *s = e->create(NULL);
        host_t h = {0, 120, 0, 220, 1, 8, 3u};
        float worst = 0, peak = 0;
        for (int b = 0; b < 3000; b++) {
            char v[16]; snprintf(v, sizeof v, "%d", (b / 7) % 16); set(e, s, "pattern", v);
            float st = block(e, s, &h, 128, &peak, 0.5f, NULL);
            if (b > 4 && st > worst) worst = st;
        }
        CHECK(worst < input_step * 2.2f + 0.012f, "click when switching patterns: %.4f", worst);
        e->destroy(s);
    }

    /* 3. the playhead follows the song position (1 bar = 4 beats, step = 1/16 bar) */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "3");
        host_t h = {0, 97, 0, 220, 1, 0, 5u};
        float peak = 0; int bad = 0, n = 0;
        for (int b = 0; b < 4000; b++) {
            double ppq = h.ppq;
            block(e, s, &h, 128, &peak, 0.3f, NULL);
            e->get_param(s, "step", buf, sizeof buf);
            double end = ppq + 128 * 97 / 60.0 / 44100.0;   /* the step after the block */
            int want = (int)(fmod(end, 4.0) / 0.25) + 1;
            int got = atoi(buf);
            if (got != want && fabs(fmod(end, 0.25)) > 0.003 && fabs(fmod(end, 0.25) - 0.25) > 0.003) bad++;
            n++;
        }
        CHECK(bad == 0, "playhead off the song position on %d of %d blocks", bad, n);
        double drift = warp_beat(s) - h.ppq;
        CHECK(fabs(drift) < 0.01, "beat drifted %.5f", drift);
        e->destroy(s);
    }

    /* 4. what the moves do. Half Time (pattern 0): 440 Hz in, about 220 Hz out (count zero crossings in bar 3) */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "0");
        host_t h = {0, 120, 0, 440, 1, 0, 9u};
        static int16_t rec[44100 * 8];
        int pos = 0; float peak = 0;
        while (pos + 128 <= 44100 * 6) { block(e, s, &h, 128, &peak, 0.5f, rec + pos); pos += 128; }
        /* at 120 bpm a bar is 2 s; bar 3 = 4.0..6.0 s. Use 4.1..5.9 s */
        int zc = 0;
        for (int i = 44100 * 41 / 10; i < 44100 * 59 / 10; i++) if ((rec[i - 1] < 0) != (rec[i] < 0)) zc++;
        double hz = zc / 2.0 / 1.8;
        CHECK(fabs(hz - 220) < 6, "half time: %.1f Hz out of 440 Hz in (want 220)", hz);
        e->destroy(s);
    }
    /* Gate 16ths (pattern 8): the off steps are silent, the on steps are not */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "8");
        host_t h = {0, 120, 0, 220, 1, 0, 2u};
        static int16_t rec[44100 * 4];
        int pos = 0; float peak = 0;
        while (pos + 128 <= 44100 * 4) { block(e, s, &h, 128, &peak, 0.5f, rec + pos); pos += 128; }
        /* a 16th at 120 bpm = 5512.5 frames; check the middle of steps 1 (silent) and 2 (sound) of bar 2 */
        double sixteenth = 44100 * 0.125;
        int silent = 0, loud = 0;
        for (int st = 16; st < 32; st++) {
            int mid = (int)((st + 0.5) * sixteenth);
            int mx = 0;
            for (int i = mid - 1000; i < mid + 1000; i++) if (abs(rec[i]) > mx) mx = abs(rec[i]);
            if (st % 2 == 1) silent += mx < 30; else loud += mx > 10000;
        }
        CHECK(silent == 8 && loud == 8, "gate 16ths: %d of 8 off steps silent, %d of 8 on steps sounding", silent, loud);
        e->destroy(s);
    }
    /* Reverse (pattern 6): during a backwards run the output is the input from before the run, played backwards.
     * Feed a rising ramp (a slow saw) and check the output falls during steps 4-7 of bar 2. */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "6");
        host_t h = {0, 120, 0, 0, 1, 0, 2u};
        static int16_t rec[44100 * 4];
        int16_t in[256], out[256];
        for (int pos = 0; pos + 128 <= 44100 * 4; pos += 128) {
            for (int i = 0; i < 128; i++) { int v = ((pos + i) % 40000) - 20000; in[2 * i] = in[2 * i + 1] = (int16_t)v; }
            e->song_pos(s, h.ppq, h.bpm, 1, 128);
            e->process(s, in, out, 128);
            for (int i = 0; i < 128; i++) rec[pos + i] = out[2 * i];
            h.ppq += 128 * 120 / 60.0 / 44100.0;
        }
        double sixteenth = 44100 * 0.125;
        int a = (int)(20.3 * sixteenth), b = (int)(23.7 * sixteenth), falls = 0, total = 0;
        for (int i = a + 1; i < b; i++) { total++; if (rec[i] < rec[i - 1] || rec[i - 1] - rec[i] > 30000) falls++; }
        CHECK(falls > total * 0.97, "reverse: output fell on %d of %d frames", falls, total);
        e->destroy(s);
    }

    /* 5. WARP OFF passes the sound straight through (2 frames late) */
    {
        void *s = e->create(NULL);
        set(e, s, "warp", "0"); set(e, s, "pattern", "13");
        host_t h = {0, 120, 0, 0, 1, 0, 1u};
        int16_t in[256], out[256], hist[4] = {0};
        int bad = 0;
        for (int b = 0; b < 400; b++) {
            for (int i = 0; i < 128; i++) { int v = (int)(rand() % 40000) - 20000; in[2 * i] = in[2 * i + 1] = (int16_t)v; }
            e->song_pos(s, h.ppq, h.bpm, 1, 128);
            e->process(s, in, out, 128);
            for (int i = 0; i < 128; i++) {
                if (b > 20 && abs(out[2 * i] - hist[1]) > 2) bad++;
                hist[1] = hist[0]; hist[0] = in[2 * i];
            }
            h.ppq += 128 * 120 / 60.0 / 44100.0;
        }
        CHECK(bad == 0, "WARP OFF changed %d samples", bad);
        e->destroy(s);
    }

    /* 6. state: save, change everything, restore */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "11"); set(e, s, "length", "2"); set(e, s, "warp", "0"); set(e, s, "fade", "1");
        set(e, s, "mix", "63"); set(e, s, "output", "-4.5");
        char st[4096]; e->get_param(s, "state", st, sizeof st);
        void *t = e->create(NULL);
        e->set_param(t, "state", st);
        char st2[4096]; e->get_param(t, "state", st2, sizeof st2);
        CHECK(!strcmp(st, st2), "state round-trip: '%s' vs '%s'", st, st2);
        e->set_param(t, "reserved", "1");   /* MPC flicks parameter 0: nothing may change */
        char st3[4096]; e->get_param(t, "state", st3, sizeof st3);
        CHECK(!strcmp(st, st3), "parameter 0 changed the state");
        e->get_param(t, "pattern:7", buf, sizeof buf);
        CHECK(!strcmp(buf, "SPINBACK"), "pattern name 7: %s", buf);
        e->destroy(s); e->destroy(t);
    }

    /* 7. no song position at all (stopped, ppq -1): runs from its own clock, no crash, no clicks */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "4");
        host_t h = {0, 0, 0, 220, 0, 0, 1u};
        float worst = 0, peak = 0;
        int16_t in[256], out[256];
        static float last = 0;
        for (int b = 0; b < 2000; b++) {
            for (int i = 0; i < 128; i++) { float v = 0.5f * (float)sin(h.phase); h.phase += 2 * M_PI * 220 / 44100.0; in[2 * i] = in[2 * i + 1] = (int16_t)lrintf(v * 32767); }
            e->song_pos(s, -1, 0, 0, 128);
            e->process(s, in, out, 128);
            for (int i = 0; i < 128; i++) { float y = out[2 * i] / 32768.0f, d = fabsf(y - last); if (b > 2 && d > worst) worst = d; last = y; if (fabsf(y) > peak) peak = fabsf(y); }
        }
        CHECK(worst < input_step * 2.2f + 0.012f, "no position: click %.4f", worst);
        CHECK(peak > 0.3f, "no position: no sound");
        e->destroy(s);
    }

    /* 8. EDIT: a step changes the pad's pattern; the strips show it; RESET puts it back; it is saved */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "0");   /* Half Time: hhhh... */
        set(e, s, "edit_step", "3"); set(e, s, "move", "1");   /* step 4 silent */
        CHECK(warp_pad(s, 0)[3] == '-', "edit: step 4 is '%c'", warp_pad(s, 0)[3]);
        CHECK(!strcmp(warp_pattern(0), "hhhhhhhhhhhhhhhh"), "edit changed the factory pattern");
        e->get_param(s, "st4", buf, sizeof buf); CHECK(!strcmp(buf, "1"), "st4: %s", buf);
        e->get_param(s, "move", buf, sizeof buf); CHECK(!strcmp(buf, "1"), "move: %s", buf);
        e->get_param(s, "pat_name", buf, sizeof buf); CHECK(strstr(buf, "EDITED") != NULL, "pat_name: %s", buf);
        set(e, s, "edit_step", "0"); e->get_param(s, "move", buf, sizeof buf); CHECK(!strcmp(buf, "2"), "move of step 1: %s", buf);
        char st[4096]; e->get_param(s, "state", st, sizeof st);
        void *t = e->create(NULL); e->set_param(t, "state", st);
        CHECK(!strcmp(warp_pad(t, 0), warp_pad(s, 0)), "edited pattern not restored: %s", warp_pad(t, 0));
        set(e, s, "reset", "1");
        CHECK(!strcmp(warp_pad(s, 0), warp_pattern(0)), "reset: %s", warp_pad(s, 0));
        e->get_param(s, "pat_name", buf, sizeof buf); CHECK(!strcmp(buf, "HALF TIME"), "pat_name after reset: %s", buf);
        /* an old (1.0.x) state has no patterns or chain: the pads' own patterns, chain off */
        e->set_param(t, "state", "pattern=5;length=1;warp=1;fade=0;mix=100;output=0;");
        CHECK(!strcmp(warp_pad(t, 0), warp_pattern(0)), "old state kept an edited pattern");
        e->get_param(t, "chain", buf, sizeof buf); CHECK(!strcmp(buf, "0"), "old state: chain %s", buf);
        e->get_param(t, "pattern", buf, sizeof buf); CHECK(!strcmp(buf, "5"), "old state: pattern %s", buf);
        e->destroy(s); e->destroy(t);
    }

    /* 9. DICE: every wildness gives a valid pattern that isn't all straight, changes the pad, and plays without clicks */
    for (int wild = 0; wild < 3; wild++) {
        void *s = e->create(NULL);
        char w[8]; snprintf(w, sizeof w, "%d", wild); set(e, s, "wild", w);
        int straight_steps = 0;
        for (int roll = 0; roll < 200; roll++) {
            set(e, s, "pattern", "7");
            char before[17]; snprintf(before, sizeof before, "%s", warp_pad(s, 7));
            set(e, s, "dice", "1");
            const char *p = warp_pad(s, 7);
            CHECK(strlen(p) == 16 && strspn(p, ".-hqturbXxyzABC") == 16, "dice %d: bad pattern %s", wild, p);
            CHECK(strpbrk(p, "-hqturbXxyzABC") != NULL, "dice %d: all straight", wild);
            CHECK(strcmp(p, before), "dice %d: same pattern twice", wild);
            for (int i = 0; i < 16; i++) straight_steps += p[i] == '.';
        }
        printf("dice %s: %.0f%% straight steps\n", wild == 0 ? "MILD" : wild == 1 ? "MEDIUM" : "WILD", straight_steps / 32.0);
        if (wild == 0) CHECK(straight_steps > 200 * 16 / 2, "MILD should be mostly straight (%d)", straight_steps);
        if (wild == 2) CHECK(straight_steps < 200 * 16 / 3, "WILD should be mostly moves (%d)", straight_steps);
        /* play 40 rolls, one bar each, at a tempo change and loop wraps: no clicks, no NaN */
        host_t h = {0, 120, 0, 220, 1, 8, 7u};
        float worst = 0, peak = 0;
        for (int roll = 0; roll < 40; roll++) {
            set(e, s, "dice", "1");
            for (int b = 0; b < 172; b++) { float st = block(e, s, &h, 128, &peak, 0.5f, NULL); if (b > 2 && st > worst) worst = st; }
            if (roll == 20) h.bpm = 93;
        }
        CHECK(worst < input_step * 2.2f + 0.02f, "dice %d: click %.4f", wild, worst);
        e->destroy(s);
    }

    /* 10. CHAIN: the slots play in turn, one LENGTH each, the pad follows, END ends it, and it is saved */
    {
        void *s = e->create(NULL);
        set(e, s, "length", "1");    /* 1 bar = 4 beats */
        set(e, s, "chain_pos", "0"); set(e, s, "chain_pick", "5");
        set(e, s, "chain_pos", "1"); set(e, s, "chain_pick", "8");
        set(e, s, "chain_pos", "2"); set(e, s, "chain_pick", "16");   /* END: a chain of 2 */
        e->get_param(s, "c3", buf, sizeof buf); CHECK(!strcmp(buf, "END"), "c3: %s", buf);
        e->get_param(s, "c4", buf, sizeof buf); CHECK(!strcmp(buf, ""), "c4: %s", buf);
        e->get_param(s, "chain_pick", buf, sizeof buf); CHECK(!strcmp(buf, "16"), "chain_pick: %s", buf);
        set(e, s, "chain", "1");
        host_t h = {0, 120, 0, 220, 1, 0, 3u};
        float peak = 0;
        int seen[3] = {-1, -1, -1};
        for (int bar = 0; bar < 3; bar++) {
            for (int b = 0; b < 600; b++) block(e, s, &h, 128, &peak, 0.5f, NULL);   /* 600 x 128 frames ~ 1.74 s < 1 bar */
            e->get_param(s, "pattern", buf, sizeof buf); seen[bar] = atoi(buf);
            for (int b = 0; b < 89; b++) block(e, s, &h, 128, &peak, 0.5f, NULL);
        }
        CHECK(seen[0] == 5 && seen[1] == 8 && seen[2] == 5, "chain played %d %d %d", seen[0], seen[1], seen[2]);
        char key[64]; int reported = 0;
        while (mpc_engine_changed(s, key, sizeof key)) reported += !strcmp(key, "pattern");
        CHECK(reported >= 1, "the pad change was not reported to MPC");
        e->get_param(s, "chain_now", buf, sizeof buf); CHECK(strstr(buf, "OF 2") != NULL, "chain_now: %s", buf);
        char st[4096]; e->get_param(s, "state", st, sizeof st);
        void *t = e->create(NULL); e->set_param(t, "state", st);
        char st2[4096]; e->get_param(t, "state", st2, sizeof st2);
        CHECK(!strcmp(st, st2), "chain state round-trip: '%s' vs '%s'", st, st2);
        set(e, s, "chain", "0");
        e->get_param(s, "chain_now", buf, sizeof buf); CHECK(strstr(buf, "OFF") != NULL, "chain off: %s", buf);
        e->destroy(s); e->destroy(t);
    }

    /* 11. (1.2.0) SWING moves the off 16ths later: with Gate 16ths at 75 %, step 2's sound starts half a step late */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "8"); set(e, s, "swing", "75");
        host_t h = {0, 120, 0, 220, 1, 0, 2u};
        static int16_t rec[44100 * 4];
        int pos = 0; float peak = 0;
        while (pos + 128 <= 44100 * 4) { block(e, s, &h, 128, &peak, 0.5f, rec + pos); pos += 128; }
        double sixteenth = 44100 * 0.125;   /* bar 2: steps 16..31; each silent off step now starts half a step late */
        int quiet_late = 0, loud_late = 0;
        for (int pair = 8; pair < 16; pair++) {
            int a = (int)((2 * pair + 1.1) * sixteenth), b = (int)((2 * pair + 1.65) * sixteenth);
            int mx = 0; for (int i = a; i < a + 600; i++) if (abs(rec[i]) > mx) mx = abs(rec[i]);
            loud_late += mx > 10000;   /* unswung this would be the silent step: swung, the on step is still sounding */
            mx = 0; for (int i = b; i < b + 600; i++) if (abs(rec[i]) > mx) mx = abs(rec[i]);
            quiet_late += mx < 30;     /* and the off step's silence comes after */
        }
        CHECK(quiet_late == 8 && loud_late == 8, "swing 75%%: %d of 8 swung off steps silent late, %d of 8 sounding after", quiet_late, loud_late);
        e->destroy(s);
    }

    /* 12. (1.2.0) 4 BARS: the playhead takes 16 beats for 16 steps; triplet repeats repeat every 2/3 step */
    {
        void *s = e->create(NULL);
        set(e, s, "pattern", "3"); set(e, s, "length", "3");
        host_t h = {0, 120, 0, 220, 1, 0, 5u};
        float peak = 0;
        for (int b = 0; b < 1550; b++) block(e, s, &h, 128, &peak, 0.3f, NULL);   /* about 4.5 s = 9 beats */
        e->get_param(s, "step", buf, sizeof buf);
        int want = (int)(fmod(h.ppq, 16.0) / 1.0) + 1;
        CHECK(abs(atoi(buf) - want) <= 1, "4 bars: step %s, want about %d", buf, want);
        e->get_param(s, "length", buf, sizeof buf); CHECK(!strcmp(buf, "3"), "length 4 bars: %s", buf);
        /* every step a 1/16T repeat: a ramp input repeats with a period of 2/3 of a 16th */
        set(e, s, "length", "1");
        for (int k = 0; k < 16; k++) { char v[8]; snprintf(v, sizeof v, "%d", k); set(e, s, "edit_step", v); set(e, s, "move", "13"); }
        CHECK(!strcmp(warp_pad(s, 3), "BBBBBBBBBBBBBBBB"), "triplet edit: %s", warp_pad(s, 3));
        e->get_param(s, "st5", buf, sizeof buf); CHECK(!strcmp(buf, "13"), "st5 shows %s", buf);
        static int16_t rec[44100 * 4];
        int16_t in[256], out[256];
        host_t g = {0, 120, 0, 0, 1, 0, 2u};
        void *t = e->create(NULL);
        char st[4096]; e->get_param(s, "state", st, sizeof st); e->set_param(t, "state", st);
        for (int pos = 0; pos + 128 <= 44100 * 4; pos += 128) {
            for (int i = 0; i < 128; i++) { int v = ((pos + i) % 40000) - 20000; in[2 * i] = in[2 * i + 1] = (int16_t)v; }
            e->song_pos(t, g.ppq, g.bpm, 1, 128); e->process(t, in, out, 128);
            for (int i = 0; i < 128; i++) rec[pos + i] = out[2 * i];
            g.ppq += 128 * 120 / 60.0 / 44100.0;
        }
        /* count the slice restarts in bar 2 (the ramp falls back, over the short crossfade): 16 steps x 1.5 = 24, plus
           the ramp's own wraps */
        int jumps = 0, falling = 0;
        for (int i = 88200 + 50; i < 176400 - 50; i++) {
            int f = rec[i] < rec[i - 1] - 8;
            if (f && !falling) jumps++;
            falling = f;
        }
        CHECK(jumps >= 22 && jumps <= 29, "1/16T repeats: %d slice restarts in a bar (want about 24)", jumps);
        e->destroy(s); e->destroy(t);
    }

    /* 13. (1.2.0) STOP FILTER: during a tape stop the top end goes; without it, it doesn't. Old states have it off. */
    {
        double hf[2];
        for (int on = 0; on < 2; on++) {
            void *s = e->create(NULL);
            set(e, s, "pattern", "1"); set(e, s, "stop_filter", on ? "1" : "0");
            static int16_t rec[44100 * 4];
            int16_t in[256], out[256]; double ppq = 0; unsigned r = 1;
            for (int pos = 0; pos + 128 <= 44100 * 4; pos += 128) {   /* white noise in */
                for (int i = 0; i < 128; i++) { r = r * 1103515245u + 12345u; int v = (int)((r >> 16) % 20000) - 10000; in[2 * i] = in[2 * i + 1] = (int16_t)v; }
                e->song_pos(s, ppq, 120, 1, 128); e->process(s, in, out, 128);
                for (int i = 0; i < 128; i++) rec[pos + i] = out[2 * i];
                ppq += 128 * 2.0 / 44100;
            }
            /* bar 2, steps 9-14 of the tape stop: the high end (sample-to-sample differences) */
            double sixteenth = 44100 * 0.125, en = 0;
            for (int i = (int)(25 * sixteenth); i < (int)(30 * sixteenth); i++) { double d = rec[i] - rec[i - 1]; en += d * d; }
            hf[on] = en;
            e->destroy(s);
        }
        CHECK(hf[1] < hf[0] * 0.5, "stop filter: tape stop energy %.3g with, %.3g without", hf[1], hf[0]);
        void *t = e->create(NULL);
        e->set_param(t, "state", "pattern=1;length=1;warp=1;fade=0;mix=100;output=0;");
        e->get_param(t, "stop_filter", buf, sizeof buf); CHECK(!strcmp(buf, "0"), "old state: stop filter %s", buf);
        e->get_param(t, "swing", buf, sizeof buf); CHECK(atof(buf) == 50, "old state: swing %s", buf);
        e->destroy(t);
    }

    /* 14. (1.2.0) ONE SHOT: a pad plays its pattern once, then straight through. HOLD: only while the note is held. */
    {
        void *s = e->create(NULL);
        set(e, s, "play", "1");
        host_t h = {0, 120, 0, 220, 1, 0, 4u};
        float peak = 0;
        for (int b = 0; b < 1100; b++) block(e, s, &h, 128, &peak, 0.5f, NULL);   /* past the 3 s a project needs to open */
        e->get_param(s, "step", buf, sizeof buf); CHECK(!strcmp(buf, "0"), "one shot: playing before a tap (%s)", buf);
        set(e, s, "pattern", "8"); set(e, s, "pattern", "8");   /* one tap = two presses */
        for (int b = 0; b < 30; b++) block(e, s, &h, 128, &peak, 0.5f, NULL);
        e->get_param(s, "step", buf, sizeof buf); CHECK(atoi(buf) >= 1 && atoi(buf) <= 2, "one shot: step %s after a tap", buf);
        static int16_t rec[44100 * 3];
        int pos = 0;
        while (pos + 128 <= 44100 * 3) { block(e, s, &h, 128, &peak, 0.5f, rec + pos); pos += 128; }
        int silent_early = 0, silent_late = 0;   /* gate 16ths: silent gaps during the shot (first 1.8 s), none after 2.2 s */
        for (int i = 0; i < 44100 * 18 / 10; i += 400) { int mx = 0; for (int k = i; k < i + 400; k++) if (abs(rec[k]) > mx) mx = abs(rec[k]); silent_early += mx < 30; }
        for (int i = 44100 * 22 / 10; i < 44100 * 3 - 400; i += 400) { int mx = 0; for (int k = i; k < i + 400; k++) if (abs(rec[k]) > mx) mx = abs(rec[k]); silent_late += mx < 30; }
        CHECK(silent_early > 40 && silent_late == 0, "one shot: %d silent bits during, %d after", silent_early, silent_late);
        /* HOLD with MIDI */
        set(e, s, "play", "2");
        uint8_t on[3] = {0x90, 36 + 8, 100}, off[3] = {0x80, 36 + 8, 0};
        e->midi(s, on, 3);
        for (int b = 0; b < 200; b++) block(e, s, &h, 128, &peak, 0.5f, NULL);
        e->get_param(s, "step", buf, sizeof buf); int held = atoi(buf);
        e->midi(s, off, 3);
        block(e, s, &h, 128, &peak, 0.5f, NULL);
        e->get_param(s, "step", buf, sizeof buf);
        CHECK(held > 0 && !strcmp(buf, "0"), "hold: step %d while held, %s after", held, buf);
        e->get_param(s, "pattern", buf, sizeof buf); CHECK(!strcmp(buf, "8"), "hold: note 44 picked pattern %s", buf);
        /* clicks: hold on and off on every few blocks */
        float worst = 0;
        for (int b = 0; b < 2000; b++) {
            if (b % 37 == 0) e->midi(s, on, 3);
            if (b % 37 == 20) e->midi(s, off, 3);
            float stp = block(e, s, &h, 128, &peak, 0.5f, NULL); if (b > 2 && stp > worst) worst = stp;
        }
        CHECK(worst < input_step * 2.2f + 0.02f, "hold on/off: click %.4f", worst);
        e->destroy(s);
    }

    printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
