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
#include "config.h" // UI_HEARTBEAT_TIMEOUT_MS, SESSION_API_BODY_MAX
#include "hal/hal_api.h"
#include "ui/ui_core1.h"
#include "ui/ui_config.h" // UI_STATUS_PERIOD_MS, UI_API_POLL_PERIOD_MS, UI_API_MOTIVO_MAX
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

// Envío del resultado a la API (Paso C): el core 0 arma el cuerpo en s_tx_body y lo entrega;
// el core 1 (tx_pump_cb) se lo pasa al cliente HTTP (hal_api) y muestra el estado real.
static critical_section_t s_tx_cs;
static char s_tx_body[SESSION_API_BODY_MAX]; // buffer compartido del cuerpo JSON (peor caso)
static uint32_t s_tx_len = 0u;
static int32_t s_tx_paciente = 0;
static bool s_tx_pending = false;            // cuerpo listo, sin entregar aún a hal_api
static bool s_tx_inflight = false;           // hal_api está usando el buffer (no pisarlo)
static bool s_tx_skip = false;               // la sesión no se envía (motivo abajo)
static char s_tx_skip_motivo[UI_API_MOTIVO_MAX] = "";

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

// --- Envío a la API: lado core 0 (arma el cuerpo, confirma o descarta) --------
bool ui_app_tx_busy(void) {
    bool busy;
    critical_section_enter_blocking(&s_tx_cs);
    busy = s_tx_pending || s_tx_inflight;
    critical_section_exit(&s_tx_cs);
    return busy;
}

char *ui_app_tx_buffer(uint32_t *cap) {
    if (cap != NULL) {
        *cap = (uint32_t)sizeof(s_tx_body);
    }
    return s_tx_body;
}

void ui_app_tx_commit(uint32_t len, int32_t paciente) {
    // El core 0 terminó de escribir s_tx_body (plain stores); la critical_section ordena esas
    // escrituras antes de publicar el flag (barrera de memoria entre núcleos).
    critical_section_enter_blocking(&s_tx_cs);
    s_tx_len = len;
    s_tx_paciente = paciente;
    s_tx_pending = true;
    s_tx_skip = false; // un envío real reemplaza cualquier aviso de "no enviada" anterior
    critical_section_exit(&s_tx_cs);
}

void ui_app_tx_skip(const char *motivo) {
    critical_section_enter_blocking(&s_tx_cs);
    (void)snprintf(s_tx_skip_motivo, sizeof(s_tx_skip_motivo), "%s", motivo);
    s_tx_skip = true;
    critical_section_exit(&s_tx_cs);
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

// Bombeo del envío a la API (core 1): entrega el cuerpo pendiente al cliente HTTP, atiende su
// máquina de estados y refleja el estado REAL en la pantalla de fin. Corre siempre (los
// reintentos siguen aunque se cambie de pantalla); el panel solo lo muestra en ScreenEnding.
static void tx_pump_cb(lv_timer_t *timer) {
    (void)timer;

    // 1) Si el core 0 dejó un cuerpo listo y el cliente está libre, entregárselo.
    bool skip = false;
    char skip_motivo[UI_API_MOTIVO_MAX];
    critical_section_enter_blocking(&s_tx_cs);
    if (s_tx_pending && !hal_api_busy() && !s_tx_inflight) {
        s_tx_pending = false;
        s_tx_inflight = true; // el buffer queda tomado hasta que hal_api termine
        hal_api_send_session(s_tx_body, s_tx_len, s_tx_paciente);
    }
    skip = s_tx_skip;
    if (skip) {
        (void)snprintf(skip_motivo, sizeof(skip_motivo), "%s", s_tx_skip_motivo);
    }
    critical_section_exit(&s_tx_cs);

    // 2) Atender el cliente HTTP (conexión, envío por partes, respuesta, reintentos).
    hal_api_poll();

    // 3) Liberar el buffer cuando el envío terminó (enviado o error definitivo).
    const hal_api_state_t api_state = hal_api_get_state();
    if (s_tx_inflight && ((api_state == HAL_API_SENT) || (api_state == HAL_API_ERROR))) {
        critical_section_enter_blocking(&s_tx_cs);
        s_tx_inflight = false;
        critical_section_exit(&s_tx_cs);
    }

    // 4) Reflejar el estado en la pantalla de fin. El aviso de "no enviada" (skip) tiene
    //    prioridad: no hubo POST, pero hay que mostrar el motivo.
    ui_panel_tx_t disp;
    const char *motivo = "";
    if (skip) {
        disp = UI_PANEL_TX_ERROR;
        motivo = skip_motivo;
    } else {
        switch (api_state) {
        case HAL_API_SENDING:  disp = UI_PANEL_TX_SENDING;  break;
        case HAL_API_RETRYING: disp = UI_PANEL_TX_RETRYING; break;
        case HAL_API_SENT:     disp = UI_PANEL_TX_SENT;     break;
        case HAL_API_ERROR:    disp = UI_PANEL_TX_ERROR; motivo = hal_api_error_motivo(); break;
        case HAL_API_IDLE:
        default:               disp = s_tx_pending ? UI_PANEL_TX_SENDING : UI_PANEL_TX_IDLE; break;
        }
    }
    ui_panel_set_tx_status(disp, motivo);
}

// Arranque del panel en el core 1 (después de lv_init + display + hal_wifi_init en ui_core1).
static void panel_core1_setup(void) {
    ui_panel_init();
    ui_panel_set_event_cb(panel_event_cb);
    (void)hal_api_init(); // cliente HTTP de la API (el CYW43 ya lo inició hal_wifi_init)
    (void)lv_timer_create(status_pump_cb, UI_STATUS_PERIOD_MS, NULL);
    (void)lv_timer_create(tx_pump_cb, UI_API_POLL_PERIOD_MS, NULL);
    s_core1_beat_ms = now_ms();
    s_core1_ready = true; // recién ahora el core 0 empieza a vigilar el latido (ajuste S-D.1)
}

void ui_app_init(void) {
    critical_section_init(&s_status_cs);
    critical_section_init(&s_evt_cs);
    critical_section_init(&s_sess_cs);
    critical_section_init(&s_tx_cs);
    s_status_valid = false;
    s_evt_head = 0u;
    s_evt_tail = 0u;
    s_sess_unread = false;
    s_tx_pending = false;
    s_tx_inflight = false;
    s_tx_skip = false;
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
