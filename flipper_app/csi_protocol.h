#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CSI_LINE_CAP         256
#define CSI_THR_MIN          5
#define CSI_THR_MAX          2000
#define CSI_PROTOCOL_VERSION 3

typedef struct {
    uint32_t seq, pkts, age, rate, drops, progress;
    int32_t score, rssi, state, threshold;
} CsiReport;
typedef struct {
    char data[CSI_LINE_CAP];
    size_t size;
    bool dropping;
} CsiLine;
/* Feed ASCII. Oversize/invalid lines are discarded THROUGH their terminator. */
bool csi_line_feed(CsiLine* line, uint8_t byte);
uint16_t csi_crc16(const char* text, size_t size);
bool csi_frame(char* out, size_t capacity, const char* payload);
/* Verify in-place, strip '$' and '*hhhh'; returns payload or NULL. */
char* csi_unframe(char* line);
bool csi_report_parse(const char* payload, CsiReport* report);
bool csi_uint(const char* text, uint32_t min, uint32_t max, uint32_t* value);
bool csi_hex_encode(const char* in, char* out, size_t capacity);
bool csi_hex_decode(const char* in, char* out, size_t capacity);
