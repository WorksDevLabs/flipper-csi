#include "csi_session.h"
#include <limits.h>
static uint32_t add(uint32_t a, uint32_t b) {
    return UINT32_MAX - a < b ? UINT32_MAX : a + b;
}
void csi_session_feed(CsiSession* s, uint32_t now, uint32_t state, uint32_t score, bool valid) {
    uint32_t dt = now - s->last;
    bool continuous = s->initialized && s->valid && valid && dt < 2000;
    bool moving = valid && state == 2;
    if(continuous) {
        s->valid_ms = add(s->valid_ms, dt);
        if(s->moving && moving) s->moving_ms = add(s->moving_ms, dt);
    }
    if(valid && score > s->peak) s->peak = score;
    if(moving && !(continuous && s->moving)) {
        s->events = add(s->events, 1);
        s->times[s->head] = now;
        s->head = (s->head + 1) % CSI_EVENTS;
        if(s->count < CSI_EVENTS) s->count++;
    }
    s->initialized = true;
    s->last = now;
    s->valid = valid;
    s->moving = moving;
}
