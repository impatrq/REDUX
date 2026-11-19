// ui_panel_fw.c · Interfaz PANEL del lado del firmware (REDUX_UI=PANEL). Hace dos cosas:
//
//  1) Implementa el enganche del core 0 (fw_ui.h: ui_init/tick/poll/message/set_waiting y
//     ui_feed_line) para que el ciclo de control use el panel como usaba al pulsador.
//  2) Implementa el contrato cruzado app/ui.h (ui_app_*): el core 0 publica el estado y lee
//     eventos; el core 1 corre LVGL + el panel SquareLine + el Wi-Fi. La comunicación entre
//     núcleos usa critical_section de la Pico (buffers cortos, el lock solo durante el memcpy).
//
// El core 1 se lanza con ui_core1_start() (lv_init + display + hal_wifi_init + este setup).
// Latido: el core 1 marca un tiempo en cada pulso de estado; el core 0 lo vigila con
// ui_app_heartbeat_lost(), pero recién después de que el core 1 avisó que terminó de arrancar.

#include "fw_ui.h"

#include <stdio.h>
#include <string.h>

#include "pico/sync.h" // critical_section_t
#include "pico/time.h" // to_ms_since_boot, get_absolute_time

#include "lvgl/lvgl.h"

#include "app/ui.h"
#include "config.h" // UI_HEARTBEAT_TIMEOUT_MS
#include "ui/ui_core1.h"
#include "ui/ui_config.h" // UI_STATUS_PERIOD_MS
#include "ui/ui_panel.h"

#define UI_EVT_QUEUE_LEN 16u // eventos de la UI encolados antes de descartar

// --- Estado compartido entre núcleos (protegido por critical_section) --------
static critical_section_t s_status_cs;
static ui_status_t s_status;          // último estado publicado por el core 0
static bool s_status_valid = false;   // false hasta el primer publish

static critical_section_t s_evt_cs;
static ui_event_t s_evt[UI_EVT_QUEUE_LEN]; // cola de eventos del core 1 hacia el core 0
static uint32_t s_evt_head = 0u;
static uint32_t s_evt_tail = 0u;

static critical_section_t s_sess_cs;
static ui_event_msg_t s_sess;          // buffer de último valor para START_SESSION
static bool s_sess_unread = false;

// Latido del core 1 (lectura/escritura de 32 bits: atómica en RP2350).
static volatile uint32_t s_core1_beat_ms = 0u;
static volatile bool s_core1_ready = false; // true cuando el core 1 terminó de arrancar

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

// --- Contrato cruzado: lado core 0 (publica estado, lee eventos y sesión) ----
void ui_app_publish_status(const ui_status_t *status) {
    critical_section_enter_blocking(&s_status_cs);
    s_status = *status;
    s_status_valid = true;
    critical_section_exit(&s_status_cs);
}

bool ui_app_poll_event(ui_event_t *event) {
    bool got = false;
    critical_section_enter_blocking(&s_evt_cs);
    if (s_evt_head != s_evt_tail) {
        *event = s_evt[s_evt_head];
        s_evt_head = (s_evt_head + 1u) % UI_EVT_QUEUE_LEN;
        got = true;
    }
    critical_section_exit(&s_evt_cs);
    return got;
}

bool ui_app_take_session(ui_event_msg_t *out) {
    bool got = false;
    critical_section_enter_blocking(&s_sess_cs);
    if (s_sess_unread) {
        *out = s_sess;
        s_sess_unread = false;
        got = true;
    }
    critical_section_exit(&s_sess_cs);
    return got;
}

// --- Contrato cruzado: lado core 1 (lee estado, publica eventos y sesión) ----
// publica la sesión (buffer de último valor). La llama el panel al mandar START_SESSION.
void ui_app_publish_session(const ui_event_msg_t *msg) {
    critical_section_enter_blocking(&s_sess_cs);
    s_sess = *msg;
    s_sess_unread = true;
    critical_section_exit(&s_sess_cs);
}

// Encola un evento del core 1 hacia el core 0; si la cola está llena lo descarta.
static void evt_push(ui_event_t ev) {
    critical_section_enter_blocking(&s_evt_cs);
    const uint32_t next = (s_evt_tail + 1u) % UI_EVT_QUEUE_LEN;
    if (next != s_evt_head) {
        s_evt[s_evt_tail] = ev;
        s_evt_tail = next;
    }
    critical_section_exit(&s_evt_cs);
}

// Recibe cada evento del panel (core 1, dentro de LVGL): START_SESSION lleva brazo + ID por
// el buffer de sesión; todos se encolan hacia el core 0.
static void panel_event_cb(const ui_event_msg_t *msg) {
    if (msg->type == UI_EV_START_SESSION) {
        ui_app_publish_session(msg);
    }
    evt_push(msg->type);
}

// Pulso de estado del core 1: copia el último estado publicado y lo pinta; marca el latido.
static void status_pump_cb(lv_timer_t *timer) {
    (void)timer;
    ui_status_t local;
    bool valid;
    critical_section_enter_blocking(&s_status_cs);
    local = s_status;
    valid = s_status_valid;
    critical_section_exit(&s_status_cs);
    if (valid) {
        ui_panel_show_status(&local);
    }
    s_core1_beat_ms = now_ms();
}

// Arranque del panel en el core 1 (después de lv_init + display + hal_wifi_init en ui_core1).
static void panel_core1_setup(void) {
    ui_panel_init();
    ui_panel_set_event_cb(panel_event_cb);
    (void)lv_timer_create(status_pump_cb, UI_STATUS_PERIOD_MS, NULL);
    s_core1_beat_ms = now_ms();
    s_core1_ready = true; // recién ahora el core 0 empieza a vigilar el latido (ajuste S-D.1)
}

void ui_app_init(void) {
    critical_section_init(&s_status_cs);
    critical_section_init(&s_evt_cs);
    critical_section_init(&s_sess_cs);
    s_status_valid = false;
    s_evt_head = 0u;
    s_evt_tail = 0u;
    s_sess_unread = false;
    s_core1_ready = false;
    s_core1_beat_ms = now_ms();
    ui_core1_start(panel_core1_setup); // lanza el core 1 (LVGL + panel + Wi-Fi)
}

// true si el core 1 ya arrancó y hace más de UI_HEARTBEAT_TIMEOUT_MS que no late. Mientras el
// core 1 no avisó que terminó de arrancar, devuelve false (no se vigila).
bool ui_app_heartbeat_lost(void) {
    if (!s_core1_ready) {
        return false;
    }
    return (now_ms() - s_core1_beat_ms) > UI_HEARTBEAT_TIMEOUT_MS;
}

// --- Enganche del core 0 (fw_ui.h) -------------------------------------------
void fw_ui_init(void) { ui_app_init(); }

void ui_tick(void) { /* El panel corre en el core 1; el core 0 no tiene trabajo de UI. */ }

ui_event_t ui_poll(void) {
    ui_event_t ev;
    return ui_app_poll_event(&ev) ? ev : UI_EV_NONE;
}

void ui_message(const char *msg) { printf("%s\n", msg); }

void ui_set_waiting(bool waiting) { (void)waiting; /* el panel lo muestra con el prompt */ }

void ui_feed_line(const char *line) {
    if (strcmp(line, "ok") == 0) {
        evt_push(UI_EV_CONFIRM);
    } else if (strcmp(line, "cancel") == 0) {
        evt_push(UI_EV_CANCEL);
    } else if (strcmp(line, "recal") == 0) {
        evt_push(UI_EV_RECALIBRATE);
    }
}
