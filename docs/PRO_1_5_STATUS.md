> Historical Pro 1.5 implementation note. Current installation: INSTALL_TR.md / INSTALL_EN.md. Later owner reports: OWNER_TEST_REPORT.md.

# Pro 1.5 implementation and release gate

Status: development/device-test candidate, not tagged or published stable.
The existing v1.0.0 tag, assets and main branch are preserved.

| Roadmap item | Implementation | Evidence / remaining gate |
| --- | --- | --- |
| Wi-Fi scanning/select, spaces, hidden password | Implemented with bounded AP messages and ASCII grid | SDK build, parser/hex bounds tests; real scan/editor test pending |
| Bilingual help, score/threshold and version screen | Implemented | Local SDK build; physical layout/usability pending |
| Quiet/Normal/Busy/custom, persistent mute/language | Implemented; CRC+backup SD settings | Portable format tests; real persistence/SD faults pending |
| Quality-aware calibration and explicit auto/manual | Implemented; 100 samples, noise/rate/gap rejection | Synthetic noise/low-rate/gap tests; room captures and tuning pending |
| Reconnect, handshake, errors and UART lifecycle | Implemented; old ESP is detected, effective age includes transport delay | Protocol/freshness tests and builds; 10-cycle device checks pending |
| Nonblocking CSV sessions and rotation | Separate queued writer, 256 KiB rotation, explicit SD/queue errors | SDK build; actual SD/full/no-media tests pending |
| Diagnostic RAW replay/comparison | Standard Python, bounded parser, CSV+JSON | Synthetic fixture tests; real labeled stationary/moving data not supplied |
| E11 and other firmware | E11/API 87.1 target retained | No new physical firmware support claimed |

Local checks: FAP compile/APPCHK (F7, API 87.1), lint, ESP32-S2 IDF build,
original host tests, Pro settings/parser/calibration regressions and Python replay tests.
ASan/UBSan use detect_leaks=0 because of the restricted /proc environment.
No GitHub Actions execution or real hardware result is inferred from a successful build.

Open stable gates remain in ROADMAP.md: 30-minute idle/10-walk metrics, 10 restart/
reconnect cycles, persistence/handoff/storage fault checks and 2-hour soak. This
work implements the software; it does not close those hardware or real-capture gates.
