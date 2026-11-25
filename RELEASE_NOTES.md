# CSI Radar Ultimate v2.0.0 — E11 / API 87.1

Final feature version for the owner's E11-patched Momentum and official ESP32-S2
Wi-Fi Devboard. Includes the accepted compact UI, manual calibration start,
readable score graph, backlight hold, real carrier waterfall, Classic/Robust,
profiles, saved mute/language, SD sessions/A-B, alarm and offline analysis tools.

## Installation

- Existing protocol-3 2.0 users: replace only `csi_radar.fap` in `/ext/apps/GPIO/`.
- New / 1.0 / 1.5 users: use the full ZIP and update ESP and FAP together; keep E11.
- Final ESP rebuild corrects its startup version text only; no algorithm/protocol
  change versus the working 2.0 ESP. Reflashing an existing 2.0 is optional.
- Check the actual COM port; it may change after flashing. Verify SHA256SUMS.txt.

## Validation scope

Local FAP/ESP builds, C regressions, 8 Python tests and ASan/UBSan are recorded.
Owner reports motion/still, vibration, waterfall and manual/automatic/filter
operation and accepted this final UI. No numeric room accuracy or 8h/3-placement
result is invented. Exhaustive alarm/A-B, SD faults, migration and endurance remain
open in docs/DEVICE_TESTS_TR.md. CI is not used as evidence without an actual run.

## Downloads

- `csi_radar.fap`: exact final FAP.
- `flipper-csi-v2.0.0.zip`: full source/firmware/guides and internal hashes.
- `SHA256SUMS.txt`: hashes of the downloadable FAP and ZIP.

[English guide](https://github.com/WorksDevLabs/flipper-csi/blob/main/docs/INSTALL_EN.md) ·
[Türkçe rehber](https://github.com/WorksDevLabs/flipper-csi/blob/main/docs/INSTALL_TR.md)
