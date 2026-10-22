#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <expansion/expansion.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csi_pro.h"
#include "csi_storage.h"
#include "csi_session.h"
#include "csi_ultimate.h"
#include "csi_graph.h"
#define GRAPH_POINTS 100
static const uint32_t delays[] = {0, 5000, 15000};
#define RX_STOP      1U
#define VIEW_RADAR   0
#define VIEW_MENU    1
#define VIEW_EDITOR  2
#define EVENT_MENU   1000U
#define EVENT_EDITOR 2000U

typedef struct {
    uint8_t waterfall[CSI_WF_ROWS][CSI_WF_BINS], wave_head, wave_count;
    uint32_t wave_tick, perf[6], heap, heap_min, uart_rate, draw_max_ms;
    CsiSavedSession compare[2];
    bool compare_ready[2], alarm_armed, alarm_active;
    uint32_t alarm_wait;
    CsiUltimatePrefs ultimate;
    CsiSession session;
    bool started, starting, await_cal_frame;
    uint32_t graph_tick[GRAPH_POINTS];
    CsiReport report;
    CsiStatus status;
    CsiSettings prefs;
    CsiAccessPoint aps[CSI_AP_MAX];
    uint8_t ap_count;
    uint16_t graph[GRAPH_POINTS], head, count;
    uint32_t last_rx, last_stat, epoch, bad_frames, log_lost;
    bool received, status_received, hello, busy;
    unsigned page, log_state;
    bool prefs_error;
    char toast[32];
    uint32_t toast_tick;
} RadarModel;
typedef struct {
    char buffer[65];
    uint8_t size, min, max, key;
    bool masked;
    bool invalid;
} EditorModel;
typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    View* radar;
    View* editor;
    Submenu* menu;
    NotificationApp* notifications;
    Expansion* expansion;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* stream;
    FuriThread* worker;
    FuriMessageQueue* replies;
    CsiStorage* storage;
    atomic_uint rx_bytes;
    uint32_t rate_tick, rate_bytes;
    atomic_bool rx_lost;
    CsiUltimatePrefs ultimate;
    CsiAlarm alarm;
    CsiCatalog* catalog;
    unsigned compare_slot;
    uint32_t catalog_generation;
    bool browsing, filter_synced, wave_enabled, wave_target;
    bool started, start_requested;
    CsiSettings settings, desired;
    bool loaded, settings_pending, was_moving, logging, synced;
    char ssid[33], password[64], pending[12];
    uint32_t sent_tick, last_ping, last_log, last_epoch, scan_tick;
    unsigned editing, menu_kind, sync_attempts;
    bool in_menu, scanning, dirty;
    uint32_t dirty_tick;
    int32_t log_previous;
} RadarApp;
static uint32_t milliseconds(uint32_t ticks) {
    return (uint32_t)((uint64_t)ticks * 1000U / furi_kernel_get_tick_frequency());
}
static bool recent(uint32_t tick, uint32_t ms) {
    return milliseconds(furi_get_tick() - tick) < ms;
}
static const char* tr(RadarApp* a, const char* en, const char* turkish) {
    return a->settings.language ? turkish : en;
}
static void snapshot(RadarApp* a, RadarModel* out) {
    with_view_model(a->radar, RadarModel * m, { *out = *m; }, false);
}
static bool ready(const RadarModel* m) {
    return m->hello && m->received && m->status_received && recent(m->last_rx, 2000) &&
           recent(m->last_stat, 2500);
}
static void toast(RadarApp* a, const char* msg) {
    with_view_model(
        a->radar,
        RadarModel * m,
        {
            snprintf(m->toast, sizeof(m->toast), "%s", msg);
            m->toast_tick = furi_get_tick();
        },
        true);
}
static void mark_settings(RadarApp* a) {
    a->dirty = true;
    a->dirty_tick = furi_get_tick();
}
static bool tx(RadarApp* a, const char* payload) {
    if(!a->serial) return false;
    char frame[CSI_LINE_CAP];
    if(!csi_frame(frame, sizeof(frame), payload)) return false;
    furi_hal_serial_tx(a->serial, (const uint8_t*)frame, strlen(frame));
    furi_hal_serial_tx_wait_complete(a->serial);
    memset(frame, 0, sizeof(frame));
    return true;
}
static bool send_command(RadarApp* a, const char* payload, const char* name) {
    RadarModel m;
    snapshot(a, &m);
    if(!ready(&m)) {
        toast(a, tr(a, "ESP firmware 2.0 needed", "ESP 2.0 / baglanti gerekli"));
        return false;
    }
    if(a->pending[0]) {
        toast(a, tr(a, "Waiting for ESP32", "ESP yaniti bekleniyor"));
        return false;
    }
    if(!tx(a, payload)) {
        toast(a, tr(a, "UART unavailable", "UART kullanilamiyor"));
        return false;
    }
    snprintf(a->pending, sizeof(a->pending), "%s", name);
    a->sent_tick = furi_get_tick();
    toast(a, tr(a, "Sending...", "Gonderiliyor..."));
    return true;
}
static bool configure(RadarApp* a, const CsiSettings* settings) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "CFG %u %u", settings->threshold, settings->automatic ? 1 : 0);
    if(!send_command(a, cmd, "CFG")) return false;
    a->desired = *settings;
    a->settings_pending = true;
    return true;
}
static void rx_callback(FuriHalSerialHandle* serial, FuriHalSerialRxEvent event, void* ctx) {
    RadarApp* a = ctx;
    if(event & (FuriHalSerialRxEventFrameError | FuriHalSerialRxEventNoiseError |
                FuriHalSerialRxEventOverrunError | FuriHalSerialRxEventParityError))
        atomic_store(&a->rx_lost, true);
    if(event & FuriHalSerialRxEventData) {
        atomic_fetch_add(&a->rx_bytes, 1);
        uint8_t b = furi_hal_serial_async_rx(serial);
        if(furi_stream_buffer_send(a->stream, &b, 1, 0) != 1) atomic_store(&a->rx_lost, true);
    }
}
static int32_t worker(void* ctx) {
    RadarApp* a = ctx;
    CsiLine line = {0};
    while(!(furi_thread_flags_get() & RX_STOP)) {
        uint8_t b;
        if(!furi_stream_buffer_receive(a->stream, &b, 1, furi_ms_to_ticks(50))) continue;
        if(atomic_exchange(&a->rx_lost, false)) {
            line.dropping = true;
            line.size = 0;
        }
        if(!csi_line_feed(&line, b)) continue;
        char* p = csi_unframe(line.data);
        CsiReport r;
        CsiStatus status;
        CsiAccessPoint ap;
        uint32_t index;
        uint8_t wave[CSI_WF_BINS];
        uint32_t perf[6];
        if(p && csi_report_parse(p, &r)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    if(m->received && (r.seq < m->report.seq || !recent(m->last_rx, 2000))) {
                        m->head = m->count = 0;
                        m->started = false;
                        m->await_cal_frame = false;
                        m->epoch++;
                        m->hello = false;
                        m->status_received = false;
                    }
                    if(r.state == 0 || r.state == 3) m->await_cal_frame = false;
                    csi_session_feed(
                        &m->session,
                        milliseconds(furi_get_tick()),
                        r.state,
                        r.score,
                        csi_measurement_ready(m->started, m->await_cal_frame, r.age, 0) &&
                            (r.state == 1 || r.state == 2));
                    m->report = r;
                    m->received = true;
                    m->last_rx = furi_get_tick();
                    m->graph_tick[m->head] = furi_get_tick();
                    m->graph[m->head] = m->started && !m->await_cal_frame &&
                                                (r.state == 1 || r.state == 2) && r.age < 2000 ?
                                            (uint16_t)r.score :
                                            UINT16_MAX;
                    m->head = (m->head + 1) % GRAPH_POINTS;
                    if(m->count < GRAPH_POINTS) m->count++;
                },
                false);
        } else if(p && csi_perf_parse(p, perf)) {
            with_view_model(
                a->radar, RadarModel * m, { memcpy(m->perf, perf, sizeof(perf)); }, false);
        } else if(p && csi_wave_parse(p, wave)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    memcpy(m->waterfall[m->wave_head], wave, CSI_WF_BINS);
                    m->wave_head = (m->wave_head + 1) % CSI_WF_ROWS;
                    if(m->wave_count < CSI_WF_ROWS) m->wave_count++;
                    m->wave_tick = furi_get_tick();
                },
                false);
        } else if(p && csi_status_parse(p, &status)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    m->status = status;
                    m->status_received = true;
                    m->last_stat = furi_get_tick();
                },
                false);
        } else if(p && !strncmp(p, "HELLO,", 6)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                { m->hello = !strcmp(p, "HELLO,2.0.0,3,ESP32-S2"); },
                false);
        } else if(p && csi_ap_parse(p, &index, &ap)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    m->aps[index] = ap;
                    if(m->ap_count <= index) m->ap_count = index + 1;
                },
                false);
        } else if(
            p && (!strncmp(p, "ACK,", 4) || !strncmp(p, "ERR,", 4) || !strncmp(p, "EV,", 3) ||
                  !strncmp(p, "APEND,", 6))) {
            char reply[96];
            snprintf(reply, sizeof(reply), "%s", p);
            if(furi_message_queue_put(a->replies, reply, 0) != FuriStatusOk)
                atomic_store(&a->rx_lost, true);
        } else
            with_view_model(a->radar, RadarModel * m, { m->bad_frames++; }, false);
    }
    return 0;
}
/* Keep even long counters and translated labels inside the physical display. */
static void draw_text(Canvas* c, int x, int y, const char* text) {
    if(x < 0 || x >= 128) return;
    char clipped[96];
    snprintf(clipped, sizeof(clipped), "%s", text);
    size_t n = strlen(clipped);
    if(canvas_string_width(c, clipped) > 128 - x) {
        while(n && canvas_string_width(c, clipped) + canvas_string_width(c, "..") > 128 - x)
            clipped[--n] = 0;
        if(n + 2 < sizeof(clipped) && canvas_string_width(c, "..") <= 128 - x)
            strcat(clipped, "..");
    }
    canvas_draw_str(c, x, y, clipped);
}
static void draw_footer(Canvas* c, bool turkish, bool calibrate) {
    canvas_set_font(c, FontSecondary);
    elements_button_left(c, "Menu");
    elements_button_center(c, calibrate ? (turkish ? "Kal" : "Cal") : "Menu");
    elements_button_right(c, turkish ? "Mod" : "Mode");
}
static void small_heading(Canvas* c, int x, int y, const char* text, bool centered) {
    canvas_set_font(c, FontSecondary);
    char b[64];
    snprintf(b, sizeof(b), "%s", text);
    size_t n = strlen(b);
    int width = centered ? 123 : 88;
    while(n && canvas_string_width(c, b) + 1 > width)
        b[--n] = 0;
    if(centered) x = (128 - canvas_string_width(c, b) - 1) / 2;
    canvas_draw_str(c, x, y, b);
    canvas_draw_str(c, x + 1, y, b);
}
static void draw_cell(Canvas* c, int x, int y, unsigned width, const char* text) {
    char b[64];
    snprintf(b, sizeof(b), "%s", text);
    size_t n = strlen(b);
    while(n && canvas_string_width(c, b) > width)
        b[--n] = 0;
    canvas_draw_str(c, x, y, b);
}
static void center_cell(Canvas* c, int x, int y, unsigned width, const char* text) {
    char b[64];
    snprintf(b, sizeof(b), "%s", text);
    size_t n = strlen(b);
    while(n && canvas_string_width(c, b) > width)
        b[--n] = 0;
    canvas_draw_str(c, x + (width - canvas_string_width(c, b)) / 2, y, b);
}
static unsigned table_y(unsigned row) {
    return 19 + (row * 38) / 4;
}
static void table_frame(Canvas* c) {
    canvas_draw_frame(c, 2, 12, 124, 39);
    for(unsigned i = 1; i < 4; i++) {
        int y = 12 + i * 38 / 4;
        canvas_draw_line(c, 2, y, 125, y);
    }
}
static void draw_rows(Canvas* c, const char* labels[4], char values[4][32]) {
    canvas_set_font(c, FontSecondary);
    table_frame(c);
    canvas_draw_line(c, 76, 12, 76, 50);
    for(unsigned i = 0; i < 4; i++) {
        center_cell(c, 4, table_y(i), 69, labels[i]);
        center_cell(c, 79, table_y(i), 44, values[i]);
    }
}
static void signal_header(Canvas* c, const CsiReport* r, bool linked) {
    canvas_set_font(c, FontSecondary);
    unsigned bars = !linked ? 0 : r->rssi >= -55 ? 4 : r->rssi >= -67 ? 3 : r->rssi >= -80 ? 2 : 1;
    for(unsigned i = 0; i < 4; i++) {
        int h = 1 + i * 2;
        if(i < bars)
            canvas_draw_box(c, 94 + i * 3, 9 - h, 2, h);
        else
            canvas_draw_dot(c, 94 + i * 3, 8);
    }
    char b[8];
    if(linked)
        snprintf(b, sizeof(b), "%ld", (long)r->rssi);
    else
        snprintf(b, sizeof(b), "--");
    canvas_draw_str_aligned(c, 127, 9, AlignRight, AlignBottom, b);
}
static void draw_monitor(Canvas* c, RadarModel* m, bool linked, bool fresh, const char* label) {
    bool tr = m->prefs.language;
    CsiReport* r = &m->report;
    char b[48], range[24] = "10 s";
    canvas_set_font(c, FontSecondary);
    small_heading(
        c,
        2,
        8,
        !m->started ?
            (m->starting ? (tr ? "HAZIRLANIYOR" : "STARTING") : (tr ? "HAZIR" : "READY")) :
        m->starting ? (tr ? "KALIBRE" : "CALIBRATING") :
                      label,
        false);
    signal_header(c, r, linked && r->state != 5 && r->state != 6 && r->rssi < 0);
    canvas_draw_frame(c, 1, 12, 126, 29);
    canvas_set_font(c, FontSecondary);
    if(!linked || !m->hello || r->state >= 4) {
        draw_text(c, 7, 27, tr ? "Kart / WiFi verisi bekleniyor" : "Waiting for board / WiFi");
        draw_text(c, 7, 36, tr ? "Menu > WiFi kurulumu" : "Menu > WiFi setup");
    } else if(!m->started && !m->starting) {
        draw_text(c, 7, 27, tr ? "Cihazi sabit yerlestir" : "Place the device still");
        draw_text(c, 7, 36, tr ? "OK ile kalibrasyonu baslat" : "Press OK to calibrate");
    } else if(m->starting || (fresh && (r->state == 0 || r->state == 3))) {
        snprintf(
            b,
            sizeof(b),
            tr ? "Ogreniyor %lu%%" : "Learning %lu%%",
            (unsigned long)(m->starting ? 0 : r->progress));
        draw_text(c, 7, 27, b);
        draw_text(c, 7, 36, tr ? "Lutfen hareket etme" : "Please remain still");
    } else if(!linked || !m->hello || !fresh || r->state >= 4) {
        draw_text(c, 7, 27, tr ? "Gecerli veri bekleniyor" : "Waiting for valid data");
        draw_text(c, 7, 36, tr ? "Menu > WiFi / Baglanti" : "Menu > WiFi / Connection");
    } else {
        uint32_t peak = 0;
        for(unsigned i = 0; i < m->count; i++) {
            unsigned idx = (m->head + GRAPH_POINTS - m->count + i) % GRAPH_POINTS;
            uint16_t v = m->graph[idx];
            if(milliseconds(furi_get_tick() - m->graph_tick[idx]) <= 10000 && v != UINT16_MAX &&
               v > peak)
                peak = v;
        }
        uint32_t ymax = csi_graph_scale((uint32_t)r->threshold, peak);
        snprintf(
            range,
            sizeof(range),
            "Y%lu.%03lu 10s",
            (unsigned long)(ymax / 1000),
            (unsigned long)(ymax % 1000));
        int ty = csi_graph_y((uint32_t)r->threshold, ymax);
        for(int x = 4; x <= 123; x += 4)
            canvas_draw_dot(c, x, ty);
        int px = -1, py = 0;
        for(unsigned i = 0; i < m->count; i++) {
            unsigned idx = (m->head + GRAPH_POINTS - m->count + i) % GRAPH_POINTS;
            uint16_t v = m->graph[idx];
            uint32_t age = milliseconds(furi_get_tick() - m->graph_tick[idx]);
            if(v == UINT16_MAX || age > 10000) {
                px = -1;
                continue;
            }
            int x = 4 + (10000 - age) * 119 / 10000, y = csi_graph_y(v, ymax);
            if(px >= 0) canvas_draw_line(c, px, py, x, y);
            px = x;
            py = y;
        }
    }
    canvas_set_font(c, FontSecondary);
    if(m->started && fresh && !m->starting && (r->state == 1 || r->state == 2)) {
        unsigned score = (unsigned)(r->score + 5) / 10;
        snprintf(b, sizeof(b), "%u.%02u", score / 100, score % 100);
    } else
        snprintf(b, sizeof(b), "--");
    bool show_toast = m->toast[0] && recent(m->toast_tick, 2200);
    if(!show_toast) draw_text(c, 3, 49, b);
    const char* status = m->prefs_error    ? (tr ? "SD AYAR HATASI" : "SD SETTINGS ERROR") :
                         m->log_state == 2 ? "SD HATA" :
                         m->alarm_armed    ? (tr ? "ALARM ACIK" : "ALARM ON") :
                         m->prefs.muted    ? (tr ? "SESSIZ" : "MUTED") :
                         m->log_state == 1 ? (tr ? "KAYIT" : "REC") :
                                             range;
    if(!show_toast) canvas_draw_str_aligned(c, 125, 49, AlignRight, AlignBottom, status);
    if(m->toast[0] && recent(m->toast_tick, 2200)) draw_text(c, 2, 49, m->toast);
    draw_footer(c, tr, true);
}
static void draw_content(Canvas* c, void* ctx) {
    RadarModel* m = ctx;
    CsiReport* r = &m->report;
    bool tr = m->prefs.language;
    bool linked = m->received && recent(m->last_rx, 2000);
    bool fresh = linked && csi_fresh(r->age, milliseconds(furi_get_tick() - m->last_rx));
    const char* en[] = {
        "LEARNING", "STILL", "MOVING", "CALIBRATE", "NO CSI", "WIFI OFF", "SETUP WIFI"};
    const char* tu[] = {
        "OGRENIYOR", "DURGUN", "HAREKET", "KALIBRE", "CSI YOK", "WIFI YOK", "WIFI KUR"};
    const char* label =
        !m->started ? (m->starting ? (tr ? "KALIBRE" : "CALIBRATING") : (tr ? "HAZIR" : "READY")) :
        m->alarm_active && fresh ? "ALARM" :
        m->busy                  ? "UART BUSY" :
        !linked                  ? (tr ? "BAGLANTI YOK" : "NO LINK") :
        r->state <= 3 && !fresh  ? (tr ? "CSI YOK" : "NO CSI") :
                                   (tr ? tu[r->state] : en[r->state]);
    canvas_clear(c);
    canvas_set_font(c, FontSecondary);
    if(m->page == 0) {
        draw_monitor(c, m, linked, fresh, label);
        return;
    }
    const char* title = m->page == 1  ? (tr ? "BAGLANTI" : "CONNECTION") :
                        m->page == 2  ? "CSI / SAURONLAB" :
                        m->page == 3  ? (tr ? "KULLANIM" : "CONTROLS") :
                        m->page == 4  ? (tr ? "OTURUM OZETI" : "SESSION SUMMARY") :
                        m->page == 5  ? (tr ? "GECMIS / UPTIME" : "HISTORY / UPTIME") :
                        m->page == 6  ? "WATERFALL" :
                        m->page == 7  ? (tr ? "OTURUMLAR / A-B" : "SESSIONS / A-B") :
                        m->page == 12 ? (tr ? "A-B / KAYIT BILGISI" : "A-B / SESSION INFO") :
                        m->page == 8  ? (tr ? "KOLAY KURULUM" : "QUICK SETUP") :
                        m->page == 9  ? "ALARM" :
                        m->page == 10 ? (tr ? "SISTEM KAYNAKLARI" : "SYSTEM RESOURCES") :
                                        (tr ? "OLCUM TANILARI" : "MEASUREMENT DIAGNOSTICS");
    small_heading(c, 2, 8, title, true);
    canvas_draw_line(c, 0, 10, 127, 10);
    char v[4][32] = {{0}};
    const char* labels[4] = {"", "", "", ""};
    char b[64];
    if(m->page == 6) {
        canvas_draw_frame(c, 1, 12, 96, 39);
        canvas_draw_frame(c, 99, 12, 28, 39);
        canvas_draw_line(c, 99, 25, 126, 25);
        canvas_draw_line(c, 99, 38, 126, 38);
        if(!m->started || m->starting || !fresh || !recent(m->wave_tick, 2000)) {
            draw_cell(c, 5, 28, 88, tr ? "Veri bekleniyor" : "Waiting for data");
            draw_cell(c, 5, 41, 88, tr ? "OK: kalibre et" : "OK: calibrate");
        } else {
            unsigned shown = m->wave_count > 16 ? 16 : m->wave_count;
            for(unsigned y = 0; y < shown; y++) {
                unsigned row = (m->wave_head + CSI_WF_ROWS - shown + y) % CSI_WF_ROWS;
                for(unsigned x = 0; x < CSI_WF_BINS; x++) {
                    uint8_t value = m->waterfall[row][x];
                    unsigned level = value < 32 ? 0 : value < 64 ? 1 : value < 128 ? 2 : 3;
                    for(unsigned dy = 0; dy < 2; dy++)
                        for(unsigned dx = 0; dx < 2; dx++)
                            if(level == 3 || (level == 2 && dx == dy) ||
                               (level == 1 && !dx && !dy))
                                canvas_draw_dot(c, 2 + x * 2 + dx, 15 + y * 2 + dy);
                }
            }
        }
        draw_text(
            c,
            101,
            21,
            canvas_string_width(c, "BIN") + canvas_string_width(c, "47") <= 22 ? "BIN" : "N");
        canvas_draw_str_aligned(c, 125, 21, AlignRight, AlignBottom, "47");
        draw_text(c, 101, 34, "Hz");
        canvas_draw_str_aligned(c, 125, 34, AlignRight, AlignBottom, "4");
        draw_text(c, 101, 47, "s");
        canvas_draw_str_aligned(c, 125, 47, AlignRight, AlignBottom, "4");
        draw_footer(c, tr, m->page == 6 || m->page == 8);
        return;
    }
    if(m->page == 4) {
        labels[0] = tr ? "Hareket olayi" : "Motion starts";
        labels[1] = tr ? "Gecerli sure" : "Valid time";
        labels[2] = tr ? "Hareket suresi" : "Moving time";
        labels[3] = tr ? "En yuksek skor" : "Peak score";
        snprintf(v[0], 32, "%lu", (unsigned long)m->session.events);
        snprintf(v[1], 32, "%lu s", (unsigned long)(m->session.valid_ms / 1000));
        snprintf(v[2], 32, "%lu s", (unsigned long)(m->session.moving_ms / 1000));
        snprintf(
            v[3],
            32,
            "%lu.%03lu",
            (unsigned long)(m->session.peak / 1000),
            (unsigned long)(m->session.peak % 1000));
    } else if(m->page == 5) {
        table_frame(c);
        canvas_draw_line(c, 17, 12, 17, 50);
        canvas_draw_line(c, 73, 12, 73, 50);
        for(unsigned i = 0; i < 4; i++) {
            unsigned y = table_y(i);
            snprintf(b, sizeof(b), "%u", i + 1);
            center_cell(c, 3, y, 12, b);
            if(i < m->session.count) {
                unsigned idx = (m->session.head + CSI_EVENTS - 1 - i) % CSI_EVENTS;
                uint32_t sec = m->session.times[idx] / 1000;
                snprintf(
                    b,
                    sizeof(b),
                    "%02lu:%02lu:%02lu",
                    (unsigned long)(sec / 3600),
                    (unsigned long)(sec / 60 % 60),
                    (unsigned long)(sec % 60));
                center_cell(c, 20, y, 50, b);
                center_cell(c, 77, y, 46, tr ? "Hareket" : "Moving");
            } else {
                center_cell(c, 20, y, 50, "--:--:--");
                center_cell(c, 77, y, 46, "--");
            }
        }
        draw_footer(c, tr, m->page == 6 || m->page == 8);
        return;
    } else if(m->page == 1) {
        labels[0] = tr ? "Sinyal" : "Signal";
        labels[1] = tr ? "Veri kalitesi" : "Data quality";
        labels[2] = tr ? "Ornek hizi" : "Sample rate";
        labels[3] = tr ? "Filtre / esik" : "Filter / mode";
        if(linked && r->rssi < 0)
            snprintf(v[0], 32, "%ld dBm", (long)r->rssi);
        else
            snprintf(v[0], 32, "--");
        snprintf(v[1], 32, "%lu/100", (unsigned long)(fresh ? m->status.quality : 0));
        snprintf(v[2], 32, "%lu Hz", (unsigned long)r->rate);
        snprintf(
            v[3], 32, "F%lu / %s", (unsigned long)m->perf[3], m->status.automatic ? "AUTO" : "MAN");
    } else if(m->page == 11) {
        labels[0] = tr ? "ESP / UART kayip" : "ESP / UART loss";
        labels[1] = tr ? "SD / WiFi hata" : "SD / WiFi error";
        labels[2] = tr ? "Kalibrasyon hata" : "Calibration error";
        labels[3] = tr ? "Gurultu" : "Noise";
        snprintf(v[0], 32, "%lu/%lu", (unsigned long)r->drops, (unsigned long)m->bad_frames);
        snprintf(v[1], 32, "%lu/%lu", (unsigned long)m->log_lost, (unsigned long)m->status.reason);
        snprintf(v[2], 32, "%lu", (unsigned long)m->status.cal_error);
        snprintf(v[3], 32, "%lu", (unsigned long)m->status.noise);
    } else if(m->page == 10) {
        labels[0] = "F7 RAM/min";
        labels[1] = "ESP RAM/Q";
        labels[2] = "DSP max";
        labels[3] = "UART/UI";
        snprintf(
            v[0],
            32,
            "%lu/%lu K",
            (unsigned long)(m->heap / 1024),
            (unsigned long)(m->heap_min / 1024));
        snprintf(
            v[1], 32, "%luK/%lu", (unsigned long)(m->perf[0] / 1024), (unsigned long)m->perf[2]);
        snprintf(v[2], 32, "%lu us", (unsigned long)m->perf[1]);
        snprintf(v[3], 32, "%lu/%lu", (unsigned long)m->uart_rate, (unsigned long)m->draw_max_ms);
    } else if(m->page == 2) {
        labels[0] = tr ? "Uygulama" : "Application";
        labels[1] = "Flipper";
        labels[2] = tr ? "Kart" : "Board";
        labels[3] = "ESP / Link";
        snprintf(v[0], 32, "2.0 Compact");
        snprintf(v[1], 32, "E11 / 87.1");
        snprintf(v[2], 32, "ESP32-S2");
        snprintf(v[3], 32, "P3 / %s", m->hello ? "OK" : "--");
    } else if(m->page == 7) {
        table_frame(c);
        canvas_draw_line(c, 55, 12, 55, 50);
        canvas_draw_line(c, 91, 12, 91, 50);
        center_cell(c, 4, table_y(0), 48, tr ? "Olcum" : "Metric");
        center_cell(c, 58, table_y(0), 31, "A");
        center_cell(c, 94, table_y(0), 29, "B");
        const char* rownames[3] = {
            tr ? "Olay" : "Events", tr ? "Gecerli s" : "Valid s", tr ? "Hareket s" : "Moving s"};
        for(unsigned row = 0; row < 3; row++) {
            unsigned y = table_y(row + 1);
            center_cell(c, 4, y, 48, rownames[row]);
            for(unsigned i = 0; i < 2; i++) {
                if(!m->compare_ready[i])
                    snprintf(b, sizeof(b), "--");
                else {
                    const CsiSession* z = &m->compare[i].summary;
                    snprintf(
                        b,
                        sizeof(b),
                        "%lu",
                        (unsigned long)(row == 0 ? z->events :
                                        row == 1 ? z->valid_ms / 1000 :
                                                   z->moving_ms / 1000));
                }
                center_cell(c, 58 + i * 36, y, 29, b);
            }
        }

        draw_footer(c, tr, m->page == 6 || m->page == 8);
        return;
    } else if(m->page == 12) {
        labels[0] = "A / ID";
        labels[1] = "B / ID";
        labels[2] = tr ? "A filtre/hata" : "A filter/errors";
        labels[3] = tr ? "B filtre/hata" : "B filter/errors";
        for(unsigned i = 0; i < 2; i++) {
            if(!m->compare_ready[i]) {
                snprintf(v[i], 32, "--");
                snprintf(v[i + 2], 32, "--");
                continue;
            }
            const CsiSavedSession* z = &m->compare[i];
            if(strlen(z->name) >= 21)
                snprintf(v[i], 32, "%.4s/%.4s", z->name + 12, z->name + 17);
            else
                snprintf(v[i], 32, "--");
            snprintf(
                v[i + 2],
                32,
                "%s/%lu",
                z->filters == 1 ? "F0" :
                z->filters == 2 ? "F1" :
                                  "F?",
                (unsigned long)z->rejected);
        }
    } else if(m->page == 9) {
        labels[0] = tr ? "Durum" : "State";
        labels[1] = tr ? "Baslama" : "Start delay";
        labels[2] = tr ? "Titresim" : "Vibration";
        labels[3] = tr ? "Olcum" : "Data";
        snprintf(
            v[0],
            32,
            "%s",
            !m->alarm_armed ? (tr ? "Kapali" : "OFF") :
            m->alarm_active ? (tr ? "Hareket" : "Motion") :
                              (tr ? "Acik" : "ON"));
        snprintf(v[1], 32, "%lu s", (unsigned long)m->alarm_wait);
        snprintf(v[2], 32, "%s", m->prefs.muted ? (tr ? "Kapali" : "OFF") : (tr ? "Acik" : "ON"));
        snprintf(
            v[3],
            32,
            "%s",
            fresh && m->status.quality ? (tr ? "Hazir" : "Ready") : (tr ? "Bekliyor" : "Waiting"));
        draw_rows(c, labels, v);
        canvas_draw_line(c, 0, 56, 127, 56);
        draw_cell(
            c,
            1,
            64,
            125,
            m->toast[0] && recent(m->toast_tick, 2200) ? m->toast :
            m->alarm_armed ? (tr ? "OK: Kapat  >:Titresim" : "OK: OFF  >:Vibration") :
                             (tr ? "OK: Ac  >:Titresim" : "OK: ON  >:Vibration"));
        return;
    } else {
        const char* help_tr[] = {
            "< Menu   > Ekran degistir",
            "OK: kalibrasyonu baslat",
            "Uzun >: titresim ac/kapat",
            "Sabit koy, ogrenmeyi bekle",
            "Alarm: Menu > Alarm"};
        const char* help_en[] = {
            "< Menu   > Next screen",
            "OK: start calibration",
            "Hold >: vibration toggle",
            "Place still, wait for learning",
            "Alarm: Menu > Alarm"};
        const char* setup_tr[] = {
            "1  Menu > WiFi kurulumu",
            "2  Cihazi sabit yerlestir",
            "3  OK ile kalibre et",
            "4  Yuru / dur, sonucu dene",
            "Geri: ana ekrana don"};
        const char* setup_en[] = {
            "1  Menu > WiFi setup",
            "2  Place the device still",
            "3  Press OK to calibrate",
            "4  Walk / stop, verify",
            "Back: return to dashboard"};
        for(unsigned i = 0; i < 5; i++)
            draw_cell(
                c,
                3,
                18 + i * 8,
                122,
                m->page == 8 ? (tr ? setup_tr[i] : setup_en[i]) : (tr ? help_tr[i] : help_en[i]));
        draw_footer(c, tr, m->page == 6 || m->page == 8);
        return;
    }
    draw_rows(c, labels, v);
    draw_footer(c, tr, m->page == 6 || m->page == 8);
}
static void draw(Canvas* c, void* ctx) {
    uint32_t start = furi_get_tick();
    draw_content(c, ctx);
    RadarModel* m = ctx;
    uint32_t elapsed = milliseconds(furi_get_tick() - start);
    if(elapsed > m->draw_max_ms) m->draw_max_ms = elapsed;
}
/* Fixed ASCII grid includes SPACE at key 0; password is always masked. */
static void editor_draw(Canvas* c, void* ctx) {
    EditorModel* m = ctx;
    canvas_clear(c);
    canvas_set_font(c, FontSecondary);
    char shown[23] = {0};
    unsigned start = m->size > 20 ? m->size - 20 : 0;
    for(unsigned i = start; i < m->size; i++)
        shown[i - start] = m->masked ? '*' : m->buffer[i];
    draw_text(c, 1, 8, shown);
    for(unsigned key = 0; key < 96; key++) {
        int x = (key % 16) * 8, y = 11 + (key / 16) * 7;
        if(key == m->key) {
            canvas_draw_box(c, x, y, 8, 7);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_glyph(c, x + 1, y + 6, key == 95 ? '#' : key == 0 ? '_' : (char)(32 + key));
        if(key == m->key) canvas_set_color(c, ColorBlack);
    }
    draw_text(
        c,
        1,
        63,
        m->invalid  ? (m->masked ? "Min 8 characters required" : "SSID required") :
        m->key == 0 ? "SPACE  Hold OK:save" :
                      "OK:add Back:del Hold OK:save");
}
static bool editor_input(InputEvent* e, void* ctx) {
    RadarApp* a = ctx;
    if(e->type == InputTypeLong && e->key == InputKeyBack) {
        view_dispatcher_send_custom_event(a->dispatcher, EVENT_EDITOR + 1);
        return true;
    }
    if(e->type == InputTypeLong && e->key == InputKeyOk) {
        view_dispatcher_send_custom_event(a->dispatcher, EVENT_EDITOR);
        return true;
    }
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return true;
    bool finish = false, cancel = false;
    with_view_model(
        a->editor,
        EditorModel * m,
        {
            m->invalid = false;
            switch(e->key) {
            case InputKeyUp:
                m->key = (m->key + 80) % 96;
                break;
            case InputKeyDown:
                m->key = (m->key + 16) % 96;
                break;
            case InputKeyLeft:
                m->key = (m->key + 95) % 96;
                break;
            case InputKeyRight:
                m->key = (m->key + 1) % 96;
                break;
            case InputKeyBack:
                if(m->size)
                    m->buffer[--m->size] = 0;
                else
                    cancel = true;
                break;
            case InputKeyOk:
                if(e->type == InputTypeShort) {
                    if(m->key == 95)
                        finish = true;
                    else if(m->size < m->max) {
                        m->buffer[m->size++] = (char)(32 + m->key);
                        m->buffer[m->size] = 0;
                    }
                }
                break;
            default:
                break;
            }
        },
        true);
    if(finish) view_dispatcher_send_custom_event(a->dispatcher, EVENT_EDITOR);
    if(cancel) view_dispatcher_send_custom_event(a->dispatcher, EVENT_EDITOR + 1);
    return true;
}
static void edit_wifi(RadarApp* a, unsigned stage) {
    a->editing = stage;
    a->in_menu = false;
    with_view_model(
        a->editor,
        EditorModel * m,
        {
            memset(m, 0, sizeof(*m));
            m->masked = stage == 2;
            m->max = stage == 1 ? 32 : 63;
            m->min = stage == 1 ? 1 : 8;
            if(stage == 1) {
                snprintf(m->buffer, sizeof(m->buffer), "%s", a->ssid);
                m->size = strlen(m->buffer);
            }
        },
        true);
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_EDITOR);
}
static void menu_callback(void* ctx, uint32_t index) {
    RadarApp* a = ctx;
    view_dispatcher_send_custom_event(a->dispatcher, EVENT_MENU + index);
}
static void request_calibration(RadarApp* a) {
    RadarModel m;
    snapshot(a, &m);
    if(!ready(&m) || m.report.state >= 4 ||
       !csi_fresh(m.report.age, milliseconds(furi_get_tick() - m.last_rx))) {
        toast(a, tr(a, "Connect WiFi / board first", "Once kart ve WiFi baglanmali"));
        return;
    }
    if(a->pending[0]) {
        toast(a, tr(a, "Wait for board reply", "Kart yanitini bekleyin"));
        return;
    }
    a->started = false;
    a->start_requested = true;
    a->sync_attempts = 0;
    memset(&a->alarm, 0, sizeof(a->alarm));
    with_view_model(
        a->radar,
        RadarModel * v,
        {
            v->started = false;
            v->starting = true;
            v->head = v->count = v->wave_count = v->wave_head = 0;
        },
        false);
}
static void menu_begin(RadarApp* a, const char* en, const char* turkish, unsigned kind) {
    submenu_reset(a->menu);
    submenu_set_header(a->menu, tr(a, en, turkish));
    a->menu_kind = kind;
    a->in_menu = true;
}
static void menu_item(RadarApp* a, const char* en, const char* turkish, unsigned id) {
    submenu_add_item(a->menu, tr(a, en, turkish), id, menu_callback, a);
}
static void menu_open(RadarApp* a) {
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_MENU);
}
static void show_settings(RadarApp* a) {
    menu_begin(a, "CSI RADAR / MENU", "CSI RADAR / MENU", 0);
    menu_item(a, "Live view", "Canli izleme", 63);
    menu_item(a, "Alarm", "Alarm", 12);
    menu_item(a, "WiFi setup", "WiFi kurulumu", 1);
    menu_item(a, "Recording / sessions", "Kayitlar", 61);
    menu_item(a, "Measurement settings", "Olcum ayarlari", 60);
    menu_item(a, "Analysis screens", "Analiz ekranlari", 64);
    menu_item(a, "Help / information", "Yardim / bilgi", 62);
    menu_open(a);
}
static void measurements_menu(RadarApp* a) {
    menu_begin(a, "MEASUREMENT", "OLCUM AYARLARI", 5);
    menu_item(a, "Calibrate - stay still", "Kalibre et - sabit kal", 0);
    menu_item(a, "Sensitivity profiles", "Hassasiyet profili", 2);
    menu_item(
        a,
        a->settings.automatic ? "Threshold: automatic" : "Threshold: manual",
        a->settings.automatic ? "Esik: otomatik" : "Esik: elle",
        3);
    menu_item(
        a,
        a->ultimate.filter ? "Filter: robust" : "Filter: classic",
        a->ultimate.filter ? "Filtre: dayanikli" : "Filtre: klasik",
        13);
    menu_item(a, "Save threshold to board", "Esigi karta kaydet", 8);
    menu_item(a, "Type WiFi name manually", "WiFi adini elle gir", 9);
    menu_open(a);
}
static void records_menu(RadarApp* a) {
    menu_begin(a, "RECORDING", "KAYITLAR", 6);
    menu_item(
        a,
        a->logging ? "Stop recording" : "Start recording",
        a->logging ? "Kaydi durdur" : "Kayda basla",
        4);
    menu_item(a, "Saved sessions / select A-B", "Kayit sec / A-B", 11);
    menu_item(a, "Compare selected A-B", "Secili A-B karsilastir", 51);
    menu_open(a);
}
static void help_menu(RadarApp* a) {
    menu_begin(a, "HELP", "YARDIM / BILGI", 7);
    menu_item(a, "Quick start", "Kolay kurulum", 14);
    menu_item(a, "Buttons / usage", "Tuslar / kullanim", 6);
    menu_item(a, "Version / hardware", "Surum / donanim", 7);
    menu_item(a, "Connection / resources", "Baglanti / kaynaklar", 15);
    menu_item(
        a,
        a->settings.language ? "Language: Turkish" : "Language: English",
        a->settings.language ? "Dil: Turkce" : "Dil: Ingilizce",
        5);
    menu_open(a);
}
static void analysis_menu(RadarApp* a) {
    menu_begin(a, "ANALYSIS", "ANALIZ EKRANLARI", 8);
    menu_item(a, "Carrier waterfall", "Tasiyici desenleri", 10);
    menu_item(a, "Connection quality", "Baglanti kalitesi", 65);
    menu_item(a, "Session summary", "Oturum ozeti", 66);
    menu_item(a, "Motion history", "Hareket gecmisi", 67);
    menu_item(a, "Measurement diagnostics", "Olcum tanilari", 69);
    menu_open(a);
}
static void alarm_menu(RadarApp* a) {
    menu_begin(a, "ALARM", "ALARM", 3);
    menu_item(
        a,
        a->alarm.armed ? "Turn alarm OFF" : "Turn alarm ON",
        a->alarm.armed ? "Alarmi kapat" : "Alarmi ac",
        40);
    menu_item(a, "Show alarm status", "Alarm durumunu goster", 46);
    menu_item(
        a,
        a->settings.muted ? "Vibration: OFF" : "Vibration: ON",
        a->settings.muted ? "Titresim: kapali" : "Titresim: acik",
        47);
    menu_item(a, "Test vibration / LED", "Titresimi dene", 45);
    menu_item(a, "Timing settings", "Sure ayarlari", 48);
    menu_open(a);
}
static void alarm_timing_menu(RadarApp* a) {
    menu_begin(a, "ALARM TIMING", "ALARM SURELERI", 9);
    char label[40];
    static const unsigned delay[] = {0, 5, 15}, hold[] = {100, 500, 1500},
                          release[] = {500, 2000, 5000}, repeat[] = {0, 10, 30};
    snprintf(
        label,
        sizeof(label),
        "%s: %us",
        tr(a, "Start delay", "Baslama beklemesi"),
        delay[a->ultimate.delay]);
    submenu_add_item(a->menu, label, 41, menu_callback, a);
    snprintf(
        label,
        sizeof(label),
        "%s: %ums",
        tr(a, "Motion duration", "Hareket suresi"),
        hold[a->ultimate.hold]);
    submenu_add_item(a->menu, label, 42, menu_callback, a);
    snprintf(
        label,
        sizeof(label),
        "%s: %ums",
        tr(a, "Clear delay", "Sakinlesme suresi"),
        release[a->ultimate.release]);
    submenu_add_item(a->menu, label, 43, menu_callback, a);
    snprintf(
        label,
        sizeof(label),
        "%s: %us",
        tr(a, "Repeat (0=once)", "Tekrar (0=bir kez)"),
        repeat[a->ultimate.repeat]);
    submenu_add_item(a->menu, label, 44, menu_callback, a);
    menu_open(a);
}
static bool toggle_alarm(RadarApp* a) {
    if(a->alarm.armed) {
        memset(&a->alarm, 0, sizeof(a->alarm));
        return true;
    }
    RadarModel m;
    snapshot(a, &m);
    if(!a->started || !ready(&m) ||
       !csi_fresh(m.report.age, milliseconds(furi_get_tick() - m.last_rx)) ||
       (m.report.state != 1 && m.report.state != 2) || !m.status.quality) {
        toast(a, tr(a, "Wait for valid measurement", "Once olcum hazir olmali"));
        return false;
    }
    memset(&a->alarm, 0, sizeof(a->alarm));
    a->alarm.armed = true;
    a->alarm.armed_at = milliseconds(furi_get_tick());
    return true;
}
static void sessions_menu(RadarApp* a) {
    submenu_reset(a->menu);
    a->menu_kind = 4;
    a->in_menu = true;
    submenu_set_header(
        a->menu,
        a->catalog->busy  ? "SD: reading..." :
        a->catalog->error ? "SD: read error" :
        a->compare_slot   ? "Select B / B sec" :
                            "Select A / A sec");
    if(!a->catalog->busy)
        for(unsigned i = 0; i < a->catalog->count; i++) {
            char label[48];
            snprintf(
                label,
                sizeof(label),
                "%.*s | %lu",
                24,
                a->catalog->items[i].name + 8,
                (unsigned long)a->catalog->items[i].summary.events);
            submenu_add_item(a->menu, label, 200 + i, menu_callback, a);
        }
    submenu_add_item(a->menu, "A / B slot", 50, menu_callback, a);
    submenu_add_item(a->menu, "Compare / Karsilastir", 51, menu_callback, a);
    submenu_add_item(
        a->menu,
        a->catalog->limited ? "Refresh (list limited)" : "Refresh / Yenile",
        52,
        menu_callback,
        a);
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_MENU);
}
static void profiles(RadarApp* a) {
    submenu_reset(a->menu);
    submenu_set_header(a->menu, tr(a, "Motion profiles", "Hareket profilleri"));
    a->menu_kind = 1;
    a->in_menu = true;
    submenu_add_item(a->menu, tr(a, "Quiet: T 0.080", "Sessiz: T 0.080"), 20, menu_callback, a);
    submenu_add_item(a->menu, "Normal: T 0.120", 21, menu_callback, a);
    submenu_add_item(a->menu, tr(a, "Busy: T 0.200", "Hareketli: T 0.200"), 22, menu_callback, a);
    submenu_add_item(
        a->menu, tr(a, "Custom: current threshold", "Ozel: mevcut esik"), 23, menu_callback, a);
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_MENU);
}
static void wifi_menu(RadarApp* a) {
    RadarModel m;
    snapshot(a, &m);
    submenu_reset(a->menu);
    a->menu_kind = 2;
    a->in_menu = true;
    submenu_set_header(
        a->menu,
        a->scanning ? tr(a, "Scanning...", "Taraniyor...") :
                      tr(a, "WiFi networks", "WiFi aglari"));
    for(unsigned i = 0; i < m.ap_count; i++) {
        char label[40], ssid[20];
        unsigned n = 0;
        for(unsigned j = 0; m.aps[i].ssid[j] && n < 18; j++)
            ssid[n++] = (unsigned char)m.aps[i].ssid[j] >= 32 &&
                                (unsigned char)m.aps[i].ssid[j] < 127 ?
                            m.aps[i].ssid[j] :
                            '?';
        ssid[n] = 0;
        snprintf(label, sizeof(label), "%s %ld", ssid, (long)m.aps[i].rssi);
        submenu_add_item(a->menu, label, 100 + i, menu_callback, a);
    }
    submenu_add_item(a->menu, tr(a, "Manual SSID", "SSID elle gir"), 9, menu_callback, a);
    submenu_add_item(a->menu, tr(a, "Scan again", "Yeniden tara"), 1, menu_callback, a);
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_MENU);
}
static void scan(RadarApp* a) {
    if(!send_command(a, "SCAN", "SCAN")) return;
    a->scanning = true;
    a->scan_tick = furi_get_tick();
    with_view_model(a->radar, RadarModel * m, { m->ap_count = 0; }, false);
    wifi_menu(a);
}
static void dashboard(RadarApp* a, unsigned page) {
    a->in_menu = false;
    with_view_model(a->radar, RadarModel * m, { m->page = page; }, true);
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_RADAR);
}
static bool custom_event(void* ctx, uint32_t event) {
    RadarApp* a = ctx;
    if(event == EVENT_EDITOR + 1) {
        a->editing = 0;
        memset(a->password, 0, sizeof(a->password));
        with_view_model(a->editor, EditorModel * m, { memset(m, 0, sizeof(*m)); }, false);
        dashboard(a, 0);
        return true;
    }
    if(event == EVENT_EDITOR) {
        char buffer[65];
        uint8_t min;
        with_view_model(
            a->editor,
            EditorModel * m,
            {
                snprintf(buffer, sizeof(buffer), "%s", m->buffer);
                min = m->min;
            },
            false);
        if(strlen(buffer) < min) {
            with_view_model(a->editor, EditorModel * m, { m->invalid = true; }, true);
            return true;
        }
        if(a->editing == 1) {
            strlcpy(a->ssid, buffer, sizeof(a->ssid));
            edit_wifi(a, 2);
        } else if(a->editing == 2) {
            char sh[65], ph[127], cmd[210];
            csi_hex_encode(a->ssid, sh, sizeof(sh));
            csi_hex_encode(buffer, ph, sizeof(ph));
            snprintf(cmd, sizeof(cmd), "WIFI %s %s", sh, ph);
            send_command(a, cmd, "WIFI");
            memset(ph, 0, sizeof(ph));
            memset(cmd, 0, sizeof(cmd));
            memset(a->password, 0, sizeof(a->password));
            a->editing = 0;
            with_view_model(a->editor, EditorModel * m, { memset(m, 0, sizeof(*m)); }, false);
            dashboard(a, 0);
        }
        memset(buffer, 0, sizeof(buffer));
        return true;
    }
    if(event < EVENT_MENU || event >= EVENT_EDITOR) return false;
    unsigned i = event - EVENT_MENU;
    if(i == 60) {
        measurements_menu(a);
        return true;
    }
    if(i == 61) {
        records_menu(a);
        return true;
    }
    if(i == 62) {
        help_menu(a);
        return true;
    }
    if(i == 63) {
        dashboard(a, 0);
        return true;
    }
    if(i == 64) {
        analysis_menu(a);
        return true;
    }
    if(i == 65) {
        dashboard(a, 1);
        return true;
    }
    if(i == 66) {
        dashboard(a, 4);
        return true;
    }
    if(i == 69) {
        dashboard(a, 11);
        return true;
    }
    if(i == 67) {
        dashboard(a, 5);
        return true;
    }
    if(i == 48) {
        alarm_timing_menu(a);
        return true;
    }
    if(i == 0) {
        request_calibration(a);
        dashboard(a, 0);
    } else if(i == 1)
        scan(a);
    else if(i == 2)
        profiles(a);
    else if(i == 3) {
        CsiSettings s = a->settings;
        s.automatic = !s.automatic;
        if(!a->started) {
            a->settings = s;
            mark_settings(a);
            dashboard(a, 0);
        } else if(configure(a, &s))
            dashboard(a, 0);
    } else if(i == 4) {
        if(!a->logging && !a->started) {
            toast(a, tr(a, "Calibrate before recording", "Kayittan once kalibre et"));
            return true;
        }
        csi_storage_catalog(a->storage, a->catalog);
        if(a->catalog->busy) {
            toast(a, "Wait for SD reading");
            return true;
        }
        RadarModel m;
        snapshot(a, &m);
        if(csi_storage_log(a->storage, a->logging ? 2 : 1, &m.report, &m.status, m.perf[3])) {
            a->logging = !a->logging;
            a->last_log = furi_get_tick();
        }
        records_menu(a);
    } else if(i == 5) {
        a->settings.language = !a->settings.language;
        mark_settings(a);
        help_menu(a);
    } else if(i == 6)
        dashboard(a, 3);
    else if(i == 7)
        dashboard(a, 2);
    else if(i == 8) {
        send_command(a, "SAVE", "SAVE");
        dashboard(a, 0);
    } else if(i == 9) {
        if(a->pending[0])
            toast(a, tr(a, "Wait for reply", "Yaniti bekleyin"));
        else
            edit_wifi(a, 1);
    } else if(i == 10)
        dashboard(a, 6);
    else if(i == 11 || i == 52) {
        if(a->logging || csi_storage_state(a->storage) == 3) {
            toast(a, tr(a, "Stop SD log before browsing", "Once SD kaydini durdurun"));
            return true;
        }
        a->browsing = true;
        csi_storage_catalog_request(a->storage);
        csi_storage_catalog(a->storage, a->catalog);
        sessions_menu(a);
    } else if(i == 12)
        alarm_menu(a);
    else if(i == 13) {
        if(a->pending[0]) {
            toast(a, "Wait for ESP reply");
            return true;
        }
        a->ultimate.filter ^= 1;
        with_view_model(
            a->radar,
            RadarModel * m,
            { m->head = m->count = m->wave_count = m->wave_head = 0; },
            false);
        a->filter_synced = false;
        if(!csi_storage_ultimate_save(a->storage, &a->ultimate)) toast(a, "SD queue full");
        measurements_menu(a);
        submenu_set_selected_item(a->menu, 13);
        toast(a, tr(a, "Filter changed; learning", "Filtre degisti; ogreniyor"));
    } else if(i == 14)
        dashboard(a, 8);
    else if(i == 15)
        dashboard(a, 10);
    else if(i >= 40 && i <= 47) {
        if(i == 40) {
            toggle_alarm(a);
            dashboard(a, 9);
            return true;
        }
        if(i == 41) a->ultimate.delay = (a->ultimate.delay + 1) % 3;
        if(i == 42) a->ultimate.hold = (a->ultimate.hold + 1) % 3;
        if(i == 43) a->ultimate.release = (a->ultimate.release + 1) % 3;
        if(i == 44) a->ultimate.repeat = (a->ultimate.repeat + 1) % 3;
        if(i == 47) {
            a->settings.muted = !a->settings.muted;
            mark_settings(a);
        }
        if(i == 45) {
            notification_message(a->notifications, &sequence_single_vibro);
            notification_message(a->notifications, &sequence_blink_blue_100);
        }
        if(i >= 41 && i <= 44) {
            memset(&a->alarm, 0, sizeof(a->alarm));
            if(!csi_storage_ultimate_save(a->storage, &a->ultimate)) toast(a, "SD queue full");
        }
        if(i == 46)
            dashboard(a, 9);
        else if(i >= 41 && i <= 44) {
            alarm_timing_menu(a);
            submenu_set_selected_item(a->menu, i);
        } else {
            alarm_menu(a);
            submenu_set_selected_item(a->menu, i);
        }
    } else if(i == 50) {
        a->compare_slot ^= 1;
        sessions_menu(a);
    } else if(i == 51)
        dashboard(a, 7);
    else if(i >= 200 && i < 200 + CSI_CATALOG_MAX) {
        unsigned index = i - 200;
        if(index < a->catalog->count && !a->catalog->busy) {
            if(a->catalog->items[index].error || !a->catalog->items[index].rows) {
                toast(a, tr(a, "Invalid CSV / read error", "CSV gecersiz / okuma hatasi"));
                return true;
            }
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    m->compare[a->compare_slot] = a->catalog->items[index];
                    m->compare_ready[a->compare_slot] = true;
                },
                false);
            a->compare_slot ^= 1;
            sessions_menu(a);
        }
    } else if(i >= 20 && i <= 23) {
        static const uint16_t preset[] = {80, 120, 200};
        CsiSettings s = a->settings;
        s.profile = i - 20;
        s.automatic = false;
        if(i < 23)
            s.threshold = preset[i - 20];
        else
            s.threshold = s.custom_threshold;
        if(!a->started) {
            a->settings = s;
            mark_settings(a);
            dashboard(a, 0);
        } else if(configure(a, &s))
            dashboard(a, 0);
    } else if(i >= 100 && i < 100 + CSI_AP_MAX) {
        RadarModel m;
        snapshot(a, &m);
        unsigned index = i - 100;
        if(index < m.ap_count) {
            if(m.aps[index].auth != 3 && m.aps[index].auth != 4 && m.aps[index].auth != 7) {
                toast(a, "WPA2/WPA3 mixed required");
                return true;
            }
            snprintf(a->ssid, sizeof(a->ssid), "%s", m.aps[index].ssid);
            edit_wifi(a, 2);
        }
    }
    return true;
}
static bool navigate(void* ctx) {
    RadarApp* a = ctx;
    if(a->editing) {
        custom_event(a, EVENT_EDITOR + 1);
        return true;
    }
    if(a->in_menu) {
        if(a->menu_kind == 9)
            alarm_menu(a);
        else if(a->menu_kind)
            show_settings(a);
        else
            dashboard(a, 0);
        return true;
    }
    RadarModel m;
    snapshot(a, &m);
    if(m.page) {
        dashboard(a, 0);
        return true;
    }
    view_dispatcher_stop(a->dispatcher);
    return true;
}
static bool input(InputEvent* e, void* ctx) {
    RadarApp* a = ctx;
    if(e->key == InputKeyBack) return false;
    if(e->type == InputTypeLong) {
        if(e->key == InputKeyLeft)
            show_settings(a);
        else if(e->key == InputKeyOk) {
            request_calibration(a);
            dashboard(a, 0);
        } else if(e->key == InputKeyRight) {
            a->settings.muted = !a->settings.muted;
            mark_settings(a);
        }
        return true;
    }
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return true;
    if(e->key == InputKeyLeft && e->type == InputTypeShort) {
        show_settings(a);
    } else if(e->key == InputKeyRight && e->type == InputTypeShort) {
        RadarModel m;
        snapshot(a, &m);
        if(m.page == 9) {
            a->settings.muted = !a->settings.muted;
            mark_settings(a);
        } else
            dashboard(
                a,
                m.page == 0  ? 6 :
                m.page == 6  ? 1 :
                m.page == 1  ? 11 :
                m.page == 11 ? 4 :
                m.page == 7  ? 12 :
                m.page == 4  ? 5 :
                               0);
    } else if(e->key == InputKeyOk && e->type == InputTypeShort) {
        RadarModel m;
        snapshot(a, &m);
        if(m.page == 9)
            toggle_alarm(a);
        else if(m.page == 0 || m.page == 6 || m.page == 8) {
            request_calibration(a);
            dashboard(a, 0);
        } else
            show_settings(a);
    } else if(e->key == InputKeyUp || e->key == InputKeyDown) {
        RadarModel m;
        snapshot(a, &m);
        int32_t t = a->settings.automatic ? m.report.threshold : a->settings.threshold;
        if(!a->started || !ready(&m)) return true;
        t += e->key == InputKeyUp ? 10 : -10;
        if(t < CSI_THR_MIN) t = CSI_THR_MIN;
        if(t > CSI_THR_MAX) t = CSI_THR_MAX;
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "THR %ld", (long)t);
        if(send_command(a, cmd, "THR")) {
            a->desired = a->settings;
            a->desired.threshold = t;
            a->desired.custom_threshold = t;
            a->desired.profile = 3;
            a->desired.automatic = false;
            a->settings_pending = true;
        }
    }
    return true;
}
static void response(RadarApp* a, const char* p) {
    if(!strncmp(p, "APEND,", 6)) {
        a->scanning = false;
        if(a->in_menu && a->menu_kind == 2) wifi_menu(a);
        return;
    }
    if(!strcmp(p, "EV,CAL,OK")) {
        toast(a, tr(a, "Calibration complete", "Kalibrasyon tamam"));
        return;
    }
    if(!strcmp(p, "EV,CAL,FAIL")) {
        toast(a, tr(a, "Noisy: recalibrate", "Gurultulu: tekrar kalibre"));
        return;
    }
    if(!strncmp(p, "ERR,", 4)) {
        if(a->start_requested) {
            a->started = false;
            a->start_requested = false;
        }
        if(!strncmp(p, "ERR,SCAN", 8)) a->scanning = false;
        a->pending[0] = 0;
        a->settings_pending = false;
        toast(
            a,
            !strcmp(p, "ERR,NO_CSI") ? tr(a, "No fresh CSI: wait", "Guncel CSI yok: bekleyin") :
            !strcmp(p, "ERR,SCAN_BUSY") ?
                                       tr(a, "WiFi busy: retry scan", "WiFi mesgul: tekrar tara") :
                                       p);
        return;
    }
    if(a->pending[0]) {
        char expected[24];
        snprintf(expected, sizeof(expected), "ACK,%s", a->pending);
        if(!strcmp(p, expected)) {
            if(a->settings_pending) {
                a->settings = a->desired;
                a->settings_pending = false;
                mark_settings(a);
            }
            if(!strcmp(a->pending, "CAL")) {
                a->started = true;
                a->start_requested = false;
                with_view_model(
                    a->radar,
                    RadarModel * v,
                    {
                        v->started = true;
                        v->await_cal_frame = true;
                        v->head = v->count = 0;
                        memset(&v->session, 0, sizeof(v->session));
                    },
                    false);
            }
            if(!strcmp(a->pending, "CFG")) a->synced = true;
            if(!strcmp(a->pending, "FILT")) a->filter_synced = true;
            if(!strcmp(a->pending, "WF")) a->wave_enabled = a->wave_target;
            toast(
                a,
                !strcmp(a->pending, "CAL") ?
                    tr(a, "Stay still: calibrating", "Hareketsiz kalin: kalibre") :
                    tr(a, "ESP32 confirmed", "ESP32 onayladi"));
            a->pending[0] = 0;
        }
    }
}
static void tick(void* ctx) {
    RadarApp* a = ctx;
    RadarModel m;
    snapshot(a, &m);
    if(m.epoch != a->last_epoch) {
        a->last_epoch = m.epoch;
        a->started = a->start_requested = false;
        memset(&a->alarm, 0, sizeof(a->alarm));
        a->synced = false;
        a->filter_synced = false;
        a->wave_enabled = false;
        a->sync_attempts = 0;
        a->pending[0] = 0;
        a->settings_pending = false;
    }
    if(!recent(a->last_ping, 2000)) {
        tx(a, "PING");
        a->last_ping = furi_get_tick();
    }
    char reply[96];
    for(unsigned i = 0; i < 16 && furi_message_queue_get(a->replies, reply, 0) == FuriStatusOk;
        i++)
        response(a, reply);
    if(ready(&m) && !a->loaded) {
        a->settings.threshold = m.report.threshold;
        a->settings.custom_threshold = m.report.threshold;
        a->settings.automatic = m.status.automatic != 0;
        a->settings.profile = 3;
        a->loaded = true;
        mark_settings(a);
    }
    if(a->start_requested && ready(&m) && !a->synced && a->sync_attempts < 3 && !a->pending[0] &&
       !a->editing) {
        if(configure(a, &a->settings)) a->sync_attempts++;
    }
    if((a->started || a->start_requested) && ready(&m) && a->synced && !a->pending[0] &&
       !a->filter_synced) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "FILT %u", a->ultimate.filter);
        send_command(a, cmd, "FILT");
    }
    if(a->start_requested && ready(&m) && a->synced && a->filter_synced && !a->pending[0])
        send_command(a, "CAL 100", "CAL");
    bool want_wave = a->started && m.page == 6;
    if(ready(&m) && a->synced && a->filter_synced && !a->pending[0] &&
       want_wave != a->wave_enabled) {
        if(send_command(a, want_wave ? "WF 1" : "WF 0", "WF")) a->wave_target = want_wave;
    }
    if(a->browsing) {
        csi_storage_catalog(a->storage, a->catalog);
        if(!a->catalog->busy && a->catalog->generation != a->catalog_generation) {
            a->catalog_generation = a->catalog->generation;
            if(a->in_menu && a->menu_kind == 4) sessions_menu(a);
        }
    }
    bool fresh = ready(&m) && csi_measurement_ready(
                                  a->started,
                                  m.await_cal_frame,
                                  m.report.age,
                                  milliseconds(furi_get_tick() - m.last_rx));
    bool moving = fresh && m.report.state == 2;
    bool alarm_notify = csi_alarm_step(
        &a->alarm,
        &a->ultimate,
        milliseconds(furi_get_tick()),
        fresh && m.status.quality > 0,
        moving);
    if((alarm_notify || (!a->alarm.armed && moving && !a->was_moving)) && !a->settings.muted &&
       !a->editing) {
        notification_message(a->notifications, &sequence_single_vibro);
        notification_message(a->notifications, &sequence_blink_blue_100);
    }
    a->was_moving = moving;
    if(a->pending[0] && !recent(a->sent_tick, 4000)) {
        a->start_requested = false;
        a->pending[0] = 0;
        a->settings_pending = false;
        toast(a, tr(a, "No reply - retry", "Yanit yok - tekrar deneyin"));
    }
    if(a->scanning && !recent(a->scan_tick, 11000)) {
        a->scanning = false;
        toast(a, tr(a, "Scan timed out", "Tarama zaman asimi"));
        if(a->in_menu && a->menu_kind == 2) wifi_menu(a);
    }
    if(a->dirty && !recent(a->dirty_tick, 500)) {
        if(csi_storage_settings(a->storage, &a->settings)) a->dirty = false;
    }
    if(a->logging && (!recent(a->last_log, 1000) || m.report.state != a->log_previous)) {
        CsiReport record = m.report;
        CsiStatus status = m.status;
        if(!fresh) {
            record.state =
                m.received && recent(m.last_rx, 2000) && m.report.state >= 4 ? m.report.state : 4;
            record.score = 0;
            status.quality = 0;
        }
        csi_storage_log(a->storage, 0, &record, &status, m.perf[3]);
        a->last_log = furi_get_tick();
        a->log_previous = m.report.state;
    }
    unsigned log_state = csi_storage_state(a->storage);
    if(log_state == 2 && a->logging) {
        a->logging = false;
        toast(a, tr(a, "SD error: logging stopped", "SD hata: kayit durdu"));
    }
    uint32_t bytes = atomic_load(&a->rx_bytes),
             elapsed = milliseconds(furi_get_tick() - a->rate_tick);
    if(elapsed >= 1000) {
        uint32_t rate = (uint32_t)((uint64_t)(bytes - a->rate_bytes) * 1000 / elapsed);
        with_view_model(a->radar, RadarModel * v, { v->uart_rate = rate; }, false);
        a->rate_tick = furi_get_tick();
        a->rate_bytes = bytes;
    }
    with_view_model(
        a->radar,
        RadarModel * v,
        {
            v->started = a->started;
            v->starting = a->start_requested || v->await_cal_frame;
            v->heap = memmgr_get_free_heap();
            v->heap_min = memmgr_get_minimum_free_heap();
            v->ultimate = a->ultimate;

            uint32_t passed = milliseconds(furi_get_tick()) - a->alarm.armed_at;
            v->alarm_wait = a->alarm.armed && passed < delays[a->ultimate.delay] ?
                                (delays[a->ultimate.delay] - passed + 999) / 1000 :
                                0;
            v->alarm_armed = a->alarm.armed;
            v->alarm_active = a->alarm.active;
            v->prefs = a->settings;
            v->log_state = log_state;
            v->log_lost = csi_storage_lost(a->storage);
            v->prefs_error = csi_storage_settings_error(a->storage);
        },
        true);
}
int32_t csi_radar_app(void* context) {
    UNUSED(context);
    RadarApp* a = malloc(sizeof(*a));
    furi_check(a);
    memset(a, 0, sizeof(*a));
    atomic_init(&a->rx_lost, false);
    atomic_init(&a->rx_bytes, 0);
    a->catalog = malloc(sizeof(CsiCatalog));
    furi_check(a->catalog);
    memset(a->catalog, 0, sizeof(CsiCatalog));
    csi_storage_ultimate_load(&a->ultimate);
    a->loaded = csi_storage_load(&a->settings);
    a->storage = csi_storage_alloc();
    a->log_previous = -1;
    a->stream = furi_stream_buffer_alloc(2048, 1);
    a->replies = furi_message_queue_alloc(16, 96);
    a->dispatcher = view_dispatcher_alloc();
    a->radar = view_alloc();
    a->editor = view_alloc();
    a->menu = submenu_alloc();
    view_allocate_model(a->radar, ViewModelTypeLocking, sizeof(RadarModel));
    view_set_context(a->radar, a);
    view_set_draw_callback(a->radar, draw);
    view_set_input_callback(a->radar, input);
    view_allocate_model(a->editor, ViewModelTypeLocking, sizeof(EditorModel));
    view_set_context(a->editor, a);
    view_set_draw_callback(a->editor, editor_draw);
    view_set_input_callback(a->editor, editor_input);
    view_dispatcher_add_view(a->dispatcher, VIEW_RADAR, a->radar);
    view_dispatcher_add_view(a->dispatcher, VIEW_EDITOR, a->editor);
    view_dispatcher_add_view(a->dispatcher, VIEW_MENU, submenu_get_view(a->menu));
    view_dispatcher_set_event_callback_context(a->dispatcher, a);
    view_dispatcher_set_navigation_event_callback(a->dispatcher, navigate);
    view_dispatcher_set_custom_event_callback(a->dispatcher, custom_event);
    view_dispatcher_set_tick_event_callback(a->dispatcher, tick, 100);
    a->gui = furi_record_open(RECORD_GUI);
    a->notifications = furi_record_open(RECORD_NOTIFICATION);
    view_dispatcher_attach_to_gui(a->dispatcher, a->gui, ViewDispatcherTypeFullscreen);
    a->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(a->expansion);
    a->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    with_view_model(
        a->radar,
        RadarModel * m,
        {
            m->busy = a->serial == NULL;
            m->prefs = a->settings;
        },
        false);
    a->worker = furi_thread_alloc_ex("CsiRadarRx", 3072, worker, a);
    furi_thread_start(a->worker);
    if(a->serial) {
        furi_hal_serial_init(a->serial, 115200);
        furi_hal_serial_async_rx_start(a->serial, rx_callback, a, true);
    }
    notification_message(a->notifications, &sequence_display_backlight_enforce_on);
    furi_hal_power_insomnia_enter();
    if(!a->loaded) dashboard(a, 8);
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_RADAR);
    view_dispatcher_run(a->dispatcher);
    if(a->serial) furi_hal_serial_async_rx_stop(a->serial);
    furi_thread_flags_set(furi_thread_get_id(a->worker), RX_STOP);
    furi_thread_join(a->worker);
    furi_thread_free(a->worker);
    if(a->logging) {
        RadarModel m;
        snapshot(a, &m);
        csi_storage_log(a->storage, 2, &m.report, &m.status, m.perf[3]);
    }
    if(a->dirty) csi_storage_settings(a->storage, &a->settings);
    csi_storage_free(a->storage);
    if(a->serial) {
        furi_hal_serial_tx_wait_complete(a->serial);
        furi_hal_serial_deinit(a->serial);
        furi_hal_serial_control_release(a->serial);
    }
    expansion_enable(a->expansion);
    notification_message(a->notifications, &sequence_display_backlight_enforce_auto);
    furi_hal_power_insomnia_exit();
    view_dispatcher_remove_view(a->dispatcher, VIEW_EDITOR);
    view_dispatcher_remove_view(a->dispatcher, VIEW_MENU);
    view_dispatcher_remove_view(a->dispatcher, VIEW_RADAR);
    with_view_model(a->editor, EditorModel * m, { memset(m, 0, sizeof(*m)); }, false);
    submenu_free(a->menu);
    view_free(a->editor);
    view_free(a->radar);
    view_dispatcher_free(a->dispatcher);
    furi_stream_buffer_free(a->stream);
    furi_message_queue_free(a->replies);
    furi_record_close(RECORD_EXPANSION);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(a->catalog);
    memset(a, 0, sizeof(*a));
    free(a);
    return 0;
}
