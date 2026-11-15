# Building PotentialWarp

You don't need to build it to use it: download the zip from [Releases](../../releases). This is for people who want to change it.

It's built with sd88me's [mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins) kit, and needs Linux or WSL, `gcc`, Python 3, `pip install ziglang pillow playwright` and `python -m playwright install chromium` (the skin is drawn by the kit's browser renderer). You don't need Docker.

```
git clone https://github.com/sd88me/mpc-vst-plugins
git clone https://github.com/PotentialBreakfast/mpc-vst-potentialwarp
cd mpc-vst-plugins && git apply ../mpc-vst-potentialwarp/shared/mpc-vst-plugins-label-fit.patch && cd ..
K=$PWD/mpc-vst-plugins; P=$PWD/mpc-vst-potentialwarp

# 1. the three looks' skins + params.h (drawn in Chromium from vst/skin/looks/; don't set SHADOW_TITLE_FONT)
mkdir -p $P/vst/build
gcc -O2 -I $K/tools/vendor/force-shadow/tools -o $P/vst/build/shadow_art $K/tools/shadow_art.c -lm
python3 $P/shared/looks/build_looks.py $P/vst $K

# 2. test on your computer (x86, with AddressSanitizer): the kit's host test, then the engine's own
bash $K/tools/test_port.sh $P/vst/vst.json
cd $P && gcc -fsanitize=address,undefined -g -O1 -std=gnu11 -DWARP_TEST -I src -I $K/wrapper tests/test_warp.c src/warp.c -lm -o vst/build/test_warp && vst/build/test_warp

# 3. build for the MPC (Gen1, 32-bit ARM)
cd $P && python3 -m ziglang cc -target arm-linux-gnueabihf.2.31 -mcpu=cortex_a9 -mfpu=neon -O2 -fPIC -shared \
  -fvisibility=hidden -std=gnu11 -ffast-math -I src -I vst/build -I $K/wrapper src/warp.c $K/wrapper/vst2_wrap.c \
  -lm -lpthread -Wl,--no-undefined -Wl,-s -o vst/build/potentialwarp.so

# 4. the installer zip
python3 $K/tools/release.py --so vst/build/potentialwarp.so --skin "vst/build/skin/PotentialBreaks - VST - PotentialWarp" \
  --entry vst/build/pluginlist-entry.xml --version X.Y.Z --id potentialwarp --repo PotentialBreakfast/mpc-vst-potentialwarp --license MIT -o dist
python3 $K/tools/catalog_check.py dist/*.zip --catalog

# 5. Akai's plugin folder format (MPC OS 3.10.1+): add a 64-bit build and convert with Akai's SDK
#    (https://github.com/inmusic-software/mpc-plugin-builder-sdk, tools/legacy_convert.py and validate.py)
python3 -m ziglang cc -target aarch64-linux-gnu.2.31 -mcpu=cortex_a55 -O2 -fPIC -shared -fvisibility=hidden -std=gnu11 \
  -ffast-math -I src -I vst/build -I $K/wrapper src/warp.c $K/wrapper/vst2_wrap.c -lm -lpthread -Wl,--no-undefined -Wl,-s \
  -o vst/build/potentialwarp-arm64.so
python3 <sdk>/tools/legacy_convert.py dist/PotentialWarp-X.Y.Z-mpc-armv7.zip --out akai
#    then copy potentialwarp-arm64.so into the converted folder, add "file64": "potentialwarp-arm64.so" to its plugin.json
#    and <uiversion>0.1.0.0</uiversion> to its version.xml, and run <sdk>/tools/validate.py on the folder.
#    Keep the .so files beside "Plugin Skins" (not in VST/): the LOOK switch finds the skins next to the .so.
```

**Notes**
- **Parameters are append-only.** `vst/params.lock` keeps saved projects working, so add new parameters at the end of the list.
- **The looks:** every Potential plugin ships three looks and a LOOK selector (parameter `theme`): Print Plate (the default), Night Rack, Colour Block and Vintage. `vst/layout.base.conf` is the only layout to edit: where each control sits, with no colours. `shared/looks/make_looks.py` turns it into `vst/layout.conf`, `layout.rack.conf` and `layout.block.conf`, adding each look's palette, stylesheet (`vst/skin/looks/print.css`, `rack.css`, `block.css`) and this plugin's own colour (`vst/skin/looks/plugin.json`). `shared/looks/build_looks.py` builds all three and merges them with the kit's `make_skinset.py`.
- **Switching looks:** MPC reads a skin only when a plugin loads, so the plugin copies the chosen look's `TUI.json` and Q-Link maps into place and the new look shows on the next load (`shared/looks/potential_theme.h`; the method is the kit's `docs/SKIN_VARIANTS.md`). `shared/looks/test_theme.c` tests it on the computer.
- **The patch:** `shared/mpc-vst-plugins-label-fit.patch` keeps button labels an even size (they shrink to fit, and only big buttons get bigger text) and makes group headings amber. It's made against mpc-vst-plugins `bf1054c`.
- **The pattern pictures:** `tools/make_pictures.py` draws `vst/pat/*.svg` from the patterns in `src/warp.c`; run it after changing a pattern. `tools/demo.c` renders a demo of all 16 patterns to a WAV.
- **The song position:** `HAS_SONG_POS` in `vst/vst.json` makes the wrapper pass MPC's position and tempo to `song_pos()` every block (the kit's `docs/MIDI_TIMING.md`).
- **The MANUAL page:** `vst/manual.txt` is the on-screen manual. A line `= NAME` starts a page (NAME is its button), `# text` is a heading, `- text` a bullet. `shared/looks/make_looks.py` turns it into the MANUAL tab in each look and stops the build if a page doesn't fit. The page names must match the options of the parameter `manual_page` in `vst/params.json`, in order. `shared/potential_manual.h` holds the page number in the engine; it isn't part of the saved state.
