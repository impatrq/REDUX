/**
 * @file ui_panel.c
 * @brief Pantallas de SquareLine conectadas al flujo de REDUX (pantalla + teclado).
 *
 * No edita el export: solo usa sus objetos (ui_*) y les agrega callbacks propios.
 */
#include "ui/ui_panel.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hal/hal_wifi.h"
#include "lvgl/lvgl.h"
#include "ui/lv_port_disp.h"
#include "ui/lv_port_indev.h"
#include "ui/squareline/ui.h"
#include "ui/ui_config.h"

#define UI_PANEL_TEXT_LEN        48u       /* Textos con números ("Potencia 100%") */
#define UI_PANEL_DEG_LIMIT       9999.0f   /* Valores fuera de ±esto (o NaN) se muestran como 0 */
#define UI_PANEL_PERCENT_FULL    100.0f
#define UI_PANEL_PERCENT_MAX     100
#define UI_PANEL_ROUND_HALF      0.5f
#define UI_PANEL_ENDING_ANIM_MS  500u      /* Marcha -> ScreenEnding, igual que Main -> ScreenBegin */
#define UI_PANEL_ONE_SHOT        1         /* Repeticiones de un timer que corre una sola vez */
#define UI_PANEL_MODE_ANIM_MS    300u      /* FADE en los cambios de pantalla por modo del firmware */
#define UI_PANEL_BOOT_MIN_MS     2000u     /* Inicio se muestra al menos esto antes de pasar a Main */

/* Lista de redes de Wifi: entre el título (y = -50) y el selector de Volver (empieza en y = 81 px). */
#define UI_PANEL_WIFI_LIST_W_PX      110
#define UI_PANEL_WIFI_LIST_H_PX      54        /* Dos redes enteras y parte de la tercera */
#define UI_PANEL_WIFI_LIST_Y_PX      (-12)     /* Desde el centro de la pantalla */
#define UI_PANEL_WIFI_ITEM_H_PX      20
#define UI_PANEL_WIFI_ITEM_GAP_PX    2
#define UI_PANEL_WIFI_ITEM_PAD_PX    4         /* Margen del texto a los lados */
#define UI_PANEL_WIFI_FOCUS_W_PX     2         /* Borde del botón con foco */
#define UI_PANEL_WIFI_OPEN_COLOR     0x6140A1  /* Abiertas: mismo violeta que los botones del diseño */
#define UI_PANEL_WIFI_SAVED_COLOR    0x2E7D5B  /* Guardadas (clave en wifi_redes.h): verde */
#define UI_PANEL_WIFI_LOCKED_COLOR   0x4A4A4A  /* Con clave y no guardadas: gris */
#define UI_PANEL_WIFI_LOCKED_TEXT    0xA0A0A0
#define UI_PANEL_WIFI_FOCUS_COLOR    0xFFFFFF

typedef struct {
    uint32_t bit;
    const char *text;
} ui_panel_fault_text_t;

/* Instrucciones de ScreenBegin; sin tildes (la fuente Montserrat 14 de LVGL es solo ASCII). */
static const char *const s_rom_prompt_text[] = {
    [UI_PROMPT_NONE]             = "Calibrando rango",
    [UI_PROMPT_GO_FLEX]          = "Flexiona lo más posible",
    [UI_PROMPT_GO_EXT]           = "Extiende lo más posible",
    [UI_PROMPT_RELAX]            = "Calibrando rango",
    [UI_PROMPT_CONTRACT_BICEPS]  = "Calibrando rango",
    [UI_PROMPT_CONTRACT_TRICEPS] = "Calibrando rango",
};
static const char *const s_emg_prompt_text[] = {
    [UI_PROMPT_NONE]             = "Calibrando sensores",
    [UI_PROMPT_GO_FLEX]          = "Calibrando sensores",
    [UI_PROMPT_GO_EXT]           = "Calibrando sensores",
    [UI_PROMPT_RELAX]            = "Relaja el brazo",
    [UI_PROMPT_CONTRACT_BICEPS]  = "Contrae el bíceps",
    [UI_PROMPT_CONTRACT_TRICEPS] = "Contrae el tríceps",
};
#define UI_PANEL_PROMPT_COUNT    (sizeof(s_rom_prompt_text) / sizeof(s_rom_prompt_text[0]))

/* Fallas en orden de prioridad: se muestra la primera activa. */
static const ui_panel_fault_text_t s_fault_text[] = {
    { UI_FAULT_OVERCURRENT, "Sobrecorriente en el motor" },
    { UI_FAULT_ENCODER,     "Falla del encoder" },
    { UI_FAULT_ANGLE_LIMIT, "Fuera del rango calibrado" },
    { UI_FAULT_EMG,         "Señal EMG inválida" },
    { UI_FAULT_OTHER,       "Falla del sistema" },
};
#define UI_PANEL_FAULT_COUNT     (sizeof(s_fault_text) / sizeof(s_fault_text[0]))
static const char s_fault_unknown_text[] = "Falla del sistema";

