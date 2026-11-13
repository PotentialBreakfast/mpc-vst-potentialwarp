# Install guide (step by step)

There are two ways in. Pick the one that matches your MPC.

- **MPC OS 3.10.1 or later** (any standalone MPC, Gen1 or Gen2) → [**Way 1: Akai's plugin folder**](#way-1-akais-plugin-folder-mpc-os-3101-and-later). No hacking, nothing to type on the MPC.
- **Older MPC OS 3.x on a Gen1 MPC** with the community plugin set-up from [Open MPC](https://open-mpc.github.io/) → [**Way 2**](#way-2-the-community-set-up-older-mpc-os-3x).

Install it one way only, or it shows up twice.

---

## Way 1: Akai's plugin folder (MPC OS 3.10.1 and later)

**Download:** `PotentialWarp-<version>-akai-vst2.zip` from [**Releases**](../../releases).

1. **Turn plugins on (once).** On the MPC: **Preferences → Enhancements (Beta)**, tick **Enable Enhancements** and **External Plugin Support**.
2. **Unzip** the download on your computer. Inside is a folder called **PotentialBreaks - VST - PotentialWarp**.
3. **Make the plugin folder (once).** At the very top of a USB stick, SD card or the MPC's internal drive, make a folder called exactly **VST2 Plugins**.
4. **Copy** the whole **PotentialBreaks - VST - PotentialWarp** folder into **VST2 Plugins**. Copy the folder, not just the files in it.
5. **Restart** the MPC (with the stick plugged in, if you used one).
6. **Find it:** add an insert effect and look under **External Plugins**.

Keep the stick in the MPC while you use the plugin, because it loads from there. The drive must not be read-only: the LOOK switch saves your choice inside the plugin's folder.

---

## Way 2: the community set-up (older MPC OS 3.x)

**Download:** `PotentialWarp-<version>-mpc-armv7.zip` from [**Releases**](../../releases). **Don't unzip it.**

**The easy way:** the free **MPC Plugin Installer** from [Open MPC](https://open-mpc.github.io/). Connect to your MPC, drop the zip in, tick it, choose a drive and press **Install**. Save your project first, because installing restarts the MPC app.

**Over SSH** (for people comfortable with a command line), from the folder the zip is in:
```
scp PotentialWarp-1.0.0-mpc-armv7.zip root@<your-mpc-ip>:/tmp/
ssh root@<your-mpc-ip>
cd /tmp && unzip -o PotentialWarp-1.0.0-mpc-armv7.zip
sh PotentialWarp-1.0.0/install.sh -t /media/<your-drive>/Synths
```
Unzip on the MPC, not on Windows (Windows loses permissions the installer needs). Remove it later with `sh /tmp/PotentialWarp-1.0.0/uninstall.sh`.

---

## Using it

It's an **effect**: open the **insert effects** on a track, program, pad or bus and choose **PotentialWarp**. Press **Play**, then tap a pattern on its page.

Something not right? See the [**FAQ**](FAQ.md).
