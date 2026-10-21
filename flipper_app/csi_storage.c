#include "csi_storage.h"
#include <storage/storage.h>
#include <furi_hal.h>
#include <datetime/datetime.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DATA        EXT_PATH("apps_data/csi_radar")
#define LOGS        DATA "/logs"
#define ROTATE_SIZE (256U * 1024U)
typedef struct {
    unsigned kind;
    uint32_t stamp, tick;
    CsiSettings settings;
    CsiReport report;
    CsiStatus status;
} Record;
struct CsiStorage {
    FuriMessageQueue* queue;
    FuriThread* thread;
    atomic_uint state, lost;
    atomic_bool settings_error;
};
static bool read_settings(Storage* s, const char* name, CsiSettings* settings) {
    File* f = storage_file_alloc(s);
    uint8_t bytes[CSI_SETTINGS_SIZE];
    bool ok = false;
    if(storage_file_open(f, name, FSAM_READ, FSOM_OPEN_EXISTING))
        ok = storage_file_size(f) == sizeof(bytes) &&
             storage_file_read(f, bytes, sizeof(bytes)) == sizeof(bytes) &&
             csi_settings_decode(settings, bytes, sizeof(bytes));
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}
bool csi_storage_load(CsiSettings* settings) {
    csi_settings_default(settings);
    Storage* s = furi_record_open(RECORD_STORAGE);
    bool loaded = read_settings(s, DATA "/settings.bin", settings) ||
                  read_settings(s, DATA "/settings.bak", settings);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}
