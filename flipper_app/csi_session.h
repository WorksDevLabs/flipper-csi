#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CSI_EVENTS 8
typedef struct {
    uint32_t events, valid_ms, moving_ms, peak, last;
    uint32_t times[CSI_EVENTS];
    uint32_t durations[CSI_EVENTS]; /* continuous moving time of each event, ms */
    uint8_t head, count;
    bool initialized, valid, moving;
} CsiSession;
/* Durations integrate only adjacent valid samples <2s apart. No stale occupancy. */
void csi_session_feed(CsiSession* s, uint32_t now, uint32_t state, uint32_t score, bool valid);

/* 2.1: session survives app restarts. Event times are stored as RTC seconds so
 * they stay correct after a Flipper reboot; durations and totals are copied.
 * Layout: "SES", events, valid_ms, moving_ms, peak (u32 LE), count (u8),
 * 8 x (rtc_seconds, duration_ms) oldest first, 3 zero bytes, CRC16 LE. */
#define CSI_SESSION_BYTES 90
void csi_session_encode(
    const CsiSession* s,
    uint32_t now_ms,
    uint32_t now_ts,
    uint8_t out[CSI_SESSION_BYTES]);
/* Restored sessions are never "continuous" with new samples. */
bool csi_session_decode(
    CsiSession* s,
    const uint8_t* bytes,
    size_t size,
    uint32_t now_ms,
    uint32_t now_ts);
