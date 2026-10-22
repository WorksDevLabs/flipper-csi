# Ultimate 2.0 implementation status

All mandatory software feature groups are implemented. Physical release gates
and capture-based performance validation remain open; this is the final feature release preparation.

| Mandatory group | Implementation | Remaining evidence |
|---|---|---|
| Dashboard, trend, carrier waterfall, event timeline, quality | Main graph, 47-bin real LLTF waterfall (4Hz/20 rows), event history, diagnostics | Physical display and real CSI |
| Robust filtering and temporal features | Optional carrier median/MAD winsorisation + median-of-three + EMA/hysteresis; Classic retained | Same labeled real captures and room placements; no accuracy improvement claimed |
| Measurement confidence | Quality independent of motion, stale rejection, explicit observed filter mode | Router/CSI dropout tests |
| Session center and A/B | Background SD catalog, bounded file parsing, on-device A/B counts/durations/rejects | Physical SD and long captures |
| PC export/replay/graphs | SD CSV report + Classic/Robust RAW comparison + real-amplitude HTML waterfall | Real labeled files required for accuracy claims |
| Alarm | Explicit arm/disarm, grace/hold/release/repeat, silent mode, user test, stale/zero-quality suppression, awake lifecycle | Physical timing/vibration/LED and long run |
| Product/UI and setup | Structured help/about, clipping, wizard, native icon, bilingual base UI, bounded queues, live resource screen | Ergonomics and actual resource values |
| Package/migration | Matched protocol-3 FAP/ESP, unchanged partitions, 1.5 settings retained, separate CRC16/backup Ultimate settings, hashes | Real power-loss/migration tests |

Optional web dashboard and second-board transmitter are excluded with reasons
in ULTIMATE_2_0_TR.md. No extra release version is planned. Three release plan
remains 1.0 Core → 1.5 Pro → 2.0 Ultimate; tag/Release publication is tracked separately.

SD CSV 2.0 şeması 2, son sütun `filter` (0 Classic / 1 Robust). Cihaz ve PC
karşılaştırması eski şema 1 kayıtlarını da okur; F? / legacy filtresi bilinmeyen
veya karışık oturumdur. Şifre/SSID kayda yazılmaz.
