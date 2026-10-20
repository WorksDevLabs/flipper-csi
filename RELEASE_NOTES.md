# CSI Radar v1.0.0 — E11 / API 87.1

First scoped stable release for the owner-tested official ESP32-S2 Wi-Fi Devboard
and E11-patched Momentum setup. Adds a native launcher icon and release metadata;
sensing behavior and the ESP32 firmware remain unchanged from the tested 0.2 build.

Existing users: replace only `csi_radar.fap` in `/ext/apps/GPIO/`. No E11 or ESP32
reflash is required. Launch the new icon-bearing FAP once to confirm installation.

Included: FAP, ESP32 binaries, full source, MIT license, installation guide and SHA256.
Owner-observed: flash verification, Wi-Fi setup, calibration, motion/still transitions,
vibration. Long-duration false alarms, reconnect/persistence/handoff and other boards
remain unverified. Distance, direction and vital signs are not measured.
