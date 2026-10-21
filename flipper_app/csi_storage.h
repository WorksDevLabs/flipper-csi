#pragma once
#include <furi.h>
#include "csi_pro.h"
typedef struct CsiStorage CsiStorage;
/* Startup-only read, before the UI and RX worker run. CRC + backup recovery. */
bool csi_storage_load(CsiSettings* settings);
CsiStorage* csi_storage_alloc(void);
void csi_storage_free(CsiStorage* storage);
bool csi_storage_settings(CsiStorage* storage, const CsiSettings* settings);
bool csi_storage_log(CsiStorage* storage, unsigned kind, const CsiReport* r, const CsiStatus* s);
/* kind: 0 sample, 1 session start, 2 session end. Never includes credentials. */
unsigned csi_storage_state(CsiStorage* storage); /* 0 off, 1 active, 2 SD error */
uint32_t csi_storage_lost(CsiStorage* storage);
bool csi_storage_settings_error(CsiStorage* storage);
