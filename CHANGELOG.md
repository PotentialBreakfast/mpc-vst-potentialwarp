# Changelog

## 1.3.0 (2026-11)
- **MANUAL page.** The plugin's manual is now on the MPC's screen: tap **MANUAL**, then the page buttons along the top (what it does, the controls, how it works with the other PotentialSuite plugins, and good-to-know notes). The full PotentialSuite manual is on the [PotentialSuite page](https://github.com/PotentialBreakfast/potential-plugins).
- One parameter was added at the end of the list (Manual Page); saved projects open as before. Sound and controls are unchanged.

## 1.2.0 (2026-11)
- **Triplet repeats**: three new step moves, **Repeat 1/8T, 1/16T and 1/32T**, on the EDIT page, and ROLL uses them too (more on WILD).
- **SWING** (TIME panel): 50 % (straight) to 75 %, MPC style. The off 16th steps start later, so gates, chops and repeats swing with your beat.
- **4 BARS**: a fourth LENGTH, for slow builds and long tape stops.
- **STOP FILTER** (OUTPUT panel): a low-pass that closes as a Tape Stop runs down and opens as a Tape Start spins up, like a real deck. On for new copies; projects saved before 1.2.0 keep it off so they sound as they did.
- **PLAY** (WARP panel): **ALWAYS** (as before: the pattern runs with the song), **ONE SHOT** (tap a pattern, or play a pad, and it plays once from the next 16th, then the sound goes straight through) or **HOLD** (the pattern plays only while you hold a pad). MIDI pads A01-A16 now pick patterns 1-16 in every mode. HOLD needs pads or keys, because MPC's touchscreen doesn't tell a plugin when your finger lifts.
- The EDIT page's step choices are now three rows. Three parameters were added at the end of the list (Swing, Play, Stop Filter); saved projects open as before.

## 1.1.1 (2026-11)
- **CPU figure recorded.** Measured on an MPC One with the Open MPC bench: **PASS**, 3.7% of an audio block at the 99th percentile and 4.9% at worst, so it runs comfortably alongside a full project. It shows on the catalog card and in INSTALL.md.
- Nothing else changed: same sound, controls and saved projects as 1.1.0.

## 1.1.0 (2026-11)
- **Edit the patterns.** A new **EDIT** page: tap a step, then what it does (straight, silent, 1/2 or 1/4 speed, tape stop or start, backwards, spinback, repeat 1/8 to 1/64). Every pad can be changed, the pad's name shows EDITED, and **RESET PAD** puts its own pattern back.
- **Dice.** **ROLL** makes a new pattern for the pad, on the beat. **MILD**, **MEDIUM** or **WILD** says how far it goes.
- **Chains.** A new **CHAIN** page: up to 8 patterns played in turn, one LENGTH each, so the warp changes over a phrase by itself. The pads follow the chain.
- The strip shows the pattern step by step, so edited and rolled patterns show as they are.
- Edited patterns and the chain are saved with your project. Projects from 1.0.x open as before.

## 1.0.1 (2026-11)
- **Ready for MPC OS 3.10.1's plugin folders.** Every download now carries the `plugin.json` that MPC OS 3.10.1 looks for, so the plugin folder can be copied by hand straight into `VST2 Plugins`.
- **Gen2 MPCs in the Open MPC catalog.** A separate 64-bit download (`-mpc-aarch64.zip`), so the Plugin Manager can install it on Gen2 MPCs, and the catalog's 3.10.1 download (`-mpc-sdk.zip`).
- Built with the current Open MPC kit. Sound, controls and saved projects are unchanged.

## 1.0.0 (2026-11)
- First release: 16 tempo-locked patterns of straight, half and quarter speed, tape stop and start, repeats from 8ths to 64th rolls, backwards, spinback and silence; LENGTH (1/2, 1 or 2 bars), FADE, WARP on/off, MIX and OUTPUT; a pattern strip with a playhead; the three looks (Print Plate, Night Rack, Colour Block).
- Two downloads: Akai's plugin folder format for MPC OS 3.10.1+ (32-bit and 64-bit MPCs), and the community package for older MPC OS 3.x on Gen1.
