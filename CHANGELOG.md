# Changelog

## 2.1 — UI update (FAP only)

- Dashboard banner (STILL / MOTION, inverted while moving, flashing on alarm), score bar with threshold marker.
- Setup checklist (board, Wi-Fi, CSI, calibration) with next-step hint; calibration progress bar.
- Page numbers, consistent footer buttons, toast overlay; shorter labels that fit 128x64.
- Recent motion with RTC clock time and per-event duration; decoded Wi-Fi/calibration diagnostics.
- Session totals and last 8 events persisted to `session.bin` (+ `.bak`, CRC16); restored on start.
- Armed alarm sound (~2.5 s, optional), red LED and double vibration; unarmed motion notify is a separate setting.
- Internal ESP commands no longer show "Sending / confirmed" toasts.
- Settings flags: bit 2 motion notify off, bit 3 alarm sound off; 2.0 files decode unchanged.
- New host tests: event durations, session encode/decode (reboot, corruption, RTC rollback), settings bits.

## 2.0.0 — Ultimate

- Optional robust carrier median/MAD + temporal filtering; Classic retained.
- Real 47-carrier waterfall, event history, session/resource diagnostics.
- Background SD catalog and A/B summaries; CSV schema 2 includes filter identity.
- Explicit alarm grace/hold/release/repeat, silent mode, test and stale-data rejection.
- Offline HTML session/RAW graphs, waterfall and Classic/Robust comparison.
- Manual measurement start and fresh calibration on user request; pre-start data ignored.
- Small weighted headers, restored native buttons, framed 10-second graph,
  centered tables/history and improved graph-only scaling with visible Y range.
- Backlight remains on while open; normal timeout restored on exit.
- Protocol 3 handshake; final ESP startup text corrected to 2.0 / protocol 3.
- Canonical Turkish/English installation, device evidence scope and reproducible hashes.
- Target remains E11 / API 87.1 + official ESP32-S2. No generic compatibility
  or improved RF accuracy claim. Real-capture/long-duration acceptance remains open.

## 1.5.0 — Pro

- Wi-Fi scanning, masked SSID/password editor, profiles and persistent mute/language.
- Quality-aware calibration, optional automatic threshold and handshake recovery.
- Queued SD CSV recording/rotation and offline diagnostic RAW replay.
- Local builds/tests passed. Owner reported all checklist items PASS; raw numeric
  results were not supplied. Source is retained on dev/v1.5-pro.

## 1.0.0 — Core

- Native launcher icon, baseline motion graph, calibration, threshold and alerts.
- E11 / API 87.1 + official board owner-tested scope.
- Original v1.0.0 tag and published artifacts are preserved.

## 0.2 — development history

Initial device-tested movement/still detector, framed UART and USB Wi-Fi setup.
