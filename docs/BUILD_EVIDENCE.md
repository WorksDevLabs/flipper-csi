# Ultimate 2.0 — build and validation evidence

Target: owner E11-patched Momentum / F7 API 87.1 and official ESP32-S2 Devboard.
This file separates local build checks from owner-reported device behavior.

## Final artifact identity

`ready/csi_radar.fap` is the final 56,108-byte screen-on build. SHA256:
`80deeb97f33624e1f97fff751ff469137bf0ac8d8f299a715f18040a58fe442e`.
It matches `flipper_app/dist/csi_radar.fap` byte-for-byte. The old 47,044-byte
candidate is not shipped. Packaging checks these identities before writing hashes.
The final ESP app is rebuilt from this source after correcting startup version
text to Ultimate 2.0 / protocol 3. Sensing code/protocol is unchanged from the
working 2.0 ESP; existing protocol-3 boards do not require a reflash for UI updates.

## Actual tools

- Installed ufbt: 0.2.6 (`pip show ufbt`).
- SDK source: pinned Momentum `mntm-012` download URL, recorded in
  `/root/.ufbt/current/ufbt_state.json` during the build. That state's version
  string says `unknown`; the release is identified by the URL, not that string.
- Actual FAP APPCHK result: target F7, API 87.1. FAP lint passes.
- ESP-IDF v5.5, commit `8c750b088c7cd857d079c0eeb495da199b359461`;
  esp32s2, 4 MB flash, USB CDC. Exact tool/config/binary values are captured in
  `release-inputs.json`. This is the builder's SDK, not a claim about the
  owner's installed Windows ESP-IDF 5.5.5.

## Checks

- Six C executables: motion, regression, Pro, session, Ultimate and graph scale.
- CRC/bounds, fixed bins, calibration, gain, stale/wrap, persistence encoding,
  AP/status, alarm timing, CSV/schema, waterfall/performance parsers and graph mapping.
- Previous 200,000-byte malformed stream and 10,000 malformed Ultimate inputs.
- Eight Python tests: replay/filter/recovery, CSV 1/2, recorded-bin waterfall,
  output safety, session durations and report escaping.
- ASan/UBSan pass with `ASAN_OPTIONS=detect_leaks=0`; leak detection not claimed.
- FAP compile/APPCHK/lint and ESP32-S2 build/partition-size checks.
- ZIP integrity and every internal file hash; downloadable FAP/ZIP hashes.

GitHub Actions is not used as evidence: no successful final workflow run was
obtained here. `.github/workflows/ci.yml` is manual build/test only, with no
publishing job. The owner previously reported an Actions account restriction.
Local logs/results provide the stated evidence; a future CI run must be recorded
before adding a passing badge or CI-success claim.

## Device scope

See OWNER_TEST_REPORT.md for the owner's reports and DEVICE_TESTS_TR.md for
open quantitative/physical checks. No raw labeled room captures, accuracy
percentage, three-placement success or eight-hour final-2.0 soak is invented.
Final ESP startup-text rebuild has not received a new physical test report.

## Bounded resources and runtime diagnostics

ESP samples16/commands8; 20Hz processing, waveform <=4Hz. UART115200 8N1.
Flipper RX2048 bytes, replies16, SD queue32, event ring8, bounded catalog12 files
and <=512 directory entries, <=256KiB+512/file. Main stack12KiB, RX3KiB, SD4KiB.
Waterfall ring47×20; newest16 rows shown in the final button layout.
The system screen reports F7 heap/min, ESP heap/queue peak, DSP maximum and
UART/UI values. These are instrumentation, not measurements acquired here.
