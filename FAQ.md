# FAQ and troubleshooting

Can't find your answer? [Ask in an issue](../../issues/new/choose). There are no silly questions.

## Installing

**The plugin isn't in my plugin list.**
- **Restart:** the list only updates after the MPC restarts.
- **The wrong list:** PotentialWarp is an **effect**, so look in the **insert effects** (under **External Plugins** on MPC OS 3.10.1+), not the instruments.
- **MPC OS 3.10.1+:** check **Preferences → Enhancements (Beta)**: both **Enable Enhancements** and **External Plugin Support** must be ticked, and the folder must sit directly inside **VST2 Plugins** at the top of the drive.

**It shows up twice.**
You installed it both ways. Keep one: on MPC OS 3.10.1+ the **VST2 Plugins** copy is the one to keep.

**Which file do I download?**
On MPC OS 3.10.1 or later: the one ending **`-akai-vst2.zip`**. On older MPC OS 3.x with the community set-up: the one ending **`-mpc-armv7.zip`**. The "Source code" links are for developers.

## PotentialWarp

**It doesn't line up with my beat.**
Press **Play**. The patterns follow the sequence's position while it plays. When it's stopped, PotentialWarp keeps its own time, so it won't be lined up to your bars.

**There's no sound for a moment when I add it or press Play.**
Some moves play what PotentialWarp heard just before. Right after you add it, it hasn't heard anything yet. It fills up within a beat.

**Jumps sound clicky.** Set **FADE** to **SOFT**. **TIGHT** is snappier on purpose.

**It's too much.** Turn **MIX** down to blend in the original sound, or automate **WARP** so it only comes in for fills.

**Can I make my own patterns?**
Yes, from 1.1.0. On the **EDIT** page, tap a step and then what it does, or press **ROLL** for a new pattern and tidy it up. Every pad's pattern is saved with your project, and **RESET PAD** brings the pad's own pattern back.

**Can it change pattern by itself?**
Yes: on the **CHAIN** page, pick up to 8 patterns in order and turn CHAIN on. Each plays for one LENGTH, then the next.

## Everything else

**Is it free? Can I use it in my music?**
Yes and yes. It's MIT-licensed, so use it however you like, commercially too. Credit is appreciated but not required.

**Can I suggest an idea or report a bug?**
Please do: [ideas](../../issues/new?template=idea.yml) · [bugs](../../issues/new?template=bug.yml).
