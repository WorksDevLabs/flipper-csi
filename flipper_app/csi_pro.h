#pragma once
#include "csi_protocol.h"
#define CSI_SETTINGS_SIZE 14
#define CSI_AP_MAX        12

typedef struct {
    uint16_t threshold, custom_threshold;
    uint8_t profile, language;
    bool muted, automatic;
} CsiSettings;
typedef struct {
    uint32_t automatic, noise, quality, reason, cal_error, calibrated;
} CsiStatus;
typedef struct {
    char ssid[33];
    int32_t rssi;
    uint8_t channel, auth;
} CsiAccessPoint;
void csi_settings_default(CsiSettings* s);
void csi_settings_encode(const CsiSettings* s, uint8_t out[CSI_SETTINGS_SIZE]);
bool csi_settings_decode(CsiSettings* s, const uint8_t* bytes, size_t size);
bool csi_status_parse(const char* text, CsiStatus* status);
bool csi_ap_parse(const char* text, uint32_t* index, CsiAccessPoint* ap);
bool csi_fresh(uint32_t age, uint32_t elapsed_ms);
