#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CSI_EVENTS 8
typedef struct {
    uint32_t events, valid_ms, moving_ms, peak, last;
    uint32_t times[CSI_EVENTS];
    uint8_t head, count;
    bool initialized, valid, moving;
} CsiSession;
/* Durations integrate only adjacent valid samples <2s apart. No stale occupancy. */
void csi_session_feed(CsiSession* s, uint32_t now, uint32_t state, uint32_t score, bool valid);
