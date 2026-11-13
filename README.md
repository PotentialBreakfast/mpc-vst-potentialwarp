# PotentialWarp

**Bend time on the beat.** A tempo-locked mangler for your MPC, in the spirit of Gross Beat: tap one of 16 patterns and whatever runs through it stutters, drops to half speed, runs down like a stopped tape, spins back, plays backwards or gates in time with your beat. Made for breaks, loops and experimental sample flips.

![PotentialWarp's page on the MPC](docs/images/potentialwarp.png)

*Insert effect · MPC OS 3.x · Gen1 and Gen2 · v1.1.1*

Free and open source (MIT), made by **PotentialBreaks**. It runs inside the MPC like its own effects, with its own touchscreen page, Q-Link control and project recall, and no computer needed while you play.

**[Download the latest release](../../releases/latest)** · [Install guide](INSTALL-GUIDE.md) · [FAQ](FAQ.md) · [Ideas and bugs](../../issues/new/choose)

## What it does

It keeps the last few seconds of whatever you feed it and plays them back through a 16-step **pattern** locked to your sequence. Each step can:

| Move | What you hear |
|---|---|
| **Straight** | The sound as it is |
| **1/2, 1/4 speed** | Drops an octave (or two) and drags behind the beat |
| **Tape stop / start** | Slows to a stop, or spins up from silence |
| **Repeat** | Stutters a slice, from a whole 8th down to a 64th roll |
| **Backwards / Spin** | Plays the last moment in reverse, or a fast spinback |
| **Silent** | Gates the sound out |

| Control | What it does |
|---|---|
| **PATTERNS** | 16 one-tap pads (also in the MPC's PRESET menu): Half Time, Tape Stop, Stop Start, Stutter, Roll Up, Beat Repeat, Reverse, Spinback, Gate 16ths, Gate 8ths, Chop, Half End, Slow Mo, Glitch, Wobble, Mangle. Every pad can be edited or rolled anew (below) |
| **LENGTH** | How long the 16 steps last: **1/2 BAR**, **1 BAR**, **2 BARS** or **4 BARS** |
| **SWING** | 50 % (straight) to 75 %, MPC style: the off 16th steps start later, so gates, chops and repeats swing with your beat |
| **FADE** | **TIGHT** (short, punchy jumps) or **SOFT** (smoother) |
| **WARP** | **ON** / **OFF**. Automate it to drop the effect in and out |
| **PLAY** | **ALWAYS**: the pattern runs with the song, every bar. **ONE SHOT**: tap a pattern (or play a pad) and it plays once, from the next 16th, then the sound goes straight through again. **HOLD**: the pattern plays only while you hold a pad, like a performance effect. Pads A01-A16 play patterns 1-16 (HOLD needs pads or keys: the touchscreen doesn't tell a plugin when your finger lifts) |
| **STOP FILTER** | A low-pass that closes as a Tape Stop runs down and opens as a Tape Start spins up, like a real deck. On for new copies; projects from before 1.2.0 keep it off so they sound as they did |
| **MIX / OUTPUT** | Blend with the original sound, set the level |

The strip at the top shows the pattern you picked, step by step and coloured by move, with a light under the step that's playing now.

### EDIT page: make the pattern your own

| Control | What it does |
|---|---|
| **Steps 1-16** | Tap a step... |
| **WHAT THE STEP DOES** | ...then tap what it does: Straight, Silent, 1/2 or 1/4 Speed, Tape Stop, Tape Start, Backwards, Spinback, Repeat 1/8, 1/16, 1/32, 1/64, or triplet repeats 1/8T, 1/16T, 1/32T (the repeat sizes are at LENGTH 1 BAR). Steps next to each other with the same move join into one longer move, so four Tape Stop steps make one long stop |
| **ROLL** | Dice: rolls a brand-new pattern for the pad, on the beat. Tap again for another |
| **MILD / MEDIUM / WILD** | How far ROLL goes: mostly straight with a few gentle moves, a mix, or almost all moves |
| **RESET PAD** | Puts the pad's own pattern back |

Edits and rolls change the pad you're on (its name shows "EDITED"), and every pad's pattern is saved with your project.

### CHAIN page: patterns that change as you go

Tap a slot (1-8), then the pattern for it, and turn **CHAIN** on. The slots play in turn, one pattern LENGTH each, and start again after the last one (or at the first **END**), so a chain like Half Time, Half Time, Stutter, Tape Stop changes over a phrase by itself. The pads follow the chain as it plays. Turn CHAIN off to stay on one pad.

## How to use it

Add it as an **insert effect** on a track, program, pad or bus. It won't be in the instrument list. Press **Play**, then tap a pattern. Try **Half Time** or **Tape Stop** on a whole drum bus, **Stutter** or **Roll Up** on a break, **Reverse** on a vocal chop.

For a live feel, map **WARP** or **PATTERN** to a Q-Link and record the moves as automation. Or press **ROLL** until something good comes up, then fix the steps you don't like on the EDIT page.

## Three looks

PotentialWarp comes in three looks: **Print Plate** (the default), **Night Rack** and **Colour Block**. Open the **SETTINGS** page and tap one of the big LOOK keys. The new look shows the next time the plugin loads: take it off the track and add it again, or load your project again. Your choice is kept on your MPC for every project. The sound and the controls are the same in all three.

## Limitations

- **It follows the song position.** The patterns line up with your bars while the sequence plays. When it's stopped, it keeps its own time at the project tempo, so pads you play still get warped, but not lined up to a bar.
- **4/4 time.** One bar is four beats.
- **It can only use what it has already heard.** Every move starts at the start of its run, so nothing reaches into the future: "backwards" plays the moment just before, in reverse.
- **2 frames of delay** (0.05 ms), on the warped and the straight sound alike.

## Will it work on my MPC?

| | |
|---|---|
| **Tested on** | MPC One (MPC OS 3.10.1) |
| **Should work on** | Any standalone MPC on **MPC OS 3.10.1 or later** (Akai's plugin folder), Gen1 (32-bit) and Gen2 (64-bit). Also Gen1 MPCs with the community plugin set-up on MPC OS 3.x |
| **Not supported** | **MPC OS 2.x** |

If you try it, please [tell me how it went](../../issues/new/choose).

## Install

**👉 [Step-by-step install guide](INSTALL-GUIDE.md)**

The short version, on **MPC OS 3.10.1 or later**:
1. Download `PotentialWarp-<version>-akai-vst2.zip` from [**Releases**](../../releases) and unzip it on your computer.
2. On the MPC, turn on **Preferences → Enhancements (Beta) → Enable Enhancements** and **External Plugin Support**.
3. Copy the folder **PotentialBreaks - VST - PotentialWarp** into a folder called **VST2 Plugins** at the top of a USB stick, SD card or the internal drive, and restart.
4. It's in **External Plugins**, as an effect.

Stuck? The [**FAQ**](FAQ.md) covers the common problems.

## Ideas, bugs and criticism: all welcome

I make these for myself first, and I want them to be good for everyone else too, so **please tell me what you think**, including what you don't like.

- 💡 [Suggest a feature](../../issues/new?template=idea.yml)
- 🐛 [Report a bug](../../issues/new?template=bug.yml)

You don't need to be technical. Short messages are great. See [CONTRIBUTING.md](CONTRIBUTING.md) for more, including how to send code.

## Licence

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md). Gross Beat is a trademark of Image-Line; PotentialWarp is not affiliated with it.
