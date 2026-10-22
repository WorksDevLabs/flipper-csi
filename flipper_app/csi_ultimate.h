#pragma once
#include "csi_protocol.h"
#define CSI_WF_BINS 47
#define CSI_WF_ROWS 20
/* Persistent configuration; armed state deliberately never persists. */
typedef struct {
    uint8_t delay, hold, release, repeat, filter;
} CsiUltimatePrefs;
typedef struct {
    bool armed, active, candidate, clearing, notified;
    uint32_t armed_at, since, clear_since, last_alert;
} CsiAlarm;
bool csi_ultimate_valid(const CsiUltimatePrefs* p);
void csi_ultimate_default(CsiUltimatePrefs* p);
bool csi_alarm_step(CsiAlarm* a, const CsiUltimatePrefs* p, uint32_t now, bool fresh, bool moving);
bool csi_wave_parse(const char* text, uint8_t bins[CSI_WF_BINS]);
#define CSI_ULTIMATE_BYTES 11
void csi_ultimate_encode(const CsiUltimatePrefs* p, uint8_t bytes[CSI_ULTIMATE_BYTES]);
bool csi_ultimate_decode(CsiUltimatePrefs* p, const uint8_t* bytes, size_t size);

bool csi_perf_parse(const char* text, uint32_t values[6]);

/* Suppress reports until user calibration was acknowledged; exclude old frames. */
bool csi_measurement_ready(bool started, bool awaiting, uint32_t age, uint32_t elapsed);
