#!/usr/bin/env python3
"""make_pictures.py: draws vst/pat/p00..p15.svg (each pattern as a strip of moves) and vst/pat/s00..s16.svg (the playhead
under step 1..16; s00 = none) from the patterns in src/warp.c. Drawn in the meter colours that shared/looks/make_looks.py
recolours for each look: 0a0c0f display, 1d2127 straight, f2b34a slower, f5c15a repeats, 8fb4d8 backwards, e6e9ee playhead."""
import os, re, sys
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
src = open(os.path.join(root, "src", "warp.c")).read()
block = src[src.index("static const char *PAT[NPAT]"):]
pats = re.findall(r'"([.hqtuzyxXrb-]{16})"', block[:block.index("};")])
assert len(pats) == 16, len(pats)
out = os.path.join(root, "vst", "pat")
os.makedirs(out, exist_ok=True)
W, H, PH = 960, 56, 14
CW = W / 16.0
BG, OFF, SLOW, REP, BACK, MID = "0a0c0f", "1d2127", "f2b34a", "e39b2d", "8fb4d8", "e6e9ee"   # REP is not a meter colour: amber in every look
FAM = {".": OFF, "h": SLOW, "q": SLOW, "t": SLOW, "u": SLOW, "z": REP, "y": REP, "x": REP, "X": REP, "r": BACK, "b": BACK}
SLICE = {"z": 0.25, "y": 0.5, "x": 1.0, "X": 2.0}
LABEL = {"h": "1/2", "q": "1/4", "r": "BACK", "b": "SPIN"}


def runs(p):
    i = 0
    while i < 16:
        j = i
        while j < 16 and p[j] == p[i]:
            j += 1
        yield i, j - i, p[i]
        i = j


for n, p in enumerate(pats):
    e = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (W, H, W, H),
         '<rect width="%d" height="%d" rx="6" fill="#%s"/>' % (W, H, BG)]
    for st, ln, m in runs(p):
        x0, x1, y0, y1 = st * CW + 3, (st + ln) * CW - 3, 7, H - 7
        if m == "-":    # silence: a flat line
            e.append('<rect x="%.1f" y="%d" width="%.1f" height="4" rx="2" fill="#%s"/>' % (x0 + 4, H // 2 - 2, x1 - x0 - 8, OFF))
            continue
        e.append('<rect x="%.1f" y="%d" width="%.1f" height="%d" rx="4" fill="#%s"/>' % (x0, y0, x1 - x0, y1 - y0, FAM[m]))
        if m == ".":    # step ticks
            for k in range(1, ln):
                xx = (st + k) * CW
                e.append('<rect x="%.1f" y="%d" width="2" height="%d" fill="#%s"/>' % (xx - 1, y0 + 12, y1 - y0 - 24, BG))
        elif m in SLICE:   # one divider per repeat slice
            k = SLICE[m]
            t = k
            while t < ln - 1e-6:
                xx = (st + t) * CW
                e.append('<rect x="%.1f" y="%d" width="3" height="%d" fill="#%s"/>' % (xx - 1.5, y0, y1 - y0, BG))
                t += k
        elif m in "tu":    # the speed ramp: the part above the line is cut away
            if m == "t":
                pts = "%.1f,%d %.1f,%d %.1f,%d" % (x0, y0, x1, y0, x1, y1 - 2)
            else:
                pts = "%.1f,%d %.1f,%d %.1f,%d" % (x0, y0, x1, y0, x0, y1 - 2)
            e.append('<polygon points="%s" fill="#%s"/>' % (pts, BG))
        if m in LABEL and x1 - x0 > 70:
            e.append('<text x="%.1f" y="%d" text-anchor="middle" font-family="IBM Plex Sans Condensed, Arial, sans-serif" '
                     'font-weight="700" font-size="18" fill="#%s">%s</text>' % ((x0 + x1) / 2, y0 + 27, BG, LABEL[m]))
    e.append("</svg>")
    open(os.path.join(out, "p%02d.svg" % n), "w").write("\n".join(e) + "\n")

for n in range(17):
    e = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (W, PH, W, PH),
         '<rect width="%d" height="%d" rx="4" fill="#%s"/>' % (W, PH, BG)]
    if n:
        x = (n - 1) * CW
        e.append('<rect x="%.1f" y="3" width="%.1f" height="%d" rx="3" fill="#%s"/>' % (x + 6, CW - 12, PH - 6, MID))
    e.append("</svg>")
    open(os.path.join(out, "s%02d.svg" % n), "w").write("\n".join(e) + "\n")
# the key under the strip
L, LH = W, 22
e = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (L, LH, L, LH),
     '<rect width="%d" height="%d" rx="4" fill="#%s"/>' % (L, LH, BG)]
items = [(SLOW, "SLOWER"), (REP, "REPEATS"), (BACK, "BACKWARDS"), (None, "SILENT"), (MID, "PLAYING NOW")]
x = 40
for col, name in items:
    if col is None:
        e.append('<rect x="%d" y="9" width="22" height="4" rx="2" fill="#%s"/>' % (x, OFF))
    elif col == MID:
        e.append('<rect x="%d" y="7" width="22" height="8" rx="3" fill="#%s"/>' % (x, MID))
    else:
        e.append('<rect x="%d" y="4" width="22" height="14" rx="3" fill="#%s"/>' % (x, col))
    e.append('<text x="%d" y="16" font-family="IBM Plex Sans Condensed, Arial, sans-serif" font-weight="600" font-size="14" '
             'letter-spacing="1" fill="#%s">%s</text>' % (x + 30, MID, name))
    x += 184
e.append("</svg>")
open(os.path.join(out, "key.svg"), "w").write("\n".join(e) + "\n")
print("pictures: 16 patterns, 17 playheads ->", out)