static const char s_sending_text[] = "Enviando datos...";
static const char s_sent_text[] = "Enviados!";
static const char s_measuring_text[] = "Midiendo...";

/* Título de Wifi (LblRedes) según el estado; cortos para que entren en 128 px. */
static const char s_wifi_title_text[] = "Redes";
static const char s_wifi_off_text[] = "Wifi apagado";
static const char s_wifi_lost_text[] = "Desconectado";
static const char s_wifi_scanning_text[] = "Buscando...";
static const char s_wifi_empty_text[] = "Sin redes";
static const char s_wifi_scan_failed_text[] = "Error al buscar";
static const char s_wifi_connecting_text[] = "Conectando...";
static const char s_wifi_connected_text[] = "Conectado";
static const char s_wifi_connect_failed_text[] = "Fallo conexión";
static const char s_wifi_locked_text[] = "Requiere clave";

static ui_panel_event_cb_t s_event_cb = NULL;
static bool s_has_mode = false;      /* false hasta el primer mensaje de estado */
static ui_mode_t s_mode = UI_MODE_BOOT;
static uint32_t s_boot_shown_ms = 0u; /* Tick en que se mostro Inicio (BOOT), para el minimo de 2 s */
static uint32_t s_last_press = 0u;   /* Última pulsación que generó un evento */
static lv_timer_t *s_send_timer = NULL;  /* "Enviando datos..." en curso */

/* Calibración: tras apretar "Guardar", la pantalla muestra "Midiendo..." hasta que el estado
 * (modo o prompt) avance. Se recuerda qué estaba midiéndose para detectar el avance. */
static bool s_measuring = false;
static ui_mode_t s_measuring_mode = UI_MODE_BOOT;
static ui_prompt_t s_measuring_prompt = UI_PROMPT_NONE;
static ui_prompt_t s_shown_prompt = UI_PROMPT_NONE;  /* Último prompt mostrado en ScreenBegin */

static lv_obj_t *s_wifi_list = NULL;                    /* Contenedor de los botones de redes */
static hal_wifi_net_t s_wifi_nets[UI_WIFI_MAX_NETS];    /* Red de cada botón, en el mismo orden */
static uint32_t s_wifi_net_count = 0u;
static char s_wifi_connected[HAL_WIFI_SSID_MAX_LEN + 1u];  /* "" si no hay conexión */
static uint32_t s_wifi_scan_gen = 0u;                   /* Última generación de escaneo dibujada */

static void ui_panel_emit(ui_event_t event)
{
    if (s_event_cb != NULL) {
        const ui_event_msg_t msg = { .type = event };  /* arm/patient_id solo en START_SESSION */
        s_event_cb(&msg);
    }
}

/** true la primera vez que se ve esta pulsación (ignora la repetición al mantener). */
static bool ui_panel_is_new_press(void)
{
    const uint32_t press = lv_port_indev_get_press_count();
    if (press == s_last_press) {
        return false;
    }
    s_last_press = press;
    return true;
}

/** Redondea al entero más cercano; 0 si el valor no es razonable (o es NaN). */
static int32_t ui_panel_round(float value)
{
    if (!((value > -UI_PANEL_DEG_LIMIT) && (value < UI_PANEL_DEG_LIMIT))) {
        return 0;
    }
    return (int32_t)((value >= 0.0f) ? (value + UI_PANEL_ROUND_HALF) : (value - UI_PANEL_ROUND_HALF));
}

/** Cambia el texto solo si es distinto (evita redibujar a 20 Hz). */
static void ui_panel_set_text(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

static const char *ui_panel_prompt_text(const char *const table[], ui_prompt_t prompt)
{
    const size_t index = (size_t)prompt;
    return (index < UI_PANEL_PROMPT_COUNT) ? table[index] : table[UI_PROMPT_NONE];
}

/** Pantalla visible, o la que se está cargando si hay una animación en curso. */
static lv_obj_t *ui_panel_current_screen(void)
{
    const lv_disp_t *disp = lv_disp_get_default();
    return (disp->scr_to_load != NULL) ? disp->scr_to_load : lv_scr_act();
}

/* ---- Teclado y grupos ---- */

/** Teclas fuera de LVGL: UI_KEY_ROTATE_CHAR gira la interfaz en cualquier pantalla. */
static void ui_panel_key_cb(char key)
{
    if (key == UI_KEY_ROTATE_CHAR) {
        (void)lv_port_disp_rotate_left();
    }
}

/** Al cargar una pantalla, el teclado pasa a su grupo (guardado en user_data de la pantalla). */
static void ui_panel_screen_loaded_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_target(e);
    lv_port_indev_set_group((lv_group_t *)lv_obj_get_user_data(screen));
}

