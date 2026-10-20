#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <expansion/expansion.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csi_protocol.h"

#define GRAPH_POINTS    126
#define LINK_TIMEOUT_MS 2000U
#define RX_STOP         1U
#define VIEW_RADAR      0
#define VIEW_TEXT       1
#define EVENT_TEXT_DONE 1

typedef struct {
    CsiReport report;
    uint16_t graph[GRAPH_POINTS]; /* UINT16_MAX = no measurement */
    uint16_t head, count;
    uint32_t last_rx, last_reply, bad_frames;
    bool received, busy, muted, details;
    char reply[40], toast[32];
    uint32_t toast_tick;
} RadarModel;
typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    View* radar;
    TextInput* text;
    NotificationApp* notifications;
    Expansion* expansion;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* stream;
    FuriThread* worker;
    atomic_bool rx_lost;
    char ssid[33], password[64], pending[12];
    uint32_t sent_tick, reply_seen;
    int32_t desired_threshold;
    unsigned editing;
    bool was_moving;
} RadarApp;

static bool recent(uint32_t tick, uint32_t period) {
    return (uint32_t)(furi_get_tick() - tick) < furi_ms_to_ticks(period);
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
static bool send_command(RadarApp* a, const char* payload, const char* name) {
    if(!a->serial) {
        toast(a, "UART busy");
        return false;
    }
    if(a->pending[0]) {
        toast(a, "Waiting for ESP32");
        return false;
    }
    char frame[CSI_LINE_CAP];
    if(!csi_frame(frame, sizeof(frame), payload)) {
        toast(a, "Command too long");
        return false;
    }
    snprintf(a->pending, sizeof(a->pending), "%s", name);
    a->sent_tick = furi_get_tick();
    furi_hal_serial_tx(a->serial, (const uint8_t*)frame, strlen(frame));
    furi_hal_serial_tx_wait_complete(a->serial);
    memset(frame, 0, sizeof(frame));
    toast(a, "Sending...");
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
        if(p && csi_report_parse(p, &r)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    if(m->received &&
                       (r.seq <= m->report.seq || !recent(m->last_rx, LINK_TIMEOUT_MS))) {
                        m->head = m->count = 0;
                    }
                    m->report = r;
                    m->received = true;
                    m->last_rx = furi_get_tick();
                    m->graph[m->head] = (r.state == 1 || r.state == 2) && r.age < LINK_TIMEOUT_MS ?
                                            (uint16_t)r.score :
                                            UINT16_MAX;
                    m->head = (m->head + 1) % GRAPH_POINTS;
                    if(m->count < GRAPH_POINTS) m->count++;
                },
                false);
        } else if(p && (strncmp(p, "ACK,", 4) == 0 || strncmp(p, "ERR,", 4) == 0)) {
            with_view_model(
                a->radar,
                RadarModel * m,
                {
                    snprintf(m->reply, sizeof(m->reply), "%s", p);
                    m->last_reply++;
                },
                false);
        } else {
            with_view_model(a->radar, RadarModel * m, { m->bad_frames++; }, false);
        }
    }
    return 0;
}
static void draw(Canvas* c, void* ctx) {
    RadarModel* m = ctx;
    CsiReport* r = &m->report;
    bool alive = m->received && recent(m->last_rx, LINK_TIMEOUT_MS);
    const char* names[] = {
        "LEARNING", "STILL", "MOVING", "CALIBRATE", "NO CSI", "WIFI OFF", "SETUP WIFI"};
    const char* label = m->busy ? "UART BUSY" : !alive ? "NO LINK" : names[r->state];
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 1, 10, label);
    canvas_set_font(c, FontSecondary);
    char buf[48];
    if(alive) {
        if(r->state == 0 || r->state == 3)
            snprintf(buf, sizeof(buf), "%lu%%", (unsigned long)r->progress);
        else
            snprintf(buf, sizeof(buf), "%lu/s%s", (unsigned long)r->rate, m->muted ? " M" : "");
        canvas_draw_str_aligned(c, 127, 1, AlignRight, AlignTop, buf);
    }
    canvas_draw_line(c, 0, 12, 127, 12);
    if(m->details) {
        snprintf(
            buf, sizeof(buf), "RSSI %ld dBm  Rate %lu/s", (long)r->rssi, (unsigned long)r->rate);
        canvas_draw_str(c, 1, 23, buf);
        snprintf(buf, sizeof(buf), "Packets %lu", (unsigned long)r->pkts);
        canvas_draw_str(c, 1, 33, buf);
        snprintf(
            buf,
            sizeof(buf),
            "Drop %lu  Bad %lu",
            (unsigned long)r->drops,
            (unsigned long)m->bad_frames);
        canvas_draw_str(c, 1, 43, buf);
        canvas_draw_str(c, 1, 53, "Hold OK:WiFi  Hold >:Save");
    } else if(!alive || r->state >= 4) {
        const char* l1;
        const char* l2;
        if(m->busy) {
            l1 = "UART in use by another app";
            l2 = "Disable UART CLI, reopen";
        } else if(!alive) {
            l1 = "Connect ESP32-S2 board";
            l2 = "Check power / firmware v2";
        } else if(r->state == 6) {
            l1 = "Hold OK to enter Wi-Fi";
            l2 = "2.4 GHz WPA2/WPA3";
        } else if(r->state == 5) {
            l1 = "Connecting / reconnecting";
            l2 = "Hold OK to change Wi-Fi";
        } else {
            l1 = "Wi-Fi up, no fresh CSI";
            l2 = "Check router / ping replies";
        }
        canvas_draw_str(c, 1, 29, l1);
        canvas_draw_str(c, 1, 42, l2);
    } else {
        const int top = 15, height = 30;
        int32_t ymax = r->threshold * 3;
        if(ymax < 300) ymax = 300;
        for(unsigned i = 0; i < m->count; i++) {
            uint16_t v = m->graph[(m->head + GRAPH_POINTS - m->count + i) % GRAPH_POINTS];
            if(v != UINT16_MAX && v > ymax) ymax = v;
        }
        int ty = top + height - 1 - r->threshold * (height - 1) / ymax;
        for(int x = 1; x < 127; x += 4)
            canvas_draw_dot(c, x, ty);
        int px = -1, py = 0;
        for(unsigned i = 0; i < m->count; i++) {
            uint16_t v = m->graph[(m->head + GRAPH_POINTS - m->count + i) % GRAPH_POINTS];
            if(v == UINT16_MAX) {
                px = -1;
                continue;
            }
            int x = 1 + GRAPH_POINTS - m->count + i,
                y = top + height - 1 - v * (height - 1) / ymax;
            if(px >= 0) canvas_draw_line(c, px, py, x, y);
            px = x;
            py = y;
        }
        snprintf(
            buf,
            sizeof(buf),
            "S %ld.%03ld  T %ld.%03ld",
            (long)(r->score / 1000),
            (long)(r->score % 1000),
            (long)(r->threshold / 1000),
            (long)(r->threshold % 1000));
        canvas_draw_str(c, 1, 54, buf);
    }
    if(m->toast[0] && recent(m->toast_tick, 1800))
        canvas_draw_str(c, 1, 64, m->toast);
    else
        canvas_draw_str(
            c, 1, 64, m->details ? "<:Graph >:Mute Back:Exit" : "OK:Cal +/-:Thr <:Info");
}
static void text_done(void* ctx) {
    RadarApp* a = ctx;
    view_dispatcher_send_custom_event(a->dispatcher, EVENT_TEXT_DONE);
}
static void edit_wifi(RadarApp* a, unsigned stage) {
    a->editing = stage;
    text_input_reset(a->text);
    text_input_set_header_text(
        a->text, stage == 1 ? "Wi-Fi SSID (2.4 GHz)" : "Password (visible input)");
    text_input_set_minimum_length(a->text, stage == 1 ? 1 : 8);
    text_input_set_result_callback(
        a->text,
        text_done,
        a,
        stage == 1 ? a->ssid : a->password,
        stage == 1 ? sizeof(a->ssid) : sizeof(a->password),
        false);
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_TEXT);
}
static bool custom_event(void* ctx, uint32_t event) {
    RadarApp* a = ctx;
    if(event != EVENT_TEXT_DONE) return false;
    if(a->editing == 1)
        edit_wifi(a, 2);
    else if(a->editing == 2) {
        char sh[65], ph[127], cmd[210];
        csi_hex_encode(a->ssid, sh, sizeof(sh));
        csi_hex_encode(a->password, ph, sizeof(ph));
        snprintf(cmd, sizeof(cmd), "WIFI %s %s", sh, ph);
        send_command(a, cmd, "WIFI");
        memset(ph, 0, sizeof(ph));
        memset(cmd, 0, sizeof(cmd));
        memset(a->password, 0, sizeof(a->password));
        a->editing = 0;
        view_dispatcher_switch_to_view(a->dispatcher, VIEW_RADAR);
    }
    return true;
}
static bool navigate(void* ctx) {
    RadarApp* a = ctx;
    if(a->editing) {
        a->editing = 0;
        memset(a->password, 0, sizeof(a->password));
        view_dispatcher_switch_to_view(a->dispatcher, VIEW_RADAR);
        return true;
    }
    view_dispatcher_stop(a->dispatcher);
    return true;
}
static bool input(InputEvent* e, void* ctx) {
    RadarApp* a = ctx;
    if(e->key == InputKeyBack) return false;
    if(e->type == InputTypeLong && e->key == InputKeyOk) {
        if(a->pending[0])
            toast(a, "Waiting for ESP32");
        else
            edit_wifi(a, 1);
        return true;
    }
    if(e->type == InputTypeLong && e->key == InputKeyRight) {
        send_command(a, "SAVE", "SAVE");
        return true;
    }
    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return true;
    if(e->key == InputKeyLeft && e->type == InputTypeShort) {
        with_view_model(a->radar, RadarModel * m, { m->details = !m->details; }, true);
    } else if(e->key == InputKeyRight && e->type == InputTypeShort) {
        with_view_model(a->radar, RadarModel * m, { m->muted = !m->muted; }, true);
    } else if(e->key == InputKeyOk && e->type == InputTypeShort)
        send_command(a, "CAL 100", "CAL");
    else if(e->key == InputKeyUp || e->key == InputKeyDown) {
        int32_t t = 0;
        with_view_model(
            a->radar,
            RadarModel * m,
            {
                if(m->received && recent(m->last_rx, LINK_TIMEOUT_MS)) t = m->report.threshold;
            },
            false);
        if(!t) {
            toast(a, "No ESP32 telemetry");
            return true;
        }
        t += e->key == InputKeyUp ? 10 : -10;
        if(t < CSI_THR_MIN) t = CSI_THR_MIN;
        if(t > CSI_THR_MAX) t = CSI_THR_MAX;
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "THR %ld", (long)t);
        if(send_command(a, cmd, "THR")) a->desired_threshold = t;
    }
    return true;
}
static void tick(void* ctx) {
    RadarApp* a = ctx;
    bool moving = false, muted = false;
    uint32_t reply_id = 0;
    char reply[40] = {0};
    int32_t threshold = 0;
    with_view_model(
        a->radar,
        RadarModel * m,
        {
            bool alive = m->received && recent(m->last_rx, LINK_TIMEOUT_MS);
            moving = alive && m->report.age < LINK_TIMEOUT_MS && m->report.state == 2;
            muted = m->muted;
            reply_id = m->last_reply;
            threshold = m->report.threshold;
            snprintf(reply, sizeof(reply), "%s", m->reply);
        },
        true);
    if(moving && !a->was_moving && !muted && !a->editing) {
        notification_message(a->notifications, &sequence_single_vibro);
        notification_message(a->notifications, &sequence_blink_blue_100);
    }
    a->was_moving = moving;
    if(reply_id != a->reply_seen) {
        a->reply_seen = reply_id;
        if(a->pending[0]) {
            char expected[20];
            snprintf(expected, sizeof(expected), "ACK,%s", a->pending);
            if(strncmp(reply, "ERR,", 4) == 0) {
                a->pending[0] = 0;
                toast(a, reply);
            } else if(strcmp(reply, expected) == 0) {
                if(strcmp(a->pending, "THR") != 0) {
                    a->pending[0] = 0;
                    toast(a, "ESP32 confirmed");
                }
            }
        }
    }
    if(strcmp(a->pending, "THR") == 0 && threshold == a->desired_threshold) {
        a->pending[0] = 0;
        toast(a, "Threshold applied");
    }
    if(a->pending[0] && !recent(a->sent_tick, 3000)) {
        a->pending[0] = 0;
        toast(a, "No reply - try again");
    }
}
int32_t csi_radar_app(void* context) {
    UNUSED(context);
    RadarApp* a = malloc(sizeof(RadarApp));
    furi_check(a);
    memset(a, 0, sizeof(*a));
    atomic_init(&a->rx_lost, false);
    a->stream = furi_stream_buffer_alloc(1024, 1);
    a->dispatcher = view_dispatcher_alloc();
    a->radar = view_alloc();
    view_allocate_model(a->radar, ViewModelTypeLocking, sizeof(RadarModel));
    view_set_context(a->radar, a);
    view_set_draw_callback(a->radar, draw);
    view_set_input_callback(a->radar, input);
    a->text = text_input_alloc();
    view_dispatcher_add_view(a->dispatcher, VIEW_RADAR, a->radar);
    view_dispatcher_add_view(a->dispatcher, VIEW_TEXT, text_input_get_view(a->text));
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
    with_view_model(a->radar, RadarModel * m, { m->busy = a->serial == NULL; }, false);
    a->worker = furi_thread_alloc_ex("CsiRadarRx", 2048, worker, a);
    furi_thread_start(a->worker);
    if(a->serial) {
        furi_hal_serial_init(a->serial, 115200);
        furi_hal_serial_async_rx_start(a->serial, rx_callback, a, true);
    }
    view_dispatcher_switch_to_view(a->dispatcher, VIEW_RADAR);
    view_dispatcher_run(a->dispatcher);
    /* Stop producer first, then consumer, then free shared state. */
    if(a->serial) furi_hal_serial_async_rx_stop(a->serial);
    furi_thread_flags_set(furi_thread_get_id(a->worker), RX_STOP);
    furi_thread_join(a->worker);
    furi_thread_free(a->worker);
    if(a->serial) {
        furi_hal_serial_tx_wait_complete(a->serial);
        furi_hal_serial_deinit(a->serial);
        furi_hal_serial_control_release(a->serial);
    }
    expansion_enable(a->expansion);
    view_dispatcher_remove_view(a->dispatcher, VIEW_TEXT);
    view_dispatcher_remove_view(a->dispatcher, VIEW_RADAR);
    text_input_free(a->text);
    view_free(a->radar);
    view_dispatcher_free(a->dispatcher);
    furi_stream_buffer_free(a->stream);
    furi_record_close(RECORD_EXPANSION);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    memset(a->password, 0, sizeof(a->password));
    free(a);
    return 0;
}
