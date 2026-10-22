# CSI Radar Ultimate

**Wi-Fi CSI motion monitoring for Flipper Zero.**  
Built for E11-patched Momentum · API 87.1 · Official ESP32-S2 Wi-Fi Devboard

[**Download v2.0.0**](https://github.com/sehmusadsoy-ui/flipper-csi/releases/tag/v2.0.0) · [Türkçe rehber](docs/INSTALL_TR.md) · [English guide](docs/INSTALL_EN.md) · [Roadmap](ROADMAP.md)

Monitor changes in Wi-Fi channel state information on your Flipper: a live motion
score, carrier waterfall and session history, with manual calibration and optional
vibration alerts. Ultimate is the final feature milestone of the Core → Pro →
Ultimate plan.

## On the device

Actual owner-provided v2.0 device captures. Click an image to open it at full size.

| Ready / manual calibration | Live motion score |
| :---: | :---: |
| ![Ready screen](https://raw.githubusercontent.com/sehmusadsoy-ui/flipper-csi/main/docs/screenshots/owner-ready.png) | ![Live score graph](https://raw.githubusercontent.com/sehmusadsoy-ui/flipper-csi/main/docs/screenshots/owner-graph.png) |
| **Carrier waterfall** | **Connection diagnostics** |
| ![Carrier waterfall](https://raw.githubusercontent.com/sehmusadsoy-ui/flipper-csi/main/docs/screenshots/owner-waterfall.png) | ![Connection diagnostics](https://raw.githubusercontent.com/sehmusadsoy-ui/flipper-csi/main/docs/screenshots/owner-connection.png) |
| **Session summary** | **Event history** |
| ![Session summary](https://raw.githubusercontent.com/sehmusadsoy-ui/flipper-csi/main/docs/screenshots/owner-session.png) | ![Event history](https://raw.githubusercontent.com/sehmusadsoy-ui/flipper-csi/main/docs/screenshots/owner-history.png) |

## Features

| Area | Included in 2.0 |
| --- | --- |
| Live monitoring | Motion/still states, framed 10-second score graph, adjustable threshold and 47-carrier waterfall |
| Calibration & filtering | Explicit manual start, Classic/Robust filters, manual or noise-based automatic threshold |
| Everyday use | Wi-Fi setup with masked password, profiles, saved mute/language/settings and backlight held on while open |
| Alerts | Vibration/LED, mute and explicitly armed alarm with grace/hold/release/repeat settings |
| Recording | Queued SD CSV sessions, session selection, A/B summaries and event history |
| PC tools | Offline HTML reports, raw-capture waterfall and filter comparison |

## Get started

1. Download the [full v2.0.0 package](https://github.com/sehmusadsoy-ui/flipper-csi/releases/download/v2.0.0/flipper-csi-v2.0.0.zip) and [SHA256 checksums](https://github.com/sehmusadsoy-ui/flipper-csi/releases/download/v2.0.0/SHA256SUMS.txt).
2. Keep your E11 system firmware. The supplied FAP targets **API 87.1**.
3. First install or upgrade from 1.0/1.5: flash the three ESP binaries from `ready/` using the [installation guide](docs/INSTALL_EN.md).
4. Copy [`csi_radar.fap`](https://github.com/sehmusadsoy-ui/flipper-csi/releases/download/v2.0.0/csi_radar.fap) to `/ext/apps/GPIO/csi_radar.fap` using qFlipper while the application is closed.
5. Open CSI Radar, place the device still, then press **OK** to calibrate.

Already running protocol-3 2.0? The FAP can be updated alone. The final ESP rebuild
only corrects startup version text; reflashing an existing working 2.0 ESP is optional.

**Türkçe:** [Kurulum, Wi-Fi ayarı, menüler ve kullanım](docs/INSTALL_TR.md).

## Compatibility & validation

The supplied build is scoped to **E11-patched Momentum / API 87.1** and the
**official ESP32-S2 Wi-Fi Devboard**. Other firmware versions or boards need their
own build and validation.

Local FAP/ESP builds, C regressions, eight Python tests and ASan/UBSan are recorded
in [build evidence](docs/BUILD_EVIDENCE.md). The owner reports working motion/still
transitions, vibration, waterfall and manual/automatic/Classic/Robust operation.

Detection depends on placement, traffic and the radio environment. Three-placement
accuracy, an eight-hour soak and exhaustive alarm/SD-fault/migration checks remain
open in the [device checklist](docs/DEVICE_TESTS_TR.md). No accuracy percentage is
claimed. The score does not measure identity, distance, direction, people count,
static presence or vital signs.

## Documentation & development

- [English installation and usage](docs/INSTALL_EN.md) · [Türkçe kurulum ve kullanım](docs/INSTALL_TR.md)
- [Protocol 3](PROTOCOL.md) · [Changelog](CHANGELOG.md) · [Roadmap](ROADMAP.md)
- [Build evidence](docs/BUILD_EVIDENCE.md) · [Owner test report](docs/OWNER_TEST_REPORT.md) · [Device checklist](docs/DEVICE_TESTS_TR.md)

Run `sh tools/run_tests.sh`. Recorded sanitizer configuration:
`SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 sh tools/run_tests.sh`.
FAP: ufbt 0.2.6 + Momentum mntm-012 SDK. ESP: ESP-IDF v5.5, target `esp32s2`.

[MIT License](LICENSE) · SauronLAB / Sehmus
