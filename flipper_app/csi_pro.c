#include "csi_pro.h"
#include <string.h>
void csi_settings_default(CsiSettings* s) {
    *s = (CsiSettings){.threshold = 120, .custom_threshold = 120, .profile = 1, .language = 1};
}
void csi_settings_encode(const CsiSettings* s, uint8_t b[CSI_SETTINGS_SIZE]) {
    memset(b, 0, CSI_SETTINGS_SIZE);
    b[0] = 'C';
    b[1] = 'S';
    b[2] = 'P';
    b[3] = 2;
    b[4] = (uint8_t)s->threshold;
    b[5] = (uint8_t)(s->threshold >> 8);
    b[6] = s->profile;
    b[7] = s->language;
    b[8] = (s->muted ? 1 : 0) | (s->automatic ? 2 : 0);
    b[10] = (uint8_t)s->custom_threshold;
    b[11] = (uint8_t)(s->custom_threshold >> 8);
    uint16_t crc = csi_crc16((const char*)b, 12);
    b[12] = (uint8_t)crc;
    b[13] = (uint8_t)(crc >> 8);
}
bool csi_settings_decode(CsiSettings* s, const uint8_t* b, size_t n) {
    if(!b || n != CSI_SETTINGS_SIZE || memcmp(b, "CSP\2", 4) || b[6] > 3 || b[7] > 1 || b[8] > 3 ||
       b[9])
        return false;
    if(csi_crc16((const char*)b, 12) != (uint16_t)(b[12] | (uint16_t)b[13] << 8)) return false;
    unsigned t = b[4] | (unsigned)b[5] << 8;
    unsigned custom = b[10] | (unsigned)b[11] << 8;
    if(t < CSI_THR_MIN || t > CSI_THR_MAX || custom < CSI_THR_MIN || custom > CSI_THR_MAX)
        return false;
    *s = (CsiSettings){
        .threshold = (uint16_t)t,
        .custom_threshold = (uint16_t)custom,
        .profile = b[6],
        .language = b[7],
        .muted = (b[8] & 1) != 0,
        .automatic = (b[8] & 2) != 0};
    return true;
}
static bool field(const char** p, char* out, size_t cap, bool last) {
    size_t n = 0;
    while(**p && **p != ',') {
        if(n + 1 >= cap) return false;
        out[n++] = *(*p)++;
    }
    out[n] = 0;
    if(last) return **p == 0;
    if(**p != ',') return false;
    (*p)++;
    return true;
}
bool csi_status_parse(const char* p, CsiStatus* s) {
    if(strncmp(p, "STAT,", 5)) return false;
    p += 5;
    uint32_t f[6];
    const uint32_t max[] = {1, 2000, 100, 255, 4, 1};
    for(unsigned i = 0; i < 6; i++) {
        char a[12];
        if(!field(&p, a, sizeof(a), i == 5) || !csi_uint(a, 0, max[i], &f[i])) return false;
    }
    *s = (CsiStatus){f[0], f[1], f[2], f[3], f[4], f[5]};
    return true;
}
bool csi_ap_parse(const char* p, uint32_t* index, CsiAccessPoint* ap) {
    if(strncmp(p, "AP,", 3)) return false;
    p += 3;
    char a[65];
    uint32_t i, signal, ch, auth;
    if(!field(&p, a, sizeof(a), false) || !csi_uint(a, 0, CSI_AP_MAX - 1, &i)) return false;
    if(!field(&p, a, sizeof(a), false) || a[0] != '-' || !csi_uint(a + 1, 0, 127, &signal))
        return false;
    if(!field(&p, a, sizeof(a), false) || !csi_uint(a, 1, 14, &ch)) return false;
    if(!field(&p, a, sizeof(a), false) || !csi_uint(a, 0, 20, &auth)) return false;
    CsiAccessPoint found = {
        .rssi = -(int32_t)signal, .channel = (uint8_t)ch, .auth = (uint8_t)auth};
    if(!field(&p, a, sizeof(a), true) || !csi_hex_decode(a, found.ssid, sizeof(found.ssid)) ||
       !found.ssid[0])
        return false;
    *index = i;
    *ap = found;
    return true;
}
bool csi_fresh(uint32_t age, uint32_t elapsed_ms) {
    return age < 2000 && elapsed_ms < 2000 - age;
}
