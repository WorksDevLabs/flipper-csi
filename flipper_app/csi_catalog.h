#pragma once
#include "csi_session.h"
#define CSI_CATALOG_MAX 12
typedef struct {
    char name[48];
    CsiSession summary;
    uint32_t rows, rejected;
    bool error;
    uint8_t filters;
} CsiSavedSession;
typedef struct {
    CsiSavedSession items[CSI_CATALOG_MAX];
    unsigned count;
    bool busy, error, limited;
    uint32_t generation;
} CsiCatalog;
/* Parse a schema-1 row with strict numeric bounds; header is handled by caller. */
bool csi_catalog_row(CsiSavedSession* item, const char* row);
