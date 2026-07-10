# Build & flash

## Option A — GitHub Actions (recommended)
1. Create an empty repo on GitHub (e.g. `zmk-config`).
2. Push these files:
   ```
   git init && git add -A && git commit -m "corne config"
   git branch -M main
   git remote add origin https://github.com/<you>/zmk-config.git
   git push -u origin main
   ```
3. The included workflow (`.github/workflows/build.yml`) builds automatically.
   Open the run's **Artifacts** and download `firmware.zip` — it contains
   `corne_left-nice_nano_v2-zmk.uf2` and `corne_right-nice_nano_v2-zmk.uf2`.

## Option B — local build
```
pip install west
west init -l config && west update && west zephyr-export
pip install -r zephyr/scripts/requirements.txt
west build -s zmk/app -b nice_nano_v2 -- -DSHIELD=corne_left  -DZMK_CONFIG="$PWD/config" -DSNIPPET=studio-rpc-usb-uart
west build -s zmk/app -b nice_nano_v2 -- -DSHIELD=corne_right -DZMK_CONFIG="$PWD/config"
```
(requires the Zephyr SDK / arm-zephyr-eabi toolchain)

## Flash (Windows)
1. Plug in the **left** half via USB.
2. Double-tap the reset button — a drive named **NICENANO** appears.
3. Drag `corne_left-...uf2` onto it. It reboots automatically.
4. Repeat with the **right** half and `corne_right-...uf2`.

After both halves are on Studio-enabled firmware, you can tweak keys live at
https://studio.zmk.dev over USB — no reflash needed for key moves.
