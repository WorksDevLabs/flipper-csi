# CSI Radar Ultimate 2.0 — installation and operation

Target: owner E11-patched Momentum, F7 API 87.1; official ESP32-S2 Wi-Fi Devboard.
Keep E11. A new install or 1.0/1.5 upgrade needs both matching FAP and ESP binaries.
An existing protocol-3 2.0 ESP can keep its firmware: the final ESP rebuild only
corrects the startup version message, not sensing/protocol behavior.

Disconnect the board from Flipper. Connect USB, hold BOOT, tap RESET, release BOOT.
In ESP-IDF PowerShell, enter the extracted `ready` directory. Check Device Manager
for the bootloader COM port. COM8 below is an example; the port can change after reset.

```powershell
python -m esptool --chip esp32s2 --port COM8 --baud 460800 write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 flipper_csi_radar.bin
```

Do not erase flash. Wait for verified completion, reset, unplug USB and attach
the board to Flipper. With the app closed, copy `ready/csi_radar.fap` using
qFlipper to `/ext/apps/GPIO/csi_radar.fap`. Launch CSI Radar Ultimate.

Place the device still, press OK, remain still through calibration. Pre-start
measurements are ignored by the app. Left opens Menu; right changes Mode/page.
OK calibrates on the graph/waterfall/setup pages and opens Menu on info pages.
Hold right toggles vibration; up/down selects manual threshold. The screen stays
lit while open. Exit restores normal backlight timeout.

Menu groups: Alarm; Wi-Fi setup; Recording; Measurement; Analysis; Help/info.
Select language under Help/info. Mute/language/profiles persist on a working SD.
Save threshold to the ESP from Measurement settings. Do not assume persistence
if SD writes fail. Changing filters invalidates the old baseline.

Alarm is explicitly armed from its menu. Its page shows status/countdown; OK
arms/disarms and right toggles vibration. Timing changes disarm it. Returning to
the dashboard does not disarm; exiting does. No stale-data alarm is generated.

Stop recording before browsing SD sessions. Select A/B then compare summaries;
Mode opens identification/filter/rejected-row details. CSV schema 2 adds filter
0 classic/1 robust; legacy schema 1 is readable. Logs contain no SSID/password.
Use `tools/session_report.py` for offline CSV graphs and `tools/capture_report.py`
for RAW waterfall/classic-robust analysis. RAW <=10Hz is diagnostic, not exact
live-20Hz replay. See BUILD_EVIDENCE.md for actual verification limits.
