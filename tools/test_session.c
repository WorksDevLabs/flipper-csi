#include "csi_session.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
int main(void) {
    CsiSession s = {0};
    csi_session_feed(&s, UINT32_MAX - 99, 1, 10, true);
    csi_session_feed(&s, 0, 2, 200, true);
    csi_session_feed(&s, 100, 2, 300, true);
    assert(s.events == 1 && s.valid_ms == 200 && s.moving_ms == 100 && s.peak == 300);
    assert(s.durations[0] == 100);
    csi_session_feed(&s, 5000, 2, 999, false);
    assert(s.valid_ms == 200 && s.peak == 300);
    csi_session_feed(&s, 5100, 2, 200, true);
    assert(s.events == 2 && s.moving_ms == 100);
    assert(s.durations[1] == 0 && s.durations[0] == 100);
    csi_session_feed(&s, 5150, 2, 200, true);
    assert(s.durations[1] == 50);
    for(unsigned i = 0; i < 100; i++) {
        csi_session_feed(&s, 5200 + i * 200, 1, 0, true);
        csi_session_feed(&s, 5300 + i * 200, 2, 250, true);
    }
    assert(s.count == CSI_EVENTS && s.events == 102 && s.head < CSI_EVENTS);
    s.valid_ms = UINT32_MAX - 1;
    csi_session_feed(&s, 25200, 1, 0, true);
    assert(s.valid_ms == UINT32_MAX);

    /* Persistence: wall-clock round trip across a "reboot" (new uptime base). */
    CsiSession p = {0};
    csi_session_feed(&p, 10000, 1, 10, true);
    csi_session_feed(&p, 10100, 2, 400, true); /* event A at 10.1 s */
    csi_session_feed(&p, 11600, 2, 300, true);
    csi_session_feed(&p, 13100, 2, 300, true); /* lasts 3 s */
    csi_session_feed(&p, 13200, 1, 10, true);
    csi_session_feed(&p, 20000, 1, 10, false);
    csi_session_feed(&p, 25100, 2, 200, true); /* event B at 25.1 s, open */
    uint8_t bytes[CSI_SESSION_BYTES];
    csi_session_encode(&p, 30100, 1000000, bytes); /* B was 5 s ago, A 20 s ago */
    CsiSession q;
    /* Restored 60 s later on a fresh uptime of 500 ms. */
    assert(csi_session_decode(&q, bytes, sizeof(bytes), 500, 1000060));
    assert(q.events == 2 && q.count == 2 && q.head == 2 && q.peak == 400);
    assert(q.valid_ms == p.valid_ms && q.moving_ms == p.moving_ms);
    assert(q.durations[0] == 3000 && q.durations[1] == 0);
    assert((uint32_t)(500 - q.times[0]) == 80000 && (uint32_t)(500 - q.times[1]) == 65000);
    assert(!q.initialized && !q.valid && !q.moving);
    /* New samples after restore start fresh events, never bridging the gap. */
    csi_session_feed(&q, 600, 2, 100, true);
    assert(q.events == 3 && q.count == 3 && q.moving_ms == p.moving_ms);
    /* Corruption, wrong size, bad count are rejected and leave output untouched. */
    CsiSession keep = q;
    for(unsigned i = 0; i < sizeof(bytes); i++) {
        bytes[i] ^= 0x10;
        assert(!csi_session_decode(&q, bytes, sizeof(bytes), 500, 1000060));
        bytes[i] ^= 0x10;
    }
    assert(!csi_session_decode(&q, bytes, sizeof(bytes) - 1, 500, 1000060));
    assert(!memcmp(&keep, &q, sizeof(q)));
    /* Full ring keeps oldest-first order; RTC set backwards clamps to "now". */
    CsiSession full = {0};
    for(unsigned i = 0; i < 12; i++) {
        csi_session_feed(&full, 1000 + i * 1000, 1, 0, true);
        csi_session_feed(&full, 1500 + i * 1000, 2, 100, true);
    }
    csi_session_encode(&full, 13000, 5000, bytes);
    assert(csi_session_decode(&q, bytes, sizeof(bytes), 100, 10));
    assert(q.count == CSI_EVENTS && q.head == 0 && q.events == 12);
    for(unsigned k = 0; k < CSI_EVENTS; k++) assert(q.times[k] == 100);
    return 0;
}
