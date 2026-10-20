# CSI Radar v1.0.0 — E11 / API 87.1

Scoped stable build prepared for release for the owner-tested official ESP32-S2 Wi-Fi Devboard
and E11-patched Momentum setup. Adds a native launcher icon and release metadata;
sensing behavior and the ESP32 firmware remain unchanged from the tested 0.2 build.

Existing users: replace only `csi_radar.fap` in `/ext/apps/GPIO/`. No E11 or ESP32
reflash is required. The owner reports the icon-bearing application and threshold adjustment are working.

Included: FAP, ESP32 binaries, full source, MIT license, installation guide and SHA256.
Owner-observed: flash verification, Wi-Fi setup, calibration, motion/still transitions,
vibration. Long-duration false alarms, reconnect/persistence/handoff and other boards
remain unverified. Distance, direction and vital signs are not measured.

## Product roadmap

Exactly three releases: **1.0 Core → 1.5 Pro → 2.0 Ultimate**.
The detailed roadmap and acceptance gates are in [ROADMAP.md](ROADMAP.md).
1.5/2.0 features are planned, not included in this build.
See [device checks](docs/DEVICE_TESTS_TR.md) for the remaining physical tests.

This file is release-note content; its presence alone is not a published GitHub Release.
