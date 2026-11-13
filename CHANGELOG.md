# Changelog

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
