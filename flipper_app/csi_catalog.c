#include "csi_catalog.h"
#include "csi_protocol.h"
#include <string.h>
#include <stdlib.h>
bool csi_catalog_row(CsiSavedSession* item, const char* row) {
    char line[220];
    if(strlen(row) >= sizeof(line)) return false;
    strcpy(line, row);
    char* fields[15];
    unsigned count = 0;
    char* start = line;
    for(char* c = line;; c++) {
        if(*c == ',' || *c == 0) {
            char v = *c;
            *c = 0;
            if(count >= 15) return false;
            fields[count++] = start;
            start = c + 1;
            if(!v) break;
        }
    }
    bool schema2 = count == 15 && !strcmp(fields[0], "2");
    if(!schema2 && (count != 14 || strcmp(fields[0], "1"))) return false;
    uint32_t filter = 0;
    if(schema2 && !csi_uint(fields[14], 0, 1, &filter)) return false;
    uint32_t t, score, threshold, state, age, unused;
    if(!csi_uint(fields[2], 0, UINT32_MAX, &t) || !csi_uint(fields[5], 0, 100000, &score) ||
       !csi_uint(fields[6], 5, 2000, &threshold) || !csi_uint(fields[8], 0, 6, &state) ||
       !csi_uint(fields[10], 0, UINT32_MAX, &age) ||
       !csi_uint(fields[1], 0, UINT32_MAX, &unused) ||
       !csi_uint(fields[4], 0, UINT32_MAX, &unused))
        return false;
    if(strcmp(fields[3], "START") && strcmp(fields[3], "END") && strcmp(fields[3], "STATE") &&
       strcmp(fields[3], "SAMPLE"))
        return false;
    for(unsigned i = 9; i < 14; i++) {
        uint32_t maximum = i == 11 ? 100 : i == 12 ? 1 : UINT32_MAX;
        if(!csi_uint(fields[i], 0, maximum, &unused)) return false;
    }
    char* end;
    long rssi = strtol(fields[7], &end, 10);
    if(!fields[7][0] || *end || rssi < -127 || rssi > 0) return false;
    csi_session_feed(&item->summary, t, state, score, age < 2000 && (state == 1 || state == 2));
    item->filters |= schema2 ? (uint8_t)(1U << filter) : 4;
    item->rows++;
    return true;
}
