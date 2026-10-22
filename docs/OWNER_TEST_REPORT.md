# Owner-reported device evidence — 2026-10-22

Hardware: official ESP32-S2 Wi-Fi Devboard; owner E11-patched Momentum/API87.1.
This record is a paraphrase of reports in the development conversation, not a
new instrumented test run.

- 1.5 checklist: owner said all PASS; individual raw numbers/captures not supplied.
- 2.0: installed ESP and FAP; motion/still observed. Vibration works after mute OFF.
- Live waterfall scrolls and responds to movement; owner observed pattern changes.
- Classic/Robust and manual/automatic threshold detect movement. Initial AUTO
  concern was withdrawn after the owner repeated operation.
- Owner requested UI iterations and accepted the final screen-on/graph version
  as the final feature release.
- Owner reports independently running 8 Python tests, C tests, ASan/UBSan and
  hash checks on a package; this is distinct from our local runs.

Not established by these reports: a numerical room accuracy rate, 3 labeled
placements, exhaustive final alarm/A-B/SD-fault validation, 8h soak or complete
final-build settings/power-loss migration. Keep these items explicit, not PASS.
A new ESP startup-text rebuild has no new physical test report yet; the working
protocol-3 ESP is compatible and need not be replaced for FAP updates.