/**
 * Al dejar una pantalla se borra la animación del selector que el export arranca en cada
 * SCREEN_LOADED (si no, se acumula una por visita). Llega antes del SCREEN_LOADED de la nueva.
 */
static void ui_panel_screen_unload_cb(lv_event_t *e)
{
    (void)e;
    (void)lv_anim_custom_del(NULL, _ui_anim_callback_set_opacity);
}

/** El selector del diseño (user_data) se mueve a la altura del objeto con foco. */
static void ui_panel_selector_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *selector = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_set_y(selector, lv_obj_get_style_y(target, LV_PART_MAIN));
}

/**
 * Con el foco en un objeto que tiene su propio borde (campo de ID, redes de Wifi), el selector
 * del diseño (user_data) se oculta; vuelve al perder el foco.
 */
static void ui_panel_hide_selector_cb(lv_event_t *e)
{
    lv_obj_t *selector = (lv_obj_t *)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        lv_obj_add_flag(selector, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(selector, LV_OBJ_FLAG_HIDDEN);
    }
}

/** D = el botón Volver de la pantalla (user_data). */
static void ui_panel_back_key_cb(lv_event_t *e)
{
    lv_obj_t *back_button = (lv_obj_t *)lv_event_get_user_data(e);
    if ((lv_event_get_key(e) == LV_KEY_ESC) && ui_panel_is_new_press()) {
        (void)lv_event_send(back_button, LV_EVENT_CLICKED, NULL);
    }
}

