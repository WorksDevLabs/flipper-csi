#include "csi_protocol.h"
#include <stdio.h>
#include <string.h>

bool csi_line_feed(CsiLine* l, uint8_t b) {
    if(b == '\r' || b == '\n') {
        bool ready = !l->dropping && l->size > 0;
        l->data[l->size] = 0;
        l->size = 0;
        l->dropping = false;
        return ready;
    }
    if(l->dropping) return false;
    if(b < 32 || b > 126 || l->size >= sizeof(l->data) - 1) {
        l->dropping = true;
        l->size = 0;
    } else
        l->data[l->size++] = (char)b;
    return false;
}
uint16_t csi_crc16(const char* text, size_t size) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)(uint8_t)text[i] << 8;
        for(unsigned j = 0; j < 8; j++)
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}
static int nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
bool csi_frame(char* out, size_t cap, const char* p) {
    size_t n = strlen(p);
    if(n + 9 > cap || strchr(p, '*') || strchr(p, '\n') || strchr(p, '\r')) return false;
    int written = snprintf(out, cap, "$%s*%04X\n", p, (unsigned)csi_crc16(p, n));
    return written > 0 && (size_t)written < cap;
}
char* csi_unframe(char* line) {
    if(line[0] != '$') return NULL;
    char* star = strchr(line + 1, '*');
    if(!star || strlen(star + 1) != 4) return NULL;
    uint16_t expected = 0;
    for(unsigned i = 1; i <= 4; i++) {
        int n = nibble(star[i]);
        if(n < 0) return NULL;
        expected = (uint16_t)((expected << 4) | n);
    }
    if(csi_crc16(line + 1, (size_t)(star - line - 1)) != expected) return NULL;
    *star = 0;
    return line + 1;
}
bool csi_uint(const char* text, uint32_t min, uint32_t max, uint32_t* value) {
    if(!text || !*text) return false;
    uint32_t n = 0;
    for(const char* p = text; *p; p++) {
        if(*p < '0' || *p > '9') return false;
        unsigned digit = (unsigned)(*p - '0');
        if(n > max / 10 || (n == max / 10 && digit > max % 10)) return false;
        n = n * 10 + digit;
    }
    if(n < min) return false;
    *value = n;
    return true;
}
bool csi_report_parse(const char* p, CsiReport* r) {
    if(strncmp(p, "CSI2,", 5) != 0) return false;
    p += 5;
    int64_t f[10];
    const uint32_t maximum[10] = {
        UINT32_MAX, 2000, 127, 6, UINT32_MAX, CSI_THR_MAX, UINT32_MAX, 1000, UINT32_MAX, 100};
    for(unsigned i = 0; i < 10; i++) {
        char field[12];
        size_t n = 0;
        bool neg = false;
        if(i == 2 && *p == '-') {
            neg = true;
            p++;
        }
        while(*p && *p != ',') {
            if(n >= sizeof(field) - 1) return false;
            field[n++] = *p++;
        }
        field[n] = 0;
        uint32_t v;
        if(!csi_uint(field, 0, maximum[i], &v)) return false;
        f[i] = neg ? -(int64_t)v : (int64_t)v;
        if(i < 9) {
            if(*p != ',') return false;
            p++;
        } else if(*p)
            return false;
    }
    if(f[2] > 0 || f[5] < CSI_THR_MIN) return false;
    *r = (CsiReport){
        .seq = (uint32_t)f[0],
        .score = (int32_t)f[1],
        .rssi = (int32_t)f[2],
        .state = (int32_t)f[3],
        .pkts = (uint32_t)f[4],
        .threshold = (int32_t)f[5],
        .age = (uint32_t)f[6],
        .rate = (uint32_t)f[7],
        .drops = (uint32_t)f[8],
        .progress = (uint32_t)f[9]};
    return true;
}
bool csi_hex_encode(const char* in, char* out, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(in);
    if(n * 2 + 1 > cap) return false;
    for(size_t i = 0; i < n; i++) {
        out[2 * i] = hex[(uint8_t)in[i] >> 4];
        out[2 * i + 1] = hex[(uint8_t)in[i] & 15];
    }
    out[2 * n] = 0;
    return true;
}
bool csi_hex_decode(const char* in, char* out, size_t cap) {
    size_t n = strlen(in);
    if(n % 2 || n / 2 >= cap) return false;
    for(size_t i = 0; i < n; i += 2) {
        int a = nibble(in[i]), b = nibble(in[i + 1]);
        if(a < 0 || b < 0 || (a == 0 && b == 0)) return false;
        out[i / 2] = (char)((a << 4) | b);
    }
    out[n / 2] = 0;
    return true;
}
