#include "csi_session.h"
#include <assert.h>
#include <limits.h>
int main(void) {
    CsiSession s = {0};
    csi_session_feed(&s, UINT32_MAX - 99, 1, 10, true);
    csi_session_feed(&s, 0, 2, 200, true);
    csi_session_feed(&s, 100, 2, 300, true);
    assert(s.events == 1 && s.valid_ms == 200 && s.moving_ms == 100 && s.peak == 300);
    csi_session_feed(&s, 5000, 2, 999, false);
    assert(s.valid_ms == 200 && s.peak == 300);
    csi_session_feed(&s, 5100, 2, 200, true);
    assert(s.events == 2 && s.moving_ms == 100);
    for(unsigned i = 0; i < 100; i++) {
        csi_session_feed(&s, 5200 + i * 200, 1, 0, true);
        csi_session_feed(&s, 5300 + i * 200, 2, 250, true);
    }
    assert(s.count == CSI_EVENTS && s.events == 102 && s.head < CSI_EVENTS);
    s.valid_ms = UINT32_MAX - 1;
    csi_session_feed(&s, 25200, 1, 0, true);
    assert(s.valid_ms == UINT32_MAX);
    return 0;
}
