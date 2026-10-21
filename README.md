![CSI Radar](docs/banner.svg)

# CSI Radar Pro 1.5 — device-test candidate

This is the **development branch for v1.5.0 Pro**, the second planned product
version after [v1.0.0 Core](https://github.com/sehmusadsoy-ui/flipper-csi/releases/tag/v1.0.0).
It is not a new stable release or an additional roadmap version. Keep the tested
1.0 release available. The next/final planned version is 2.0 Ultimate.

**Target:** the owner's E11 / API 87.1 Flipper and official ESP32-S2 Wi-Fi Devboard.
FAP built with the pinned Momentum mntm-012 SDK; ESP firmware built with ESP-IDF v5.5.
Both sides must be upgraded for Pro. The Flipper's E11 system firmware stays installed.

[Turkce kurulum ve test](docs/PRO_1_5_TR.md) · [Implementation and gates](docs/PRO_1_5_STATUS.md) · [Roadmap](ROADMAP.md)

## Implemented

- Wi-Fi scan/select, including SSIDs containing spaces; manual ASCII keyboard
  with a SPACE key, masked password and minimum-length validation. UTF-8 SSIDs
  selected from scans retain their bytes; unsupported glyphs display as `?`.
- Turkish (ASCII font-safe labels) / English help and version information.
- Quiet / Normal / Busy presets and a separate retained custom threshold.
- CRC-protected SD settings with a backup copy; persistent mute/language/profile.
- Explicit manual / automatic threshold mode. Auto estimates a threshold from
  the stationary calibration noise; it does not silently follow ongoing motion.
- Rejection of noisy, interrupted or too-slow calibration; quality/noise display.
- Firmware handshake, bounded command responses, reconnect reconfiguration and
  stale-age compensation for display, vibration and logs.
- Nonblocking SD queue and separate writer: RTC-stamped CSV sessions, state
  changes, 256 KiB file rotation and visible SD/queue errors. No credentials in CSV.
- Standard-Python RAW diagnostic replay, CSV output, per-capture JSON comparison.
- Existing USB setup/capture commands retained; USB `info` and `scan` added.

## Controls

| Control | Action |
| --- | --- |
| OK | Calibrate; keep the room still |
| Hold OK | Scan/select Wi-Fi |
| Up / Down | Change manual threshold (disables auto) |
| Left | Details / dashboard |
| Hold Left | Settings, profiles, SD logging, language, help and version |
| Right | Mute / unmute (saved on SD) |
| Hold Right | Save current ESP threshold/mode |
| Back | Exit / return / delete in keyboard |
| Keyboard: arrows, OK | Select character, append |
| Keyboard: Hold OK | Save SSID/password field |
| Keyboard: Hold Back | Cancel and clear password buffer |

## Auto mode and data quality

Auto requires a successful stationary calibration. Its provisional rule is
`clamp(4 * normalized_noise_std + 0.020, 0.040, 0.800)`. Noise >0.100, fewer
than 24 useful bins or an average calibration sample interval >200 ms rejects
calibration. A CSI gap >=2 seconds cancels it. These are engineering starting
parameters, not real-room accuracy guarantees. `AUTO ?` means calibration is
not valid; the last manual/configured threshold is the fallback.

`Q` is sample/link quality, not a probability that a person is present. No
range, direction, identity, stationary occupancy or vital-sign measurement.

## Installation

The test ZIP contains `ready/csi_radar.fap`, the three ESP32 binaries, source,
SHA256 and Turkish instructions. Upgrade ESP first, then the FAP. The NVS layout
and Wi-Fi key names are retained; do not use erase-flash. Back up the working
FAP and any existing SD settings/logs. Do not overwrite the v1.0.0 GitHub assets.
Only the documented E11/API target is intended; other builds need device checks.

## Build and verification

Flipper, in `flipper_app`:

```sh
python -m pip install ufbt==0.2.6
ufbt update --hw-target f7 --url https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-012/flipper-z-f7-sdk-mntm-012.zip
ufbt
```

ESP-IDF v5.5, in `esp32`: `idf.py set-target esp32s2`, then `idf.py build`.
Host: `ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 sh tools/run_tests.sh`.
Leak detection is disabled only for the restricted local /proc environment.
The manual-only workflow runs tests/builds and never publishes a release.

**No Pro device acceptance is claimed yet.** The 30-minute idle, 9/10 walk,
10 restart/reconnect and 2-hour soak gates in [ROADMAP.md](ROADMAP.md) are open.
Real stationary/moving captures are also still needed; see [captures](captures/README.md).

## License

MIT · SauronLAB / Sehmus.
