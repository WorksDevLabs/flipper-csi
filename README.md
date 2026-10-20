# CSI Radar

**Wi-Fi CSI motion detection for Flipper Zero and the official ESP32-S2 Wi-Fi Developer Board.**

![CSI Radar launcher icon](flipper_app/csi_radar_10px.png)

**v1.0.0 — scoped stable release for the tested E11 / API 87.1 setup.**
Motion detection and vibration were physically checked by the device owner.
This is a motion detector: it does not measure distance, direction, people count,
heart rate or stationary human presence.

## Features
- Live motion score graph, visible threshold, STILL / MOVING states.
- Calibration, vibration/LED alerts, mute, packet rate and link diagnostics.
- Separate Wi-Fi, CSI and UART health states; stale scores do not remain valid.
- CRC16-protected serial protocol and bounded malformed-input handling.
- Wi-Fi configuration via Flipper keyboard or USB helper; saved ESP32 settings.
- USB raw CSI capture tool for tuning and reproducible experiments.
- Native 10×10 monochrome application icon.

## Compatibility and evidence
| Item | Scope |
| --- | --- |
| Physical setup checked | Official Flipper Wi-Fi Devboard, ESP32-S2, E11-patched Momentum d3f89dfe / API 87.1 |
| App build | Momentum mntm-012 SDK, F7 / API 87.1; FAP validation passed |
| ESP32 build | ESP-IDF v5.5, esp32s2, 4 MB flash, USB CDC |
| Owner-observed PASS | ESP32 USB flash/verification, Wi-Fi via USB helper, calibration, MOVING/STILL transitions, vibration |
| Host tests | Synthetic motion tests, regression tests, ASan/UBSan (leak detection disabled in restricted environment), malformed-stream stress |
| Not yet physically checked | Long idle false-alarm rate, router reconnect, settings persistence after reboot, UART handoff, other firmware versions/boards |

The new launcher icon and version metadata do not change sensing behavior.
The icon-bearing FAP still needs an on-device launch check. No universal firmware
compatibility or independently calibrated detection accuracy is claimed.

## Install or update
1. Copy `ready/csi_radar.fap` to `/ext/apps/GPIO/csi_radar.fap` using qFlipper.
2. Existing CSI Radar 0.2 users: **only replace the FAP**. Keep the working E11
   firmware and ESP32 firmware; Wi-Fi credentials and threshold remain on ESP32.
3. First install: flash the three ESP32 binaries using [the Turkish guide](docs/INSTALL_TR.md).
4. Open Apps → GPIO → CSI Radar. For SSIDs awkward to enter on the Flipper,
   use `python tools/csi_usb.py COMx setup`. Passwords are prompted, not shell arguments.

| Button | Action |
| --- | --- |
| OK | Calibrate with 100 valid packets |
| Hold OK | Configure Wi-Fi |
| Up / Down | Adjust threshold |
| Left | Toggle details / graph |
| Right | Toggle mute |
| Hold Right | Save ESP32 threshold |
| Back | Exit / cancel Wi-Fi editor |

Use a 2.4 GHz WPA2-compatible network; start with 20 MHz channel width.
Wi-Fi passwords are stored unencrypted in ESP32 NVS and are visible during
Flipper keyboard entry. No SSID/password is included in this repository or binaries.

## Build
Install `ufbt==0.2.6`, then in `flipper_app`:

```sh
ufbt update --hw-target f7 --url https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-012/flipper-z-f7-sdk-mntm-012.zip
ufbt
```

ESP-IDF v5.5, in `esp32`:

```sh
idf.py set-target esp32s2
idf.py build
```

Host regression tests: `tools/run_tests.sh`.
Protocol: [PROTOCOL.md](PROTOCOL.md). Release notes: [CHANGELOG.md](CHANGELOG.md).

## License
MIT. Maintained by SauronLAB / Sehmus. Use only in spaces/networks where you
have permission to perform motion sensing. This is an educational instrument.
