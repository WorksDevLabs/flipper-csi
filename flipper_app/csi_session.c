#include "csi_session.h"
#include <limits.h>
#include <string.h>
#include "csi_protocol.h"
static uint32_t add(uint32_t a, uint32_t b) {
    return UINT32_MAX - a < b ? UINT32_MAX : a + b;
}
void csi_session_feed(CsiSession* s, uint32_t now, uint32_t state, uint32_t score, bool valid) {
    uint32_t dt = now - s->last;
    bool continuous = s->initialized && s->valid && valid && dt < 2000;
    bool moving = valid && state == 2;
    if(continuous) {
        s->valid_ms = add(s->valid_ms, dt);
        if(s->moving && moving) {
            s->moving_ms = add(s->moving_ms, dt);
            unsigned last = (s->head + CSI_EVENTS - 1) % CSI_EVENTS;
            s->durations[last] = add(s->durations[last], dt);
        }
    }
    if(valid && score > s->peak) s->peak = score;
    if(moving && !(continuous && s->moving)) {
        s->events = add(s->events, 1);
        s->times[s->head] = now;
        s->durations[s->head] = 0;
        s->head = (s->head + 1) % CSI_EVENTS;
        if(s->count < CSI_EVENTS) s->count++;
    }
    s->initialized = true;
    s->last = now;
    s->valid = valid;
    s->moving = moving;
}
static void put32(uint8_t* b, uint32_t v) {
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t* b) {
    return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
void csi_session_encode(
    const CsiSession* s,
    uint32_t now_ms,
    uint32_t now_ts,
    uint8_t out[CSI_SESSION_BYTES]) {
    memset(out, 0, CSI_SESSION_BYTES);
    memcpy(out, "SES", 4);
    put32(out + 4, s->events);
    put32(out + 8, s->valid_ms);
    put32(out + 12, s->moving_ms);
    put32(out + 16, s->peak);
    unsigned count = s->count > CSI_EVENTS ? CSI_EVENTS : s->count;
    out[20] = (uint8_t)count;
    for(unsigned k = 0; k < count; k++) {
        unsigned idx = (s->head + CSI_EVENTS - count + k) % CSI_EVENTS;
        uint32_t ago = (now_ms - s->times[idx]) / 1000;
        put32(out + 21 + k * 8, ago > now_ts ? 0 : now_ts - ago);
        put32(out + 25 + k * 8, s->durations[idx]);
    }
    uint16_t crc = csi_crc16((const char*)out, CSI_SESSION_BYTES - 2);
    out[CSI_SESSION_BYTES - 2] = (uint8_t)crc;
    out[CSI_SESSION_BYTES - 1] = (uint8_t)(crc >> 8);
}
bool csi_session_decode(
    CsiSession* s,
    const uint8_t* b,
    size_t size,
    uint32_t now_ms,
    uint32_t now_ts) {
    if(!b || size != CSI_SESSION_BYTES || memcmp(b, "SES", 4) || b[20] > CSI_EVENTS ||
       b[85] || b[86] || b[87])
        return false;
    if(csi_crc16((const char*)b, CSI_SESSION_BYTES - 2) !=
       (uint16_t)(b[CSI_SESSION_BYTES - 2] | (uint16_t)b[CSI_SESSION_BYTES - 1] << 8))
        return false;
    CsiSession out;
    memset(&out, 0, sizeof(out));
    out.events = get32(b + 4);
    out.valid_ms = get32(b + 8);
    out.moving_ms = get32(b + 12);
    out.peak = get32(b + 16);
    out.count = b[20];
    /* Keep ms offsets well inside uint32 wrap (49.7 days). */
    const uint32_t max_age = 40U * 24U * 3600U;
    for(unsigned k = 0; k < out.count; k++) {
        uint32_t ts = get32(b + 21 + k * 8);
        uint32_t ago = ts > now_ts ? 0 : now_ts - ts;
        if(ago > max_age) ago = max_age;
        out.times[k] = now_ms - ago * 1000U;
        out.durations[k] = get32(b + 25 + k * 8);
    }
    out.head = out.count % CSI_EVENTS;
    *s = out;
    return true;
}