static lv_group_t *ui_panel_attach_group(lv_obj_t *screen)
{
    lv_group_t *group = lv_group_create();
    lv_obj_set_user_data(screen, group);
    lv_obj_add_event_cb(screen, ui_panel_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(screen, ui_panel_screen_unload_cb, LV_EVENT_SCREEN_UNLOAD_START, NULL);
    return group;
}

/** Botón: entra al grupo, mueve el selector del diseño y no usa el borde del tema. */
static void ui_panel_add_button(lv_group_t *group, lv_obj_t *button, lv_obj_t *selector)
{
    lv_obj_set_style_outline_width(button, 0, LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(button, ui_panel_selector_cb, LV_EVENT_FOCUSED, selector);
    lv_group_add_obj(group, button);
}

/** En los botones de la lista, D aprieta back_button. */
static void ui_panel_set_back_key(lv_obj_t *const buttons[], size_t count, lv_obj_t *back_button)
{
    for (size_t i = 0u; i < count; i++) {
        lv_obj_add_event_cb(buttons[i], ui_panel_back_key_cb, LV_EVENT_KEY, back_button);
    }
}

/* ---- Main y Ajustes ---- */

static void ui_panel_setup_menus(void)
{
    /* Main: de arriba a abajo; arranca en Comenzar. "Comenzar" navega a ScreenBrazo (evento del export). */
    lv_group_t *main_group = ui_panel_attach_group(ui_Main);
    ui_panel_add_button(main_group, ui_SettignsButton, ui_MainSelec);
    ui_panel_add_button(main_group, ui_BeginButton, ui_MainSelec);
    ui_panel_add_button(main_group, ui_ProfileButton, ui_MainSelec);
    lv_group_focus_obj(ui_BeginButton);

    /* Ajustes: Wifi arriba, Volver abajo; arranca en Wifi. */
    lv_group_t *settings_group = ui_panel_attach_group(ui_SettingScreen);
    ui_panel_add_button(settings_group, ui_WifiButton, ui_SettingSelect);
    ui_panel_add_button(settings_group, ui_BackButton, ui_SettingSelect);
    lv_group_focus_obj(ui_WifiButton);
    lv_obj_t *const settings_buttons[] = { ui_WifiButton, ui_BackButton };
    ui_panel_set_back_key(settings_buttons, sizeof(settings_buttons) / sizeof(settings_buttons[0]),
                          ui_BackButton);

}

/* ---- Pantalla de brazo: elige izquierdo/derecho e inicia la sesión ---- */

/** START_SESSION con el brazo elegido y el ID escrito en Perfil (ui_TextArea1); "" si está vacío. */
static void ui_panel_start_session(ui_arm_t arm)
{
    if (s_event_cb == NULL) {
        return;
    }
    ui_event_msg_t msg = { .type = UI_EV_START_SESSION, .arm = arm };
    (void)snprintf(msg.patient_id, sizeof(msg.patient_id), "%s", lv_textarea_get_text(ui_TextArea1));
    s_event_cb(&msg);
}

static void ui_panel_left_arm_cb(lv_event_t *e)
{
    (void)e;
    ui_panel_start_session(UI_ARM_LEFT);
}

static void ui_panel_right_arm_cb(lv_event_t *e)
{
    (void)e;
    ui_panel_start_session(UI_ARM_RIGHT);
}

static void ui_panel_setup_arm(void)
{
    /* Izquierdo arriba, Derecho, Volver abajo; arranca en Izquierdo. Los botones de brazo no
     * cambian de pantalla: al apretarlos se manda START_SESSION y el modo del firmware carga Begin. */
    lv_group_t *group = ui_panel_attach_group(ui_ScreenBrazo);
    ui_panel_add_button(group, ui_LeftArmButton, ui_BrazoSelec);
    ui_panel_add_button(group, ui_RightArmButton, ui_BrazoSelec);
    ui_panel_add_button(group, ui_BackButtonBrazo, ui_BrazoSelec);
    lv_group_focus_obj(ui_LeftArmButton);
    lv_obj_add_event_cb(ui_LeftArmButton, ui_panel_left_arm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_RightArmButton, ui_panel_right_arm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *const arm_buttons[] = { ui_LeftArmButton, ui_RightArmButton, ui_BackButtonBrazo };
    ui_panel_set_back_key(arm_buttons, sizeof(arm_buttons) / sizeof(arm_buttons[0]), ui_BackButtonBrazo);
}

/* ---- Wifi: lista de redes (hal_wifi, en el core 0, escanea y conecta) ---- */

/** Texto del botón de una red: la conectada lleva un tilde adelante. */
static void ui_panel_wifi_label_text(char *text, size_t len, const hal_wifi_net_t *net)
{
    const bool connected = (strcmp(net->ssid, s_wifi_connected) == 0);
    /* Marcador de la red conectada en Latin-1 ("»"): la fuente por defecto (Font14Latin,
     * rango 0x20-0xFF) no trae los símbolos FontAwesome de LV_SYMBOL_*. */
    /* Precisión acotada al SSID: el nombre entra siempre en el buffer (y lo sabe el compilador). */
    (void)snprintf(text, len, "%s%.*s", connected ? "» " : "",
                   (int)HAL_WIFI_SSID_MAX_LEN, net->ssid);
}

/** Reescribe los textos de los botones (cambió la red conectada). Botón i = red i. */
static void ui_panel_wifi_refresh_labels(void)
{
    char text[UI_PANEL_TEXT_LEN];
    for (uint32_t i = 0u; i < s_wifi_net_count; i++) {
        lv_obj_t *label = lv_obj_get_child(lv_obj_get_child(s_wifi_list, (int32_t)i), 0);
        ui_panel_wifi_label_text(text, sizeof(text), &s_wifi_nets[i]);
        ui_panel_set_text(label, text);
    }
}

/** B sobre una red: abiertas y guardadas conectan (la guardada con su clave); las que piden
 *  clave y no están guardadas solo avisan (no hay cómo escribir la clave). */
static void ui_panel_wifi_net_cb(lv_event_t *e)
{
    const uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (index >= s_wifi_net_count) {
        return;
    }
    const hal_wifi_net_t *net = &s_wifi_nets[index];
    if (net->open || net->saved) {
        hal_wifi_request_connect(net->ssid);
        ui_panel_set_text(ui_LblRedes, s_wifi_connecting_text);
    } else {
        ui_panel_set_text(ui_LblRedes, s_wifi_locked_text);
    }
}

/** Deja el label de una red en UNA sola línea con el modo dado (DOT recorta, SCROLL se desplaza). */
static void ui_panel_wifi_set_label_line(lv_obj_t *label, lv_label_long_mode_t mode)
{
    lv_label_set_long_mode(label, mode);
    lv_obj_set_width(label, lv_pct(100));  /* ancho fijo: set_long_mode puede volverlo a content */
    lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, LV_PART_MAIN)));
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
}

/** La red con foco desplaza su nombre (SCROLL_CIRCULAR); las demás lo recortan con "..." (DOT). */
static void ui_panel_wifi_focus_cb(lv_event_t *e)
{
    lv_obj_t *label = lv_obj_get_child(lv_event_get_target(e), 0);
    const bool focused = (lv_event_get_code(e) == LV_EVENT_FOCUSED);
    ui_panel_wifi_set_label_line(label, focused ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT);
}

