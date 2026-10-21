# Local build evidence

Target: E11 / F7 API 87.1, official ESP32-S2 Wi-Fi Devboard.

- ufbt 0.2.6 + pinned Momentum mntm-012 SDK: compile and APPCHK pass.
- Flipper source formatter/linter: pass.
- ESP-IDF v5.5, esp32s2, 4 MB flash, USB CDC: build pass.
- Existing synthetic movement/regression suite: pass.
- Pro suite: settings CRC/custom profile, status/AP field bounds, effective
  stale ages, auto-calibration, noisy/gap/low-rate rejection: pass.
- 200,000-byte malformed-stream stress and 10,000 malformed settings: pass.
- Python RAW replay: bounds/gain, motion/recovery, non-destructive output: pass.
- ASan/UBSan pass; leak detection disabled for restricted /proc environment.

Source and portable behavior checks do not establish real RF performance,
physical UI layout/usability, SD fault recovery or device persistence. Those
checks and real stationary/moving captures remain pending. No Actions run is claimed.
