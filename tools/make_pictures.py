#!/usr/bin/env python3
"""make_pictures.py: draws the pictures of PotentialWarp's pattern strips:
  vst/pat/m00..m14.svg  one step cell per MOVE (params.json's order: straight, silent, 1/2, 1/4, tape stop, tape start,
                        backwards, spinback, repeat 1/8, 1/16, 1/32, 1/64,
                        1/8T, 1/16T, 1/32T). The strip is 16 of them side by side, one per
                        step parameter st1..st16, so an edited or rolled pattern shows as it is.
  vst/pat/s00..s16.svg  the playhead under step 1..16 (s00 = none)
  vst/pat/key.svg       the colour key under the strip
Drawn in the meter colours that shared/looks/make_looks.py recolours for each look: 0a0c0f display, 1d2127 straight,
f2b34a slower, e39b2d repeats (amber in every look), 8fb4d8 backwards, e6e9ee playhead.
The cells line up with the EDIT STEP buttons (enum_h sw=58: 58 px wide, 60 px apart)."""
import os

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
out = os.path.join(root, "vst", "pat")
os.makedirs(out, exist_ok=True)
for f in os.listdir(out):   # the old one-picture-per-pad strips (1.0.x) are gone
    if f.startswith("p") and f.endswith(".svg"):
        os.remove(os.path.join(out, f))

CW, H = 58, 56          # one cell
PITCH, N = 60, 16       # the strip: 16 cells, 2 px apart
W = PITCH * (N - 1) + CW
PH = 14
BG, OFF, SLOW, REP, BACK, MID = "0a0c0f", "1d2127", "f2b34a", "e39b2d", "8fb4d8", "e6e9ee"
FONT = 'font-family="IBM Plex Sans Condensed, Arial, sans-serif" font-weight="700"'
# (fill, label, extra drawing) per MOVE
MOVES = [
    (OFF, "", "ticks"),        # straight
    (None, "", "line"),        # silent
    (SLOW, "1/2", ""),
    (SLOW, "1/4", ""),
    (SLOW, "", "stop"),        # tape stop: the speed runs down
    (SLOW, "", "start"),       # tape start: and up
    (BACK, "REV", ""),
    (BACK, "SPIN", ""),
    (REP, "8", "rep1"),        # repeat 1/8 ... 1/64: slices drawn in the cell
    (REP, "16", "rep1"),
    (REP, "32", "rep2"),
    (REP, "64", "rep4"),
    (REP, "8T", "rep3"),       # triplet repeats (1.2.0): three slices drawn
    (REP, "16T", "rep3"),
    (REP, "32T", "rep3"),
]


def svg(w, h, body):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">\n' % (w, h, w, h)
            + "\n".join(body) + "\n</svg>\n")


for n, (fill, label, extra) in enumerate(MOVES):
    x0, x1, y0, y1 = 3, CW - 3, 7, H - 7
    e = ['<rect width="%d" height="%d" rx="6" fill="#%s"/>' % (CW, H, BG)]
    if extra == "line":
        e.append('<rect x="%d" y="%d" width="%d" height="4" rx="2" fill="#%s"/>' % (x0 + 6, H // 2 - 2, x1 - x0 - 12, OFF))
    else:
        e.append('<rect x="%d" y="%d" width="%d" height="%d" rx="4" fill="#%s"/>' % (x0, y0, x1 - x0, y1 - y0, fill))
    if extra == "stop":
        e.append('<polygon points="%d,%d %d,%d %d,%d" fill="#%s"/>' % (x0, y0, x1, y0, x1, y1 - 2, BG))
    elif extra == "start":
        e.append('<polygon points="%d,%d %d,%d %d,%d" fill="#%s"/>' % (x0, y0, x1, y0, x0, y1 - 2, BG))
    elif extra.startswith("rep") and extra != "rep1":
        k = int(extra[3])
        for j in range(1, k):
            xx = x0 + (x1 - x0) * j / k
            e.append('<rect x="%.1f" y="%d" width="2" height="%d" fill="#%s"/>' % (xx - 1, y0, 8, BG))
            e.append('<rect x="%.1f" y="%d" width="2" height="%d" fill="#%s"/>' % (xx - 1, y1 - 8, 8, BG))
    if label:
        size = 17 if len(label) <= 3 else 14
        e.append('<text x="%d" y="%d" text-anchor="middle" %s font-size="%d" fill="#%s">%s</text>'
                 % (CW // 2, H // 2 + 6, FONT, size, BG, label))
    open(os.path.join(out, "m%02d.svg" % n), "w").write(svg(CW, H, e))

for n in range(N + 1):
    e = ['<rect width="%d" height="%d" rx="4" fill="#%s"/>' % (W, PH, BG)]
    if n:
        x = (n - 1) * PITCH
        e.append('<rect x="%d" y="3" width="%d" height="%d" rx="3" fill="#%s"/>' % (x + 6, CW - 12, PH - 6, MID))
    open(os.path.join(out, "s%02d.svg" % n), "w").write(svg(W, PH, e))

# the key under the strip
LH = 22
e = ['<rect width="%d" height="%d" rx="4" fill="#%s"/>' % (W, LH, BG)]
x = 30
for col, name in [(SLOW, "SLOWER"), (REP, "REPEATS"), (BACK, "BACKWARDS"), (None, "SILENT"), (MID, "PLAYING NOW")]:
    if col is None:
        e.append('<rect x="%d" y="9" width="22" height="4" rx="2" fill="#%s"/>' % (x, OFF))
    elif col == MID:
        e.append('<rect x="%d" y="7" width="22" height="8" rx="3" fill="#%s"/>' % (x, MID))
    else:
        e.append('<rect x="%d" y="4" width="22" height="14" rx="3" fill="#%s"/>' % (x, col))
    e.append('<text x="%d" y="16" font-family="IBM Plex Sans Condensed, Arial, sans-serif" font-weight="600" font-size="14" '
             'letter-spacing="1" fill="#%s">%s</text>' % (x + 30, MID, name))
    x += 184
open(os.path.join(out, "key.svg"), "w").write(svg(W, LH, e))
print("pictures: %d moves, %d playheads, key -> %s" % (len(MOVES), N + 1, out))
