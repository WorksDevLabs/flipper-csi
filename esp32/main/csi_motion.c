#include "csi_motion.h"
#include <math.h>
#include <string.h>
#define WARMUP_PACKETS 40U

/* 20 MHz LLTF: keep data bins +/-2..26 and -1; exclude DC, guards,
 * pilots (+/-7,+/-21), and bins 0/1 affected by first_word_invalid. */
static bool usable(unsigned i) {
    return ((i >= 2 && i <= 26) || (i >= 38 && i <= 63)) &&
           i != 7 && i != 21 && i != 43 && i != 57;
}
void csi_motion_init(csi_motion_t* m) {
    memset(m, 0, sizeof(*m));
    m->threshold = 0.12f;
    m->warmup_left = WARMUP_PACKETS;
}
void csi_motion_reset(csi_motion_t* m) {
    float threshold = m->threshold;
    bool automatic=m->automatic; bool interrupted=m->calibrating;
    uint32_t accepted = m->accepted, rejected = m->rejected, clock = m->synthetic_ms;
    csi_motion_init(m);
    m->threshold = threshold;
    m->automatic=automatic;
    m->cal_error=interrupted ? 1 : 0;
    m->accepted = accepted;
    m->rejected = rejected;
    m->synthetic_ms = clock;
}
void csi_motion_calibrate(csi_motion_t* m, uint32_t packets) {
    csi_motion_reset(m);
    if(packets < 1) packets = 1;
    if(packets > 2000) packets = 2000;
    m->cal_error=0;
    m->calibrating = true;
    m->cal_total = m->cal_left = packets;
}
bool csi_motion_feed(csi_motion_t* m, const int8_t* buf, uint16_t len,
                     uint16_t skip, uint32_t now_ms) {
    if(!buf || len != 128 || (skip != 0 && skip != 4)) {
        m->rejected++;
        return false;
    }
    float amp[64] = {0}, total = 0;
    unsigned count = 0, clipped = 0;
    for(unsigned i = 0; i < 64; i++) {
        if(!usable(i)) continue;
        int im = buf[2*i], re = buf[2*i+1];
        if(im == -128 || im == 127 || re == -128 || re == 127) clipped++;
        amp[i] = sqrtf((float)(im*im + re*re));
        total += amp[i];
        count++;
    }
    if(total < (float)count || clipped > count / 4) {
        m->rejected++;
        return false;
    }
    m->quality=(uint8_t)(100U*(count-clipped)/count);
    /* Remove packet-wide gain changes (e.g. receiver AGC). */
    float mean = total / (float)count;
    for(unsigned i = 0; i < 64; i++) amp[i] /= mean;
    uint32_t dt = m->primed ? now_ms - m->last_ms : 50;
    if(m->primed && dt >= CSI_STALE_MS) {
        /* A gap invalidates calibration as well as the old baseline. */
        csi_motion_reset(m);
    }
    if(dt > 250) dt = 250;
    m->last_ms = now_ms;
    m->accepted++;
    if(m->calibrating) {
        uint32_t k = m->cal_total - m->cal_left + 1;
        if(k==1) m->cal_start_ms=now_ms;
        float a = 1.0f / (float)k;
        for(unsigned i = 0; i < 64; i++) {
            float delta=amp[i]-m->baseline[i];
            m->baseline[i] += a*delta;
            m->cal_m2[i] += delta*(amp[i]-m->baseline[i]);
        }
        m->primed = true;
        if(--m->cal_left == 0) {
            m->calibrating = false;
            float scatter=0; unsigned used=0;
            for(unsigned i=0;i<64;i++) if(usable(i) && m->baseline[i]>=0.15f) {
                scatter+=sqrtf(fmaxf(m->cal_m2[i]/(float)(m->cal_total>1?m->cal_total-1:1),0.0f))/(m->baseline[i]+0.05f);
                used++;
            }
            m->noise=used?scatter/(float)used:2.0f;
            bool rate_ok=now_ms-m->cal_start_ms<=(m->cal_total>1?m->cal_total-1:1)*200U;
            m->calibrated=used>=24 && m->noise<=0.10f && rate_ok;
            m->cal_error=m->calibrated?0:!rate_ok?3:2;
            m->warmup_left=m->calibrated?0:WARMUP_PACKETS;
            if(!m->calibrated) m->primed=false;
            if(m->calibrated && m->automatic)
                m->threshold=fminf(0.8f,fmaxf(0.04f,m->noise*4.0f+0.02f));
        }
        return true;
    }
    if(!m->primed) {
        memcpy(m->baseline, amp, sizeof(amp));
        m->primed = true;
        m->warmup_left--;
        return true;
    }
    if(m->warmup_left) {
        for(unsigned i = 0; i < 64; i++) m->baseline[i] += 0.1f*(amp[i]-m->baseline[i]);
        m->warmup_left--;
        return true;
    }
    float raw = 0;
    unsigned used = 0;
    for(unsigned i = 0; i < 64; i++) {
        if(!usable(i) || m->baseline[i] < 0.15f) continue;
        float dev = fabsf(amp[i]-m->baseline[i])/(m->baseline[i]+0.05f);
        raw += fminf(dev, 2.0f); /* bound deep-fade outliers */
        used++;
    }
    if(used) raw /= (float)used;
    float a_score = 1.0f-expf(-(float)dt/225.0f);
    float a_base = 1.0f-expf(-(float)dt/5000.0f);
    m->score += a_score*(raw-m->score);
    for(unsigned i = 0; i < 64; i++) m->baseline[i] += a_base*(amp[i]-m->baseline[i]);
    if(m->score > m->threshold) {
        m->below_active = false;
        if(!m->above_active) { m->above_active = true; m->above_ms = now_ms; }
        if(now_ms-m->above_ms >= 100) m->moving = true;
    } else if(m->score < m->threshold*0.7f) {
        m->above_active = false;
        if(!m->below_active) { m->below_active = true; m->below_ms = now_ms; }
        if(now_ms-m->below_ms >= 700) m->moving = false;
    } else {
        m->above_active = m->below_active = false;
    }
    return true;
}
float csi_motion_update(csi_motion_t* m, const int8_t* buf, uint16_t len, uint16_t skip) {
    m->synthetic_ms += 50;
    csi_motion_feed(m, buf, len, skip, m->synthetic_ms);
    return m->score;
}
csi_state_t csi_motion_state(const csi_motion_t* m) {
    if(m->calibrating) return CSI_STATE_CALIBRATING;
    if(!m->primed || m->warmup_left) return CSI_STATE_WARMUP;
    return m->moving ? CSI_STATE_MOVING : CSI_STATE_STILL;
}
uint32_t csi_motion_progress(const csi_motion_t* m) {
    if(m->calibrating && m->cal_total) return 100U*(m->cal_total-m->cal_left)/m->cal_total;
    if(m->warmup_left) return 100U*(WARMUP_PACKETS-m->warmup_left)/WARMUP_PACKETS;
    return 100;
}

