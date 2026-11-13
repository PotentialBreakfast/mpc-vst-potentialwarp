/* demo.c: renders a short demo of all 16 patterns on a made-up drum loop (one bar straight, one bar warped, each).
 *   gcc -O2 -std=gnu11 -I src -I <kit>/wrapper tools/demo.c src/warp.c -lm -o build/demo && build/demo out.wav */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#define SR 44100
static float drums(long n, double bpm, unsigned *rnd) {
    double spb = SR * 60.0 / bpm, s16 = spb / 4;
    long step = (long)(n / s16) % 16; double t = (n - floor(n / s16) * s16) / SR;
    float v = 0;
    static const char *K = "x.....x...x.....", *S = "....x.......x..x", *H = "x.x.x.x.x.x.x.xx";
    if (K[step] == 'x') v += 0.9f * expf(-t * 18) * sinf(2 * M_PI * (48 + 90 * expf(-t * 40)) * t);
    *rnd = *rnd * 1664525u + 1013904223u; float nz = ((*rnd >> 9) / 8388608.0f) * 2 - 1;
    if (S[step] == 'x') v += 0.55f * expf(-t * 22) * (0.6f * nz + 0.4f * sinf(2 * M_PI * 185 * t));
    if (H[step] == 'x') v += 0.18f * expf(-t * 60) * nz;
    double bar = n / (spb * 4); /* a bass line so pitch moves are audible */
    static const double NOTE[4] = {55, 55, 65.4, 49};
    double f = NOTE[(long)bar % 4]; v += 0.22f * sinf(2 * M_PI * f * n / SR) * (step % 4 == 3 ? 0.4f : 1.0f);
    return v * 0.7f;
}
static void w16(FILE *f, int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void w32(FILE *f, int v) { w16(f, v & 65535); w16(f, (v >> 16) & 65535); }
int main(int argc, char **argv) {
    const mpc_engine_t *e = mpc_engine(); void *s = e->create(NULL);
    double bpm = 92, spbar = SR * 60.0 / bpm * 4;
    long total = (long)(spbar * 2 * 16) + SR;
    int16_t *out = malloc(total * 4), in[256], blk[256];
    unsigned rnd = 1; double ppq = 0;
    for (long pos = 0; pos < total; pos += 128) {
        int pat = (int)(pos / (spbar * 2)); if (pat > 15) pat = 15;
        int warped = fmod(pos, spbar * 2) >= spbar;
        char v[8]; snprintf(v, sizeof v, "%d", pat); e->set_param(s, "pattern", v); e->set_param(s, "warp", warped ? "1" : "0");
        for (int i = 0; i < 128; i++) { float d = drums(pos + i, bpm, &rnd); int q = (int)lrintf(d * 32767); if (q > 32767) q = 32767; if (q < -32767) q = -32767; in[2 * i] = in[2 * i + 1] = q; }
        e->song_pos(s, ppq, bpm, 1, 128); e->process(s, in, blk, 128);
        memcpy(out + 2 * pos, blk, 128 * 4 > (total - pos) * 4 ? (total - pos) * 4 : 128 * 4);
        ppq += 128 * bpm / 60.0 / SR;
    }
    FILE *f = fopen(argv[1], "wb"); fwrite("RIFF", 1, 4, f); w32(f, 36 + total * 4); fwrite("WAVEfmt ", 1, 8, f); w32(f, 16); w16(f, 1); w16(f, 2);
    w32(f, SR); w32(f, SR * 4); w16(f, 4); w16(f, 16); fwrite("data", 1, 4, f); w32(f, total * 4); fwrite(out, 4, total, f); fclose(f);
    printf("demo: %.1f s\n", total / (double)SR); return 0;
}
