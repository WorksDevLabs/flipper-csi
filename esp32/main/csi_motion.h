#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CSI_MAX_SUBCARRIERS 64
#define CSI_STALE_MS        2000U

typedef enum {
    CSI_STATE_WARMUP,
    CSI_STATE_STILL,
    CSI_STATE_MOVING,
    CSI_STATE_CALIBRATING,
    CSI_STATE_NO_CSI,
    CSI_STATE_WIFI_DOWN,
    CSI_STATE_SETUP
} csi_state_t;

typedef struct {
    float baseline[CSI_MAX_SUBCARRIERS];
    float score, threshold;
    float deviation[64], last_raw[3], raw_score, mad;
    uint8_t spectrum[47], filter, raw_count, raw_head;
    float cal_m2[CSI_MAX_SUBCARRIERS], noise;
    uint8_t quality, cal_error;
    bool automatic, calibrated;
    uint32_t cal_start_ms;
    uint32_t warmup_left, cal_total, cal_left, last_ms, above_ms, below_ms;
    uint32_t accepted, rejected, synthetic_ms;
    bool primed, calibrating, moving, above_active, below_active;
} csi_motion_t;

void csi_motion_init(csi_motion_t* m);
/* Preserve threshold, discard the previous room/link baseline. */
void csi_motion_reset(csi_motion_t* m);
void csi_motion_calibrate(csi_motion_t* m, uint32_t packets);
/* Fixed 20 MHz LLTF layout only, exactly 128 bytes; skip is 0 or 4.
 * First two bins are ALWAYS excluded, preserving indices when skip changes.
 * Returns false for malformed, zero-energy or excessively clipped samples. */
bool csi_motion_feed(
    csi_motion_t* m,
    const int8_t* buf,
    uint16_t len,
    uint16_t skip,
    uint32_t now_ms);
/* Compatibility helper for host fixtures at 20 Hz. */
float csi_motion_update(csi_motion_t* m, const int8_t* buf, uint16_t len, uint16_t skip);
csi_state_t csi_motion_state(const csi_motion_t* m);
uint32_t csi_motion_progress(const csi_motion_t* m);
