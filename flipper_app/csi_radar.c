#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
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
#define GRAPH_POINTS 126
#define RX_STOP      1U
#define VIEW_RADAR   0
#define VIEW_MENU    1
#define VIEW_EDITOR  2
#define EVENT_MENU   1000U
#define EVENT_EDITOR 2000U

typedef struct {
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
    atomic_bool rx_lost;
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
        toast(a, tr(a, "ESP firmware 1.5 needed", "ESP 1.5 / baglanti gerekli"));
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
        if(p && csi_report_parse(p, &r)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    if(m->received && (r.seq < m->report.seq || !recent(m->last_rx, 2000))) {
                        m->head = m->count = 0;
                        m->epoch++;
                        m->hello = false;
                        m->status_received = false;
                    }
                    m->report = r;
                    m->received = true;
                    m->last_rx = furi_get_tick();
                    m->graph[m->head] = (r.state == 1 || r.state == 2) && r.age < 2000 ?
                                            (uint16_t)r.score :
                                            UINT16_MAX;
                    m->head = (m->head + 1) % GRAPH_POINTS;
                    if(m->count < GRAPH_POINTS) m->count++;
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
                { m->hello = !strcmp(p, "HELLO,1.5.0,2,ESP32-S2"); },
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
static void draw(Canvas* c, void* ctx) {
    RadarModel* m = ctx;
    CsiReport* r = &m->report;
    bool turkish = m->prefs.language;
    bool linked = m->received && recent(m->last_rx, 2000);
    bool fresh = linked && csi_fresh(r->age, milliseconds(furi_get_tick() - m->last_rx));
    const char* en[] = {
        "LEARNING", "STILL", "MOVING", "CALIBRATE", "NO CSI", "WIFI OFF", "SETUP WIFI"};
    const char* tu[] = {
        "OGRENIYOR", "DURGUN", "HAREKET", "KALIBRE", "CSI YOK", "WIFI YOK", "WIFI KUR"};
    const char* label = m->busy                 ? "UART BUSY" :
                        !linked                 ? (turkish ? "BAGLANTI YOK" : "NO LINK") :
                        r->state <= 3 && !fresh ? (turkish ? "CSI YOK" : "NO CSI") :
                                                  (turkish ? tu[r->state] : en[r->state]);
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 1, 10, label);
    canvas_set_font(c, FontSecondary);
    char buf[64];
    if(m->page == 2) {
        canvas_draw_str(c, 1, 23, "CSI Radar Pro 1.5.0");
        canvas_draw_str(c, 1, 33, "FAP: API 87.1 / E11");
        canvas_draw_str(
            c, 1, 43, m->hello ? "ESP: 1.5.0 / protocol 2" : "ESP: update / link needed");
        canvas_draw_str(c, 1, 53, "ESP32-S2 / 20 MHz LLTF");
        canvas_draw_str(c, 1, 64, turkish ? "Geri: ana ekran" : "Back: dashboard");
        return;
    }
    if(m->page == 3) {
        const char* lines_en[] = {
            "S: motion score T: threshold",
            "Lower T = more sensitive",
            "OK: calibrate (stay still)",
            "Hold <: menu / profiles",
            "Hold OK: WiFi; hold >:save",
            "Back: dashboard"};
        const char* lines_tr[] = {
            "S: skor   T: hareket esigi",
            "Dusuk T = daha hassas",
            "OK: kalibre (hareketsiz)",
            "Uzun <: menu / profiller",
            "Uzun OK:WiFi  Uzun >:kayit",
            "Geri: ana ekran"};
        for(unsigned i = 0; i < 6; i++)
            canvas_draw_str(c, 1, 20 + i * 8, turkish ? lines_tr[i] : lines_en[i]);
        return;
    }
    snprintf(
        buf,
        sizeof(buf),
        "%lu/s%s%s",
        (unsigned long)r->rate,
        m->prefs.muted ? " M" : "",
        m->log_state == 1 ? " R" :
        m->log_state == 2 ? " !" :
                            "");
    canvas_draw_str_aligned(c, 127, 1, AlignRight, AlignTop, buf);
    canvas_draw_line(c, 0, 12, 127, 12);
    if(m->page == 1) {
        snprintf(
            buf,
            sizeof(buf),
            "RSSI %ld  Q %lu/100",
            (long)r->rssi,
            (unsigned long)(fresh ? m->status.quality : 0));
        canvas_draw_str(c, 1, 23, buf);
        snprintf(
            buf,
            sizeof(buf),
            "Noise %lu  %s%s",
            (unsigned long)m->status.noise,
            m->status.automatic ? "AUTO" : "MANUAL",
            m->status.automatic && !m->status.calibrated ? " ?" : "");
        canvas_draw_str(c, 1, 33, buf);
        snprintf(
            buf,
            sizeof(buf),
            "Drop %lu RX %lu SD %lu",
            (unsigned long)r->drops,
            (unsigned long)m->bad_frames,
            (unsigned long)m->log_lost);
        canvas_draw_str(c, 1, 43, buf);
        snprintf(
            buf,
            sizeof(buf),
            "WiFi err %lu Cal err %lu",
            (unsigned long)m->status.reason,
            (unsigned long)m->status.cal_error);
        canvas_draw_str(c, 1, 53, buf);
    } else if(!linked || !m->hello || r->state >= 4 || !fresh) {
        const char* l1;
        const char* l2;
        if(!linked) {
            l1 = turkish ? "ESP karti / kabloyu kontrol" : "Check ESP board / wiring";
            l2 = turkish ? "Uzun <: ayarlar / yardim" : "Hold <: settings / help";
        } else if(!m->hello) {
            l1 = "ESP firmware 1.5 gerekli";
            l2 = "Update ESP firmware to 1.5";
        } else if(r->state == 6) {
            l1 = turkish ? "Uzun OK: ag sec / sifre" : "Hold OK: select WiFi";
            l2 = "2.4 GHz / 20 MHz";
        } else if(r->state == 5) {
            l1 = (m->status.reason == 202 || m->status.reason == 15) ?
                     (turkish ? "Sifre / kimlik hatasi" : "Password / auth failure") :
                 m->status.reason == 201 ?
                     (turkish ? "WiFi agi bulunamadi" : "WiFi network not found") :
                     (turkish ? "WiFi yeniden baglaniyor" : "WiFi reconnecting");
            l2 = turkish ? "Uzun OK: ag ayarlari" : "Hold OK: WiFi setup";
        } else {
            l1 = turkish ? "WiFi var, gecerli CSI yok" : "WiFi up, no fresh CSI";
            l2 = turkish ? "Router / ping / 20 MHz" : "Check ping / 20 MHz";
        }
        canvas_draw_str(c, 1, 30, l1);
        canvas_draw_str(c, 1, 44, l2);
    } else {
        int32_t ymax = r->threshold * 3;
        if(ymax < 300) ymax = 300;
        for(unsigned i = 0; i < m->count; i++) {
            uint16_t v = m->graph[(m->head + GRAPH_POINTS - m->count + i) % GRAPH_POINTS];
            if(v != UINT16_MAX && v > ymax) ymax = v;
        }
        int ty = 43 - r->threshold * 28 / ymax;
        for(int x = 1; x < 127; x += 4)
            canvas_draw_dot(c, x, ty);
        int px = -1, py = 0;
        for(unsigned i = 0; i < m->count; i++) {
            uint16_t v = m->graph[(m->head + GRAPH_POINTS - m->count + i) % GRAPH_POINTS];
            if(v == UINT16_MAX) {
                px = -1;
                continue;
            }
            int x = 1 + GRAPH_POINTS - m->count + i, y = 43 - v * 28 / ymax;
            if(px >= 0) canvas_draw_line(c, px, py, x, y);
            px = x;
            py = y;
        }
        if(r->state == 0 || r->state == 3)
            snprintf(
                buf,
                sizeof(buf),
                "%lu%% %s",
                (unsigned long)r->progress,
                m->status.automatic ? "AUTO" : "");
        else
            snprintf(
                buf,
                sizeof(buf),
                "S %ld.%03ld T %ld.%03ld%s",
                (long)(r->score / 1000),
                (long)(r->score % 1000),
                (long)(r->threshold / 1000),
                (long)(r->threshold % 1000),
                m->status.automatic ? " A" : "");
        canvas_draw_str(c, 1, 54, buf);
    }
    if(m->toast[0] && recent(m->toast_tick, 2200))
        canvas_draw_str(c, 1, 64, m->toast);
    else if(m->prefs_error)
        canvas_draw_str(c, 1, 64, turkish ? "SD ayar kaydi basarisiz" : "SD settings save failed");
    else
        canvas_draw_str(
            c, 1, 64, turkish ? "OK:Kal +/-:T  Uzun <:Menu" : "OK:Cal +/-:T Hold <:Menu");
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
    canvas_draw_str(c, 1, 8, shown);
    for(unsigned key = 0; key < 96; key++) {
        int x = (key % 16) * 8, y = 11 + (key / 16) * 7;
        if(key == m->key) {
            canvas_draw_box(c, x, y, 8, 7);
            canvas_set_color(c, ColorWhite);
        }
        canvas_draw_glyph(c, x + 1, y + 6, key == 95 ? '#' : key == 0 ? '_' : (char)(32 + key));
        if(key == m->key) canvas_set_color(c, ColorBlack);
    }
    canvas_draw_str(
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
static void show_settings(RadarApp* a) {
    submenu_reset(a->menu);
    submenu_set_header(a->menu, "CSI Radar Pro 1.5");
    a->menu_kind = 0;
    a->in_menu = true;
    submenu_add_item(a->menu, tr(a, "Calibrate", "Kalibrasyon"), 0, menu_callback, a);
    submenu_add_item(
        a->menu, tr(a, "WiFi scan / select", "WiFi tara / ag sec"), 1, menu_callback, a);
    submenu_add_item(a->menu, tr(a, "Profiles", "Profiller"), 2, menu_callback, a);
    submenu_add_item(
        a->menu,
        a->settings.automatic ? "AUTO: ON (cal)" : "AUTO: OFF (manual)",
        3,
        menu_callback,
        a);
    submenu_add_item(a->menu, a->logging ? "SD log: STOP" : "SD log: START", 4, menu_callback, a);
    submenu_add_item(
        a->menu,
        a->settings.language ? "Dil: Turkce -> English" : "Language: EN -> TR",
        5,
        menu_callback,
        a);
    submenu_add_item(a->menu, tr(a, "Help / controls", "Yardim / tuslar"), 6, menu_callback, a);
    submenu_add_item(
        a->menu, tr(a, "Version / compatibility", "Surum / uyumluluk"), 7, menu_callback, a);
    submenu_add_item(
        a->menu, tr(a, "Save threshold to ESP", "Esigi ESP'ye kaydet"), 8, menu_callback, a);
    submenu_add_item(
        a->menu, tr(a, "Manual WiFi SSID", "WiFi adini elle gir"), 9, menu_callback, a);
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
    if(i == 0) {
        send_command(a, "CAL 100", "CAL");
        dashboard(a, 0);
    } else if(i == 1)
        scan(a);
    else if(i == 2)
        profiles(a);
    else if(i == 3) {
        CsiSettings s = a->settings;
        s.automatic = !s.automatic;
        if(configure(a, &s)) dashboard(a, 0);
    } else if(i == 4) {
        RadarModel m;
        snapshot(a, &m);
        if(csi_storage_log(a->storage, a->logging ? 2 : 1, &m.report, &m.status)) {
            a->logging = !a->logging;
            a->last_log = furi_get_tick();
        }
        show_settings(a);
    } else if(i == 5) {
        a->settings.language = !a->settings.language;
        mark_settings(a);
        show_settings(a);
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
    } else if(i >= 20 && i <= 23) {
        static const uint16_t preset[] = {80, 120, 200};
        CsiSettings s = a->settings;
        s.profile = i - 20;
        s.automatic = false;
        if(i < 23)
            s.threshold = preset[i - 20];
        else
            s.threshold = s.custom_threshold;
        if(configure(a, &s)) dashboard(a, 0);
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
        if(a->menu_kind)
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
        else if(e->key == InputKeyOk)
            scan(a);
        else if(e->key == InputKeyRight)
            send_command(a, "SAVE", "SAVE");
        return true;
    }
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return true;
    if(e->key == InputKeyLeft && e->type == InputTypeShort) {
        with_view_model(a->radar, RadarModel * m, { m->page = m->page == 1 ? 0 : 1; }, true);
    } else if(e->key == InputKeyRight && e->type == InputTypeShort) {
        a->settings.muted = !a->settings.muted;
        mark_settings(a);
    } else if(e->key == InputKeyOk && e->type == InputTypeShort)
        send_command(a, "CAL 100", "CAL");
    else if(e->key == InputKeyUp || e->key == InputKeyDown) {
        RadarModel m;
        snapshot(a, &m);
        int32_t t = a->settings.automatic ? m.report.threshold : a->settings.threshold;
        if(!ready(&m)) return true;
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
            if(!strcmp(a->pending, "CFG")) a->synced = true;
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
        a->synced = false;
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
    if(ready(&m) && !a->synced && a->sync_attempts < 3 && !a->pending[0] && !a->editing) {
        if(configure(a, &a->settings)) a->sync_attempts++;
    }
    bool fresh = ready(&m) && csi_fresh(m.report.age, milliseconds(furi_get_tick() - m.last_rx));
    bool moving = fresh && m.report.state == 2;
    if(moving && !a->was_moving && !a->settings.muted && !a->editing) {
        notification_message(a->notifications, &sequence_single_vibro);
        notification_message(a->notifications, &sequence_blink_blue_100);
    }
    a->was_moving = moving;
    if(a->pending[0] && !recent(a->sent_tick, 4000)) {
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
        csi_storage_log(a->storage, 0, &record, &status);
        a->last_log = furi_get_tick();
        a->log_previous = m.report.state;
    }
    unsigned log_state = csi_storage_state(a->storage);
    if(log_state == 2 && a->logging) {
        a->logging = false;
        toast(a, tr(a, "SD error: logging stopped", "SD hata: kayit durdu"));
    }
    with_view_model(
        a->radar,
        RadarModel * v,
        {
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
    furi_hal_power_insomnia_enter();
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_RADAR);
    view_dispatcher_run(a->dispatcher);
    if(a->serial) furi_hal_serial_async_rx_stop(a->serial);
    furi_thread_flags_set(furi_thread_get_id(a->worker), RX_STOP);
    furi_thread_join(a->worker);
    furi_thread_free(a->worker);
    if(a->logging) {
        RadarModel m;
        snapshot(a, &m);
        csi_storage_log(a->storage, 2, &m.report, &m.status);
    }
    if(a->dirty) csi_storage_settings(a->storage, &a->settings);
    csi_storage_free(a->storage);
    if(a->serial) {
        furi_hal_serial_tx_wait_complete(a->serial);
        furi_hal_serial_deinit(a->serial);
        furi_hal_serial_control_release(a->serial);
    }
    expansion_enable(a->expansion);
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
    memset(a, 0, sizeof(*a));
    free(a);
    return 0;
}