static void ui_panel_wifi_add_net(lv_group_t *group, uint32_t index)
{
    const hal_wifi_net_t *net = &s_wifi_nets[index];

    lv_obj_t *button = lv_btn_create(s_wifi_list);
    lv_obj_set_size(button, lv_pct(100), UI_PANEL_WIFI_ITEM_H_PX);
    lv_obj_add_flag(button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_pad_ver(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(button, UI_PANEL_WIFI_ITEM_PAD_PX, LV_PART_MAIN);
    const uint32_t bg_color = net->open    ? UI_PANEL_WIFI_OPEN_COLOR
                            : (net->saved ? UI_PANEL_WIFI_SAVED_COLOR : UI_PANEL_WIFI_LOCKED_COLOR);
    lv_obj_set_style_bg_color(button, lv_color_hex(bg_color), LV_PART_MAIN);
    lv_obj_set_style_outline_width(button, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_border_color(button, lv_color_hex(UI_PANEL_WIFI_FOCUS_COLOR), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(button, UI_PANEL_WIFI_FOCUS_W_PX, LV_STATE_FOCUSED);

    lv_obj_t *label = lv_label_create(button);
    ui_panel_wifi_set_label_line(label, LV_LABEL_LONG_DOT);  /* una sola línea (sin foco: recortada) */
    if (!net->open && !net->saved) {
        lv_obj_set_style_text_color(label, lv_color_hex(UI_PANEL_WIFI_LOCKED_TEXT), LV_PART_MAIN);
    }
    char text[UI_PANEL_TEXT_LEN];
    ui_panel_wifi_label_text(text, sizeof(text), net);
    lv_label_set_text(label, text);

    lv_obj_add_event_cb(button, ui_panel_wifi_net_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)index);
    lv_obj_add_event_cb(button, ui_panel_hide_selector_cb, LV_EVENT_FOCUSED, ui_SettingSelect1);
    lv_obj_add_event_cb(button, ui_panel_hide_selector_cb, LV_EVENT_DEFOCUSED, ui_SettingSelect1);
    lv_obj_add_event_cb(button, ui_panel_wifi_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(button, ui_panel_wifi_focus_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(button, ui_panel_back_key_cb, LV_EVENT_KEY, ui_BackButton1);
    lv_group_add_obj(group, button);
}

/**
 * Rehace los botones con el resultado de un escaneo (ya viene ordenado: abiertas primero).
 * Las redes entran al grupo después de Volver; como el foco da la vuelta, A/C recorren
 * igual que en pantalla: redes de arriba a abajo y después Volver.
 */
/** Rehace los botones con las redes ya copiadas en s_wifi_nets/s_wifi_net_count. */
static void ui_panel_wifi_build_list(void)
{
    lv_group_t *group = (lv_group_t *)lv_obj_get_user_data(ui_WifiScreen);
    lv_group_focus_obj(ui_BackButton1);  /* Que el foco no quede en un botón que se borra */
    lv_obj_clean(s_wifi_list);

    for (uint32_t i = 0u; i < s_wifi_net_count; i++) {
        ui_panel_wifi_add_net(group, i);
    }
    if (s_wifi_net_count > 0u) {
        lv_group_focus_obj(lv_obj_get_child(s_wifi_list, 0));
    }
}

static const char *ui_panel_wifi_title(hal_wifi_state_t state, const char *connected, uint32_t net_count)
{
    const char *ready_text = (connected[0] != '\0') ? s_wifi_connected_text : s_wifi_title_text;
    switch (state) {
    case HAL_WIFI_STATE_OFF:            return s_wifi_off_text;
    case HAL_WIFI_STATE_SCANNING:       return s_wifi_scanning_text;
    case HAL_WIFI_STATE_SCAN_DONE:      return (net_count == 0u) ? s_wifi_empty_text : ready_text;
    case HAL_WIFI_STATE_SCAN_FAILED:    return s_wifi_scan_failed_text;
    case HAL_WIFI_STATE_CONNECTING:     return s_wifi_connecting_text;
    case HAL_WIFI_STATE_CONNECTED:      return s_wifi_connected_text;
    case HAL_WIFI_STATE_CONNECT_FAILED: return s_wifi_connect_failed_text;
    case HAL_WIFI_STATE_IDLE:
    default:                            return s_wifi_lost_text;  /* IDLE solo llega si se cae la conexión */
    }
}

/**
 * Relee el estado del Wi-Fi (todo en el core 1): rehace la lista cuando hay un escaneo nuevo,
 * actualiza el tilde de la red conectada y el título.
 */
static void ui_panel_wifi_report_cb(lv_timer_t *timer)
{
    (void)timer;
    const hal_wifi_state_t state = hal_wifi_get_state();

    const uint32_t gen = hal_wifi_scan_generation();
    if (gen != s_wifi_scan_gen) {
        s_wifi_scan_gen = gen;
        s_wifi_net_count = hal_wifi_get_nets(s_wifi_nets, UI_WIFI_MAX_NETS);
        ui_panel_wifi_build_list();
    }

    const char *connected = hal_wifi_connected_ssid();
    if (strcmp(connected, s_wifi_connected) != 0) {
        (void)snprintf(s_wifi_connected, sizeof(s_wifi_connected), "%s", connected);
        ui_panel_wifi_refresh_labels();
    }

    ui_panel_set_text(ui_LblRedes, ui_panel_wifi_title(state, s_wifi_connected, s_wifi_net_count));
}

/** Cada vez que se entra a Wifi se busca de nuevo. */
static void ui_panel_wifi_loaded_cb(lv_event_t *e)
{
    (void)e;
    hal_wifi_request_scan();
    ui_panel_set_text(ui_LblRedes, s_wifi_scanning_text);
}

static void ui_panel_setup_wifi(void)
{
    /* Volver primero; las redes se agregan al grupo con cada escaneo. */
    lv_group_t *group = ui_panel_attach_group(ui_WifiScreen);
    ui_panel_add_button(group, ui_BackButton1, ui_SettingSelect1);
    lv_obj_add_event_cb(ui_BackButton1, ui_panel_back_key_cb, LV_EVENT_KEY, ui_BackButton1);
    lv_obj_add_event_cb(ui_WifiScreen, ui_panel_wifi_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);

    s_wifi_list = lv_obj_create(ui_WifiScreen);
    lv_obj_set_size(s_wifi_list, UI_PANEL_WIFI_LIST_W_PX, UI_PANEL_WIFI_LIST_H_PX);
    lv_obj_align(s_wifi_list, LV_ALIGN_CENTER, 0, UI_PANEL_WIFI_LIST_Y_PX);
    lv_obj_set_flex_flow(s_wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_wifi_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_wifi_list, UI_PANEL_WIFI_ITEM_GAP_PX, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_wifi_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_wifi_list, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_wifi_list, LV_SCROLLBAR_MODE_OFF);

    (void)lv_timer_create(ui_panel_wifi_report_cb, UI_WIFI_REPORT_CHECK_MS, NULL);
}

/* ---- Perfil ---- */

static void ui_panel_setup_profile(void)
{
    /* Campo de ID arriba (arranca con el foco), Volver abajo. Los dígitos van al campo. */
    lv_textarea_set_accepted_chars(ui_TextArea1, UI_PROFILE_ID_CHARS);
    lv_group_t *group = ui_panel_attach_group(ui_ScreenPerfil);
    lv_obj_add_event_cb(ui_TextArea1, ui_panel_hide_selector_cb, LV_EVENT_FOCUSED, ui_SettingSelect5);
    lv_obj_add_event_cb(ui_TextArea1, ui_panel_hide_selector_cb, LV_EVENT_DEFOCUSED, ui_SettingSelect5);
    lv_group_add_obj(group, ui_TextArea1);
    ui_panel_add_button(group, ui_BackButtonID, ui_SettingSelect5);
    lv_group_focus_obj(ui_TextArea1);
    lv_obj_t *const profile_objs[] = { ui_TextArea1, ui_BackButtonID };
    ui_panel_set_back_key(profile_objs, sizeof(profile_objs) / sizeof(profile_objs[0]), ui_BackButtonID);
}

/* ---- Sesión: Begin, Marcha, Ending, Falla ---- */

static void ui_panel_save_cb(lv_event_t *e)
{
    (void)e;
    if (s_measuring) {
        return;  /* Ya se está midiendo este paso: una sola confirmación */
    }
    s_measuring = true;
    s_measuring_mode = s_mode;
    s_measuring_prompt = s_shown_prompt;
    ui_panel_set_text(ui_LblInstruccion, s_measuring_text);
    ui_panel_emit(UI_EV_CONFIRM);  /* El firmware guarda el extremo y pasa al paso siguiente */
}

/** D en ScreenBegin cancela la sesión. */
static void ui_panel_begin_key_cb(lv_event_t *e)
{
    if ((lv_event_get_key(e) == LV_KEY_ESC) && ui_panel_is_new_press()) {
        ui_panel_emit(UI_EV_CANCEL);
    }
}

static void ui_panel_sent_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_panel_set_text(ui_LblEnvio, s_sent_text);
    s_send_timer = NULL;  /* LVGL borra el timer al terminar su única repetición */
}

/** Terminar: fin de sesión. La UI pasa sola a ScreenEnding; el firmware vuelve a IDLE. */
static void ui_panel_finish_cb(lv_event_t *e)
{
    (void)e;
    ui_panel_set_text(ui_LblEnvio, s_sending_text);
    if (s_send_timer != NULL) {
        lv_timer_del(s_send_timer);
    }
    s_send_timer = lv_timer_create(ui_panel_sent_cb, UI_SEND_MSG_MS, NULL);
    lv_timer_set_repeat_count(s_send_timer, UI_PANEL_ONE_SHOT);

    lv_scr_load_anim(ui_ScreenEnding, LV_SCR_LOAD_ANIM_FADE_ON, UI_PANEL_ENDING_ANIM_MS, 0u, false);
    ui_panel_emit(UI_EV_CANCEL);
}

static void ui_panel_retry_cb(lv_event_t *e)
{
    (void)e;
    ui_panel_emit(UI_EV_CONFIRM);  /* Rearme: el firmware vuelve a IDLE y la UI a Main */
}

static void ui_panel_setup_session(void)
{
    lv_group_t *begin_group = ui_panel_attach_group(ui_ScreenBegin);
    ui_panel_add_button(begin_group, ui_SaveButton, ui_SettingSelect2);
    lv_obj_add_event_cb(ui_SaveButton, ui_panel_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_SaveButton, ui_panel_begin_key_cb, LV_EVENT_KEY, NULL);

    lv_group_t *run_group = ui_panel_attach_group(ui_ScreenMarcha);
    ui_panel_add_button(run_group, ui_RetryButton2, ui_SettingSelect4);  /* Botón "Terminar" */
    lv_obj_add_event_cb(ui_RetryButton2, ui_panel_finish_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_RetryButton2, ui_panel_back_key_cb, LV_EVENT_KEY, ui_RetryButton2);
    lv_bar_set_range(ui_BarRango, 0, UI_PANEL_PERCENT_MAX);  /* La barra muestra la potencia */

    lv_group_t *ending_group = ui_panel_attach_group(ui_ScreenEnding);
    ui_panel_add_button(ending_group, ui_BackButton2, ui_SettingSelect6);
    lv_obj_add_event_cb(ui_BackButton2, ui_panel_back_key_cb, LV_EVENT_KEY, ui_BackButton2);

    lv_group_t *fault_group = ui_panel_attach_group(ui_ScreenFalla);
    ui_panel_add_button(fault_group, ui_RetryButton, ui_SettingSelect3);
    lv_obj_add_event_cb(ui_RetryButton, ui_panel_retry_cb, LV_EVENT_CLICKED, NULL);
}

/* ---- Contenido de cada pantalla ---- */

/** Texto del botón "Guardar" según el paso de calibración. */
static const char *ui_panel_save_label(ui_mode_t mode, ui_prompt_t prompt)
{
    if (mode == UI_MODE_CALIB_ROM) {
        return (prompt == UI_PROMPT_GO_EXT) ? "Guardar extensión" : "Guardar flexión";
    }
    switch (prompt) {
    case UI_PROMPT_CONTRACT_BICEPS:  return "Guardar bíceps";
    case UI_PROMPT_CONTRACT_TRICEPS: return "Guardar tríceps";
    case UI_PROMPT_RELAX:
    default:                         return "Guardar reposo";
    }
}

/**
 * ScreenBegin: mientras se mide el paso recordado, deja "Midiendo..."; cuando el estado avanza
 * (cambia modo o prompt) muestra la instrucción y el texto del botón del paso nuevo.
 */
static void ui_panel_update_calib(const ui_status_t *status, const char *const prompt_table[])
{
    if (s_measuring && (status->mode == s_measuring_mode) && (status->prompt == s_measuring_prompt)) {
        return;
    }
    s_measuring = false;
    s_shown_prompt = status->prompt;
    ui_panel_set_text(ui_LblInstruccion, ui_panel_prompt_text(prompt_table, status->prompt));
    ui_panel_set_text(ui_RecalText2, ui_panel_save_label(status->mode, status->prompt));
}

static void ui_panel_update_run(const ui_status_t *status)
{
    char text[UI_PANEL_TEXT_LEN];
    (void)snprintf(text, sizeof(text), "%ld grados", (long)ui_panel_round(status->angle_deg));
    ui_panel_set_text(ui_Lblangulo, text);

    (void)snprintf(text, sizeof(text), "%lu reps", (unsigned long)status->rep_count);
    ui_panel_set_text(ui_LblReps, text);
    (void)snprintf(text, sizeof(text), "ROM %ld°", (long)ui_panel_round(status->last_rom_deg));
    ui_panel_set_text(ui_LblRom, text);

    /* El texto y la barra usan el mismo porcentaje entero. */
    int32_t percent = ui_panel_round(status->duty * UI_PANEL_PERCENT_FULL);
    percent = (percent < 0) ? 0 : ((percent > UI_PANEL_PERCENT_MAX) ? UI_PANEL_PERCENT_MAX : percent);
    (void)snprintf(text, sizeof(text), "Potencia %ld%%", (long)percent);
    ui_panel_set_text(ui_LblEstado, text);
    lv_bar_set_value(ui_BarRango, percent, LV_ANIM_OFF);
}

static void ui_panel_update_fault(const ui_status_t *status)
{
    const char *text = s_fault_unknown_text;
    for (size_t i = 0u; i < UI_PANEL_FAULT_COUNT; i++) {
        if ((status->faults & s_fault_text[i].bit) != 0u) {
            text = s_fault_text[i].text;
            break;
        }
    }
    ui_panel_set_text(ui_LblFalla, text);
}

/** true en las pantallas que la UI recorre sola mientras el firmware está en IDLE. */
static bool ui_panel_is_idle_screen(const lv_obj_t *screen)
{
    return (screen == ui_Main) || (screen == ui_SettingScreen) || (screen == ui_WifiScreen) ||
           (screen == ui_ScreenPerfil) || (screen == ui_ScreenEnding) || (screen == ui_ScreenBrazo);
}

/** Pantalla de cada modo. En IDLE se queda donde está si es una pantalla del menú. */
static lv_obj_t *ui_panel_mode_screen(ui_mode_t mode)
{
    switch (mode) {
    case UI_MODE_CALIB_ROM:
    case UI_MODE_CALIB_EMG: return ui_ScreenBegin;
    case UI_MODE_RUN:       return ui_ScreenMarcha;
    case UI_MODE_FAULT:     return ui_ScreenFalla;
    case UI_MODE_IDLE: {
        lv_obj_t *current = ui_panel_current_screen();
        return ui_panel_is_idle_screen(current) ? current : ui_Main;
    }
    case UI_MODE_BOOT:
    default:                return ui_Inicio;
    }
}

/* ---- API ---- */

void ui_panel_init(void)
{
    ui_init();  /* Export de SquareLine: crea todas las pantallas y carga Inicio */
    lv_port_indev_init();
    lv_port_indev_set_key_cb(ui_panel_key_cb);

    /* Inicio no tiene botones: la pantalla misma va al grupo. */
    lv_group_add_obj(ui_panel_attach_group(ui_Inicio), ui_Inicio);
    ui_panel_setup_menus();
    ui_panel_setup_arm();
    ui_panel_setup_wifi();
    ui_panel_setup_profile();
    ui_panel_setup_session();

    /* Inicio ya se cargó en ui_init(), antes de tener grupo. */
    lv_port_indev_set_group((lv_group_t *)lv_obj_get_user_data(lv_scr_act()));
    s_last_press = lv_port_indev_get_press_count();
    s_boot_shown_ms = lv_tick_get(); /* Inicio queda a la vista desde acá (minimo UI_PANEL_BOOT_MIN_MS) */
}

void ui_panel_set_event_cb(ui_panel_event_cb_t cb)
{
    s_event_cb = cb;
}

void ui_panel_show_status(const ui_status_t *status)
{
    /* Inicio (BOOT) se muestra al menos UI_PANEL_BOOT_MIN_MS antes de pasar a Main, aunque el
     * firmware deje BOOT enseguida (el core 0 manda un CONFIRM inicial para autoarrancar). */
    if ((s_mode == UI_MODE_BOOT) && (status->mode != UI_MODE_BOOT) &&
        (lv_tick_elaps(s_boot_shown_ms) < UI_PANEL_BOOT_MIN_MS)) {
        return;
    }

    if (!s_has_mode || (status->mode != s_mode)) {
        lv_obj_t *screen = ui_panel_mode_screen(status->mode);
        if (screen != ui_panel_current_screen()) {
            /* El export arma un pulso de opacidad (PInPout, repeticion infinita) sobre el
             * selector en cada SCREEN_LOADED. Como las pantallas se reutilizan (sesiones
             * sucesivas), al recargar una se apilaba un segundo pulso sobre el mismo selector
             * y el recuadro blanco parpadeaba rapido. Se borran los pulsos anteriores antes de
             * cargar, asi queda uno solo por pantalla y solo se re-arma al cambiar de modo. */
            (void)lv_anim_custom_del(NULL, _ui_anim_callback_set_opacity);
            /* Cambio de pantalla por modo con un FADE suave (antes era de golpe). */
            lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_ON, UI_PANEL_MODE_ANIM_MS, 0, false);
        }
        s_mode = status->mode;
        s_has_mode = true;
        s_measuring = false;  /* Un cambio de modo cierra cualquier "Midiendo..." pendiente */
    }

    switch (status->mode) {
    case UI_MODE_CALIB_ROM: ui_panel_update_calib(status, s_rom_prompt_text); break;
    case UI_MODE_CALIB_EMG: ui_panel_update_calib(status, s_emg_prompt_text); break;
    case UI_MODE_RUN:   ui_panel_update_run(status);   break;
    case UI_MODE_FAULT: ui_panel_update_fault(status); break;
    case UI_MODE_BOOT:
    case UI_MODE_IDLE:
    default:            break;  /* Inicio y el menú no muestran datos del firmware */
    }
}