static bool save_settings(Storage* s, const CsiSettings* settings) {
    if(!storage_simply_mkdir(s, DATA)) return false;
    uint8_t bytes[CSI_SETTINGS_SIZE];
    csi_settings_encode(settings, bytes);
    File* f = storage_file_alloc(s);
    bool ok = storage_file_open(f, DATA "/settings.tmp", FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok)
        ok = storage_file_write(f, bytes, sizeof(bytes)) == sizeof(bytes) && storage_file_sync(f);
    storage_file_close(f);
    storage_file_free(f);
    if(!ok) return false;
    if(storage_file_exists(s, DATA "/settings.bin")) {
        storage_common_remove(s, DATA "/settings.bak");
        if(storage_common_rename(s, DATA "/settings.bin", DATA "/settings.bak") != FSE_OK)
            return false;
    }
    if(storage_common_rename(s, DATA "/settings.tmp", DATA "/settings.bin") != FSE_OK) {
        (void)storage_common_rename(s, DATA "/settings.bak", DATA "/settings.bin");
        return false;
    }
    return true;
}
static bool open_log(Storage* s, File* f, uint32_t stamp, uint32_t* serial) {
    if(!storage_simply_mkdir(s, DATA) || !storage_simply_mkdir(s, LOGS)) return false;
    char path[100];
    bool opened = false;
    for(unsigned attempt = 0; attempt < 100; attempt++) {
        snprintf(
            path,
            sizeof(path),
            LOGS "/session-%08lX-%04lu.csv",
            (unsigned long)stamp,
            (unsigned long)(*serial)++);
        if(storage_file_exists(s, path)) continue;
        opened = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_NEW);
        break;
    }
    static const char header[] =
        "schema,rtc_seconds,uptime_ms,event,seq,score_milli,threshold_milli,rssi,state,rate,age_ms,quality,auto,noise_milli\n";
    if(opened && storage_file_write(f, header, sizeof(header) - 1) != sizeof(header) - 1) {
        storage_file_close(f);
        return false;
    }
    return opened;
}
static int32_t storage_worker(void* ctx) {
    CsiStorage* a = ctx;
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    bool active = false;
    uint32_t serial = 0;
    int32_t last_state = -1;
    for(;;) {
        Record r;
        if(furi_message_queue_get(a->queue, &r, FuriWaitForever) != FuriStatusOk) continue;
        if(r.kind == 4) break;
        if(r.kind == 3) {
            atomic_store(&a->settings_error, !save_settings(s, &r.settings));
            continue;
        }
        if(r.kind == 1) {
            if(active) storage_file_close(f);
            active = open_log(s, f, r.stamp, &serial);
            last_state = -1;
            atomic_store(&a->state, active ? 1 : 2);
        }
        if(!active) {
            if(r.kind == 2) atomic_store(&a->state, 0);
            continue;
        }
        if(storage_file_size(f) >= ROTATE_SIZE) {
            storage_file_close(f);
            active = open_log(s, f, r.stamp, &serial);
            if(!active) {
                atomic_store(&a->state, 2);
                continue;
            }
        }
        const char* event = r.kind == 1                  ? "START" :
                            r.kind == 2                  ? "END" :
                            r.report.state != last_state ? "STATE" :
                                                           "SAMPLE";
        char row[220];
        int n = snprintf(
            row,
            sizeof(row),
            "1,%lu,%lu,%s,%lu,%ld,%ld,%ld,%ld,%lu,%lu,%lu,%lu,%lu\n",
            (unsigned long)r.stamp,
            (unsigned long)r.tick,
            event,
            (unsigned long)r.report.seq,
            (long)r.report.score,
            (long)r.report.threshold,
            (long)r.report.rssi,
            (long)r.report.state,
            (unsigned long)r.report.rate,
            (unsigned long)r.report.age,
            (unsigned long)r.status.quality,
            (unsigned long)r.status.automatic,
            (unsigned long)r.status.noise);
        last_state = r.report.state;
        bool ok = n > 0 && (size_t)n < sizeof(row) &&
                  storage_file_write(f, row, (size_t)n) == (size_t)n && storage_file_sync(f);
        if(!ok || r.kind == 2) {
            storage_file_close(f);
            active = false;
            atomic_store(&a->state, ok ? 0 : 2);
        }
    }
    if(active) {
        storage_file_sync(f);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return 0;
}
CsiStorage* csi_storage_alloc(void) {
    CsiStorage* a = malloc(sizeof(*a));
    furi_check(a);
    atomic_init(&a->state, 0);
    atomic_init(&a->lost, 0);
    atomic_init(&a->settings_error, false);
    a->queue = furi_message_queue_alloc(32, sizeof(Record));
    a->thread = furi_thread_alloc_ex("CsiSd", 3072, storage_worker, a);
    furi_thread_start(a->thread);
    return a;
}
void csi_storage_free(CsiStorage* a) {
    Record r = {.kind = 4};
    furi_message_queue_put(a->queue, &r, FuriWaitForever);
    furi_thread_join(a->thread);
    furi_thread_free(a->thread);
    furi_message_queue_free(a->queue);
    free(a);
}
static bool enqueue(CsiStorage* a, const Record* r) {
    if(furi_message_queue_put(a->queue, r, 0) == FuriStatusOk) return true;
    atomic_fetch_add(&a->lost, 1);
    return false;
}
bool csi_storage_settings(CsiStorage* a, const CsiSettings* settings) {
    Record r = {.kind = 3, .settings = *settings};
    return enqueue(a, &r);
}
bool csi_storage_log(
    CsiStorage* a,
    unsigned kind,
    const CsiReport* report,
    const CsiStatus* status) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    Record r = {
        .kind = kind,
        .stamp = datetime_datetime_to_timestamp(&dt),
        .tick = (uint32_t)((uint64_t)furi_get_tick() * 1000 / furi_kernel_get_tick_frequency()),
        .report = *report,
        .status = *status};
    unsigned old = atomic_load(&a->state);
    if(kind == 1) atomic_store(&a->state, 3);
    bool ok = enqueue(a, &r);
    if(!ok && kind == 1) atomic_store(&a->state, old);
    return ok;
}
unsigned csi_storage_state(CsiStorage* a) {
    return atomic_load(&a->state);
}
uint32_t csi_storage_lost(CsiStorage* a) {
    return atomic_load(&a->lost);
}
bool csi_storage_settings_error(CsiStorage* a) {
    return atomic_load(&a->settings_error);
}
