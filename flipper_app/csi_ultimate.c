#include "csi_ultimate.h"
#include <string.h>
bool csi_ultimate_valid(const CsiUltimatePrefs* p) {
    return p->delay < 3 && p->hold < 3 && p->release < 3 && p->repeat < 3 && p->filter < 2;
}
void csi_ultimate_default(CsiUltimatePrefs* p) {
    *p = (CsiUltimatePrefs){.delay = 1, .hold = 1, .release = 1, .repeat = 1, .filter = 0};
}
bool csi_alarm_step(CsiAlarm* a, const CsiUltimatePrefs* p, uint32_t now, bool fresh, bool moving) {
    static const uint32_t delay[] = {0, 5000, 15000}, hold[] = {100, 500, 1500};
    static const uint32_t release[] = {500, 2000, 5000}, repeat[] = {0, 10000, 30000};
    if(!csi_ultimate_valid(p) || !a->armed) return false;
    if(now - a->armed_at < delay[p->delay] || !fresh) {
        a->active = a->candidate = a->clearing = a->notified = false;
        return false;
    }
    if(moving) {
        a->clearing = false;
        if(!a->candidate) {
            a->candidate = true;
            a->since = now;
        }
        if(now - a->since >= hold[p->hold]) a->active = true;
    } else {
        a->candidate = false;
        if(a->active && !a->clearing) {
            a->clearing = true;
            a->clear_since = now;
        }
        if(a->clearing && now - a->clear_since >= release[p->release]) {
            a->active = a->clearing = a->notified = false;
        }
    }
    if(a->active &&
       (!a->notified || (repeat[p->repeat] && now - a->last_alert >= repeat[p->repeat]))) {
        a->notified = true;
        a->last_alert = now;
        return true;
    }
    return false;
}
static int digit(char c) {
    return c >= '0' && c <= '9' ? c - '0' :
           c >= 'A' && c <= 'F' ? c - 'A' + 10 :
           c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                                  -1;
}
bool csi_wave_parse(const char* text, uint8_t bins[CSI_WF_BINS]) {
    if(strncmp(text, "WF,", 3) || strlen(text + 3) != CSI_WF_BINS * 2) return false;
    uint8_t decoded[CSI_WF_BINS];
    for(unsigned i = 0; i < CSI_WF_BINS; i++) {
        int a = digit(text[3 + i * 2]), b = digit(text[4 + i * 2]);
        if(a < 0 || b < 0) return false;
        decoded[i] = (uint8_t)(a * 16 + b);
    }
    memcpy(bins, decoded, sizeof(decoded));
    return true;
}
void csi_ultimate_encode(const CsiUltimatePrefs* p, uint8_t bytes[CSI_ULTIMATE_BYTES]) {
    const uint8_t data[9] = {
        'U', 'L', 'T', 1, p->delay, p->hold, p->release, p->repeat, p->filter};
    memcpy(bytes, data, 9);
    uint16_t crc = csi_crc16((const char*)bytes, 9);
    bytes[9] = (uint8_t)crc;
    bytes[10] = (uint8_t)(crc >> 8);
}
bool csi_ultimate_decode(CsiUltimatePrefs* p, const uint8_t* b, size_t size) {
    if(size != CSI_ULTIMATE_BYTES || memcmp(b, "ULT\1", 4) ||
       (uint16_t)(b[9] | ((uint16_t)b[10] << 8)) != csi_crc16((const char*)b, 9))
        return false;
    CsiUltimatePrefs decoded = {b[4], b[5], b[6], b[7], b[8]};
    if(!csi_ultimate_valid(&decoded)) return false;
    *p = decoded;
    return true;
}

bool csi_perf_parse(const char* text, uint32_t values[6]) {
    if(strncmp(text, "PERF,", 5) || strlen(text) >= 100) return false;
    char copy[100];
    strcpy(copy, text + 5);
    char* field = copy;
    uint32_t parsed[6];
    const uint32_t maxima[] = {UINT32_MAX, 1000000, 16, 1, 100000, 100000};
    for(unsigned i = 0; i < 6; i++) {
        char* next = strchr(field, ',');
        if((i < 5 && !next) || (i == 5 && next)) return false;
        if(next) *next = 0;
        if(!csi_uint(field, 0, maxima[i], &parsed[i])) return false;
        if(next) field = next + 1;
    }
    memcpy(values, parsed, sizeof(parsed));
    return true;
}

bool csi_measurement_ready(bool started, bool awaiting, uint32_t age, uint32_t elapsed) {
    return started && !awaiting && age < 2000 && elapsed < 2000 - age;
}
