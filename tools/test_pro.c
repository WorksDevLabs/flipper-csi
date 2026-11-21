#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "csi_pro.h"
#include "csi_motion.h"
static void packet(int8_t* p, unsigned sample, bool noisy) {
    for(unsigned i = 0; i < 64; i++) {
        int v = 20 + (int)(i % 9);
        if(noisy) v += ((i + sample) % 2) ? 9 : -9;
        p[2 * i] = (int8_t)v;
        p[2 * i + 1] = (int8_t)(v / 2);
    }
}
int main(void) {
    CsiSettings s, copy;
    csi_settings_default(&s);
    s.custom_threshold = 175;
    s.muted = true;
    s.automatic = true;
    s.profile = 3;
    uint8_t bytes[CSI_SETTINGS_SIZE];
    csi_settings_encode(&s, bytes);
    assert(
        csi_settings_decode(&copy, bytes, sizeof(bytes)) && copy.threshold == 120 &&
        copy.custom_threshold == 175 && copy.muted && copy.automatic && !copy.quiet_motion);
    s.quiet_motion = true;
    csi_settings_encode(&s, bytes);
    assert(csi_settings_decode(&copy, bytes, sizeof(bytes)) && copy.quiet_motion && copy.muted);
    assert(!copy.alarm_quiet);
    s.alarm_quiet = true;
    csi_settings_encode(&s, bytes);
    assert(bytes[8] == 15);
    assert(
        csi_settings_decode(&copy, bytes, sizeof(bytes)) && copy.alarm_quiet && copy.quiet_motion);
    s.alarm_quiet = false;
    {
        /* 2.0 files (flags <= 3) still decode with quiet_motion off. */
        CsiSettings old = s;
        old.quiet_motion = false;
        uint8_t legacy[CSI_SETTINGS_SIZE];
        csi_settings_encode(&old, legacy);
        assert(legacy[8] == 3);
        assert(csi_settings_decode(&copy, legacy, sizeof(legacy)) && !copy.quiet_motion);
    }
    for(unsigned i = 0; i < sizeof(bytes); i++) {
        bytes[i] ^= 1;
        assert(!csi_settings_decode(&copy, bytes, sizeof(bytes)));
        bytes[i] ^= 1;
    }
    assert(!csi_settings_decode(&copy, bytes, sizeof(bytes) - 1));
    s.threshold = 2001;
    csi_settings_encode(&s, bytes);
    assert(!csi_settings_decode(&copy, bytes, sizeof(bytes)));
    s.threshold = 120;
    s.custom_threshold = 0;
    csi_settings_encode(&s, bytes);
    assert(!csi_settings_decode(&copy, bytes, sizeof(bytes)));
    CsiStatus status;
    assert(
        csi_status_parse("STAT,1,12,90,202,0,1", &status) && status.automatic == 1 &&
        status.calibrated == 1);
    assert(!csi_status_parse("STAT,1,12,101,202,0,1", &status));
    assert(!csi_status_parse("STAT,1,12,90,202,0,1x", &status));
    assert(!csi_status_parse("STAT,1,12,90,202,0", &status));
    CsiAccessPoint ap;
    uint32_t index;
    assert(csi_ap_parse("AP,2,-65,6,3,44656D6F204576", &index, &ap));
    assert(index == 2 && !strcmp(ap.ssid, "Demo Ev") && ap.rssi == -65 && ap.channel == 6);
    assert(!csi_ap_parse("AP,12,-65,6,3,6162", &index, &ap));
    assert(!csi_ap_parse("AP,0,-65,0,3,6162", &index, &ap));
    assert(!csi_ap_parse("AP,0,-65,6,3,00", &index, &ap));
    assert(!csi_ap_parse("AP,0,-65,6,3,6162,extra", &index, &ap));
    assert(csi_fresh(1999, 0) && !csi_fresh(1999, 1));
    assert(!csi_fresh(UINT32_MAX, 1) && !csi_fresh(0, UINT32_MAX));
    csi_motion_t m;
    int8_t p[128];
    csi_motion_init(&m);
    m.automatic = true;
    csi_motion_calibrate(&m, 100);
    for(unsigned i = 0; i < 100; i++) {
        packet(p, i, false);
        assert(csi_motion_feed(&m, p, 128, i % 2 ? 4 : 0, i * 50));
    }
    assert(m.calibrated && m.cal_error == 0 && !m.calibrating);
    assert(fabsf(m.threshold - 0.04f) < 0.0001f && m.noise < 0.0001f);
    csi_motion_calibrate(&m, 100);
    for(unsigned i = 0; i < 100; i++) {
        packet(p, i, true);
        csi_motion_feed(&m, p, 128, 0, 5000 + i * 50);
    }
    assert(m.cal_error == 2 && !m.calibrated && csi_motion_state(&m) == CSI_STATE_WARMUP);
    csi_motion_calibrate(&m, 100);
    packet(p, 0, false);
    csi_motion_feed(&m, p, 128, 0, 11000);
    csi_motion_feed(&m, p, 128, 0, 14000);
    assert(m.cal_error == 1 && !m.calibrated && !m.calibrating);
    csi_motion_init(&m);
    m.threshold = 0.25f;
    csi_motion_calibrate(&m, 20);
    for(unsigned i = 0; i < 20; i++) {
        packet(p, i, false);
        csi_motion_feed(&m, p, 128, 0, i * 50);
    }
    assert(m.calibrated && fabsf(m.threshold - 0.25f) < 0.0001f);
    csi_motion_calibrate(&m, 20);
    for(unsigned i = 0; i < 20; i++) {
        packet(p, i, false);
        csi_motion_feed(&m, p, 128, 0, 2000 + i * 500);
    }
    assert(!m.calibrated && m.cal_error == 3);
    /* Random settings bytes must never be accepted without checksum/bounds. */
    unsigned seed = 7;
    for(unsigned n = 0; n < 10000; n++) {
        for(unsigned i = 0; i < sizeof(bytes); i++) {
            seed = seed * 1664525U + 1013904223U;
            bytes[i] = (uint8_t)(seed >> 24);
        }
        assert(!csi_settings_decode(&copy, bytes, sizeof(bytes)));
    }
    puts(
        "PRO PASS: settings CRC/custom profile, status/scan bounds, stale ages, exact auto calibration, noisy/gap rejection");
}
