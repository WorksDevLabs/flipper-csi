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
    unsigned kind, filter;
    uint32_t stamp, tick;
    union {
        CsiUltimatePrefs ultimate;
        CsiSettings settings;
        struct {
            CsiReport report;
            CsiStatus status;
        };
        uint8_t session[CSI_SESSION_BYTES];
    };
} Record;
struct CsiStorage {
    FuriMutex* mutex;
    CsiCatalog catalog;
    FuriMessageQueue* queue;
    FuriThread* thread;
    atomic_uint state, lost;
    atomic_bool settings_error, ultimate_error, session_error;
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
        "schema,rtc_seconds,uptime_ms,event,seq,score_milli,threshold_milli,rssi,state,rate,age_ms,quality,auto,noise_milli,filter\n";
    if(opened && storage_file_write(f, header, sizeof(header) - 1) != sizeof(header) - 1) {
        storage_file_close(f);
        return false;
    }
    return opened;
}
static bool ultimate_io(Storage* s, CsiUltimatePrefs* prefs, bool write) {
    if(write && !storage_simply_mkdir(s, DATA)) return false;
    uint8_t bytes[CSI_ULTIMATE_BYTES];
    csi_ultimate_encode(prefs, bytes);
    File* f = storage_file_alloc(s);
    bool ok = false;
    if(write) {
        if(storage_file_open(f, DATA "/ultimate.tmp", FSAM_WRITE, FSOM_CREATE_ALWAYS))
            ok = storage_file_write(f, bytes, sizeof(bytes)) == sizeof(bytes) &&
                 storage_file_sync(f);
        storage_file_close(f);
        if(ok) {
            storage_common_remove(s, DATA "/ultimate.bak");
            if(storage_file_exists(s, DATA "/ultimate.bin"))
                ok = storage_common_rename(s, DATA "/ultimate.bin", DATA "/ultimate.bak") ==
                     FSE_OK;
            if(ok)
                ok = storage_common_rename(s, DATA "/ultimate.tmp", DATA "/ultimate.bin") ==
                     FSE_OK;
        }
    } else {
        const char* names[] = {DATA "/ultimate.bin", DATA "/ultimate.bak"};
        for(unsigned i = 0; i < 2 && !ok; i++) {
            if(storage_file_open(f, names[i], FSAM_READ, FSOM_OPEN_EXISTING)) {
                ok = storage_file_size(f) == sizeof(bytes) &&
                     storage_file_read(f, bytes, sizeof(bytes)) == sizeof(bytes) &&
                     csi_ultimate_decode(prefs, bytes, sizeof(bytes));
            }
            storage_file_close(f);
        }
    }
    storage_file_free(f);
    return ok;
}
bool csi_storage_ultimate_load(CsiUltimatePrefs* p) {
    csi_ultimate_default(p);
    Storage* s = furi_record_open(RECORD_STORAGE);
    bool ok = ultimate_io(s, p, false);
    furi_record_close(RECORD_STORAGE);
    if(!ok) csi_ultimate_default(p);
    return ok;
}
/* Same tmp -> bin, old bin -> bak rotation as the other preference files. */
static bool session_write(Storage* s, const uint8_t bytes[CSI_SESSION_BYTES]) {
    if(!storage_simply_mkdir(s, DATA)) return false;
    File* f = storage_file_alloc(s);
    bool ok = storage_file_open(f, DATA "/session.tmp", FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(f, bytes, CSI_SESSION_BYTES) == CSI_SESSION_BYTES &&
              storage_file_sync(f);
    storage_file_close(f);
    storage_file_free(f);
    if(!ok) return false;
    storage_common_remove(s, DATA "/session.bak");
    if(storage_file_exists(s, DATA "/session.bin") &&
       storage_common_rename(s, DATA "/session.bin", DATA "/session.bak") != FSE_OK)
        return false;
    if(storage_common_rename(s, DATA "/session.tmp", DATA "/session.bin") != FSE_OK) {
        (void)storage_common_rename(s, DATA "/session.bak", DATA "/session.bin");
        return false;
    }
    return true;
}
bool csi_storage_session_load(CsiSession* session) {
    memset(session, 0, sizeof(*session));
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    uint8_t bytes[CSI_SESSION_BYTES];
    uint32_t now_ms =
        (uint32_t)((uint64_t)furi_get_tick() * 1000 / furi_kernel_get_tick_frequency());
    uint32_t now_ts = furi_hal_rtc_get_timestamp();
    const char* names[] = {DATA "/session.bin", DATA "/session.bak"};
    bool ok = false;
    for(unsigned i = 0; i < 2 && !ok; i++) {
        if(storage_file_open(f, names[i], FSAM_READ, FSOM_OPEN_EXISTING))
            ok = storage_file_size(f) == sizeof(bytes) &&
                 storage_file_read(f, bytes, sizeof(bytes)) == sizeof(bytes) &&
                 csi_session_decode(session, bytes, sizeof(bytes), now_ms, now_ts);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
static void build_catalog(Storage* s, CsiCatalog* catalog) {
    catalog->count = 0;
    catalog->error = false;
    catalog->limited = false;
    File* dir = storage_file_alloc(s);
    FileInfo info;
    char name[64];
    unsigned visited = 0;
    if(!storage_dir_open(dir, LOGS)) {
        catalog->error = true;
        storage_file_free(dir);
        return;
    }
    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(++visited > 512) {
            catalog->limited = true;
            break;
        }
        size_t n = strlen(name);
        if((info.flags & FSF_DIRECTORY) || strncmp(name, "session-", 8) || n < 5 ||
           strcmp(name + n - 4, ".csv") || n >= 48)
            continue;
        unsigned pos = 0;
        while(pos < catalog->count && strcmp(catalog->items[pos].name, name) > 0)
            pos++;
        if(pos >= CSI_CATALOG_MAX) {
            catalog->limited = true;
            continue;
        }
        if(catalog->count < CSI_CATALOG_MAX)
            catalog->count++;
        else
            catalog->limited = true;
        for(unsigned j = catalog->count - 1; j > pos; j--)
            catalog->items[j] = catalog->items[j - 1];
        memset(&catalog->items[pos], 0, sizeof(catalog->items[pos]));
        snprintf(catalog->items[pos].name, 48, "%.47s", name);
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    File* f = storage_file_alloc(s);
    for(unsigned i = 0; i < catalog->count; i++) {
        char path[120];
        snprintf(path, sizeof(path), LOGS "/%s", catalog->items[i].name);
        if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            catalog->error = true;
            catalog->items[i].error = true;
            continue;
        }
        if(storage_file_size(f) > ROTATE_SIZE + 512) {
            catalog->error = true;
            catalog->items[i].error = true;
            storage_file_close(f);
            continue;
        }
        char chunk[256], line[220];
        unsigned used = 0;
        bool dropping = false, header = true;
        uint16_t n;
        while((n = storage_file_read(f, chunk, sizeof(chunk))) > 0) {
            for(unsigned j = 0; j < n; j++) {
                char c = chunk[j];
                if(c == '\r') continue;
                if(c == '\n') {
                    line[used] = 0;
                    bool ok = header ? !strncmp(line, "schema,rtc_seconds,uptime_ms,event,", 33) :
                                       !dropping && csi_catalog_row(&catalog->items[i], line);
                    if(!ok) {
                        catalog->items[i].rejected++;
                        catalog->items[i].summary.valid = false;
                    }
                    if(header && !ok) {
                        catalog->error = true;
                        catalog->items[i].error = true;
                        dropping = true;
                        break;
                    }
                    header = false;
                    used = 0;
                    dropping = false;
                } else if(used + 1 < sizeof(line) && !dropping)
                    line[used++] = c;
                else
                    dropping = true;
            }
            if(header && dropping) break;
        }
        if(storage_file_get_error(f) != FSE_OK || header) {
            catalog->error = true;
            catalog->items[i].error = true;
        }
        if(used || dropping) catalog->items[i].rejected++;
        storage_file_close(f);
    }
    storage_file_free(f);
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
        if(r.kind == 6) {
            atomic_store(&a->ultimate_error, !ultimate_io(s, &r.ultimate, true));
            continue;
        }
        if(r.kind == 7) {
            atomic_store(&a->session_error, !session_write(s, r.session));
            continue;
        }
        if(r.kind == 5) {
            CsiCatalog* catalog = malloc(sizeof(CsiCatalog));
            furi_check(catalog);
            memset(catalog, 0, sizeof(*catalog));
            if(active)
                catalog->error = true;
            else
                build_catalog(s, catalog);
            furi_mutex_acquire(a->mutex, FuriWaitForever);
            catalog->generation = a->catalog.generation + 1;
            a->catalog = *catalog;
            furi_mutex_release(a->mutex);
            free(catalog);
            continue;
        }
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
            "2,%lu,%lu,%s,%lu,%ld,%ld,%ld,%ld,%lu,%lu,%lu,%lu,%lu,%u\n",
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
            (unsigned long)r.status.noise,
            r.filter);
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
    a->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    memset(&a->catalog, 0, sizeof(a->catalog));
    atomic_init(&a->state, 0);
    atomic_init(&a->lost, 0);
    atomic_init(&a->settings_error, false);
    atomic_init(&a->ultimate_error, false);
    atomic_init(&a->session_error, false);
    a->queue = furi_message_queue_alloc(32, sizeof(Record));
    a->thread = furi_thread_alloc_ex("CsiSd", 4096, storage_worker, a);
    furi_thread_start(a->thread);
    return a;
}
void csi_storage_free(CsiStorage* a) {
    Record r = {.kind = 4};
    furi_message_queue_put(a->queue, &r, FuriWaitForever);
    furi_thread_join(a->thread);
    furi_thread_free(a->thread);
    furi_message_queue_free(a->queue);
    furi_mutex_free(a->mutex);
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
    const CsiStatus* status,
    unsigned filter) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    Record r = {
        .kind = kind,
        .filter = filter,
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
    return atomic_load(&a->settings_error) || atomic_load(&a->ultimate_error) ||
           atomic_load(&a->session_error);
}

bool csi_storage_catalog_request(CsiStorage* a) {
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    if(a->catalog.busy) {
        furi_mutex_release(a->mutex);
        return false;
    }
    a->catalog.busy = true;
    furi_mutex_release(a->mutex);
    Record r = {.kind = 5};
    bool ok = enqueue(a, &r);
    if(!ok) {
        furi_mutex_acquire(a->mutex, FuriWaitForever);
        a->catalog.busy = false;
        furi_mutex_release(a->mutex);
    }
    return ok;
}
void csi_storage_catalog(CsiStorage* a, CsiCatalog* out) {
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    *out = a->catalog;
    furi_mutex_release(a->mutex);
}
bool csi_storage_ultimate_save(CsiStorage* a, const CsiUltimatePrefs* p) {
    if(!csi_ultimate_valid(p)) return false;
    Record r = {.kind = 6, .ultimate = *p};
    return enqueue(a, &r);
}
bool csi_storage_session_save(CsiStorage* a, const CsiSession* session) {
    Record r = {.kind = 7};
    uint32_t now_ms =
        (uint32_t)((uint64_t)furi_get_tick() * 1000 / furi_kernel_get_tick_frequency());
    csi_session_encode(session, now_ms, furi_hal_rtc_get_timestamp(), r.session);
    return enqueue(a, &r);
}
