#include "csi_ultimate.h"
#include "csi_catalog.h"
#include "csi_motion.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
int main(void) {
    assert(!csi_measurement_ready(false,false,0,0));
    assert(!csi_measurement_ready(false,false,0,100));
    assert(!csi_measurement_ready(true,true,0,0));
    assert(csi_measurement_ready(true,false,0,0));
    assert(!csi_measurement_ready(true,false,1999,1));
    assert(!csi_measurement_ready(true,false,UINT32_MAX,0));

    CsiUltimatePrefs p;
    csi_ultimate_default(&p);
    uint8_t bytes[CSI_ULTIMATE_BYTES];
    csi_ultimate_encode(&p, bytes);
    CsiUltimatePrefs other;
    assert(csi_ultimate_decode(&other, bytes, sizeof(bytes)));
    for(unsigned i = 0; i < sizeof(bytes); i++) {
        bytes[i] ^= 1;
        assert(!csi_ultimate_decode(&other, bytes, sizeof(bytes)));
        bytes[i] ^= 1;
    }
    CsiAlarm a = {.armed = true, .armed_at = UINT32_MAX - 999};
    p.delay = 0;
    p.hold = 1;
    p.release = 0;
    p.repeat = 1;
    assert(!csi_alarm_step(&a, &p, UINT32_MAX - 499, true, true));
    assert(csi_alarm_step(&a, &p, 0, true, true));
    assert(!csi_alarm_step(&a, &p, 9999, true, true));
    assert(csi_alarm_step(&a, &p, 10000, true, true));
    assert(!csi_alarm_step(&a, &p, 10100, false, true));
    assert(!a.active);
    assert(!csi_alarm_step(&a, &p, 10200, true, true));
    assert(csi_alarm_step(&a, &p, 10700, true, true));
    assert(!csi_alarm_step(&a, &p, 10800, true, false));
    assert(a.active);
    assert(!csi_alarm_step(&a, &p, 11300, true, false));
    assert(!a.active);
    p.delay = 1;
    a = (CsiAlarm){.armed = true, .armed_at = 0};
    assert(!csi_alarm_step(&a, &p, 4900, true, true));
    assert(!a.candidate);
    a.armed = false;
    assert(!csi_alarm_step(&a, &p, 99999, true, true));
    char wave[98] = "WF,";
    memset(wave + 3, '4', 94);
    wave[97] = 0;
    uint8_t bins[47];
    assert(csi_wave_parse(wave, bins));
    assert(bins[46] == 0x44);
    wave[95] = 'Z';
    assert(!csi_wave_parse(wave, bins));
    assert(!csi_wave_parse("WF,00", bins));
    uint32_t perf[6];
    assert(csi_perf_parse("PERF,180000,150,5,1,120,20", perf));
    assert(perf[3] == 1 && perf[1] == 150);
    assert(!csi_perf_parse("PERF,180000,150,999,1,120,20", perf));
    assert(!csi_perf_parse("PERF,1,2,3,4,5", perf));
    uint32_t seed = 12345;
    for(unsigned i = 0; i < 10000; i++) {
        char malformed[120];
        unsigned n = i % 119;
        for(unsigned j = 0; j < n; j++) {
            seed = seed * 1664525U + 1013904223U;
            malformed[j] = (char)(32 + (seed >> 24) % 95);
        }
        malformed[n] = 0;
        CsiSavedSession scratch = {0};
        (void)csi_catalog_row(&scratch, malformed);
        (void)csi_perf_parse(malformed, perf);
        (void)csi_wave_parse(malformed, bins);
    }
    CsiSavedSession item = {0};
    assert(csi_catalog_row(&item, "1,0,0,START,1,10,120,-65,1,20,0,90,0,5"));
    assert(csi_catalog_row(&item, "1,0,1000,STATE,2,200,120,-65,2,20,0,90,0,5"));
    assert(csi_catalog_row(&item, "1,0,2000,SAMPLE,3,200,120,-65,2,20,0,90,0,5"));
    assert(item.summary.events == 1 && item.summary.moving_ms == 1000);
    assert(csi_catalog_row(&item, "2,0,3000,SAMPLE,4,200,120,-65,2,20,0,90,0,5,1"));
    assert(!csi_catalog_row(&item, "2,0,3000,SAMPLE,4,200,120,-65,2,20,0,90,0,5,9"));
    assert(!csi_catalog_row(&item, "1,0,2000,SAMPLE,3,200,120,-999,2,20,0,90,0,5"));
    assert(!csi_catalog_row(&item, "1,0,2000,SAMPLE,3,200,120,-65,2,20,0,101,0,5"));
    csi_motion_t classic, robust;
    csi_motion_init(&classic);
    csi_motion_init(&robust);
    robust.filter = 1;
    int8_t iq[128];
    memset(iq, 20, sizeof(iq));
    for(unsigned i = 0; i < 100; i++) {
        csi_motion_update(&classic, iq, 128, 0);
        csi_motion_update(&robust, iq, 128, 0);
    }
    iq[20] = 100;
    iq[21] = 100;
    csi_motion_update(&classic, iq, 128, 0);
    csi_motion_update(&robust, iq, 128, 0);
    assert(robust.score <= classic.score);
    for(unsigned i = 0; i < 80; i++) {
        for(unsigned j = 0; j < 64; j++) {
            iq[j * 2] = (j + i) % 2 ? 10 : 35;
            iq[j * 2 + 1] = 20;
        }
        csi_motion_update(&robust, iq, 128, 0);
    }
    assert(csi_motion_state(&robust) == CSI_STATE_MOVING);
    csi_motion_reset(&robust);
    assert(robust.filter == 1 && robust.raw_count == 0);
    puts(
        "ULTIMATE PASS: alarm delay/hold/release/repeat/gap/wrap, CRC, waterfall, CSV, robust filter");
}
