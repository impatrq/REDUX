// fw_ui.h · Driver de interfaz del lado del core 0 (control). Es el enganche que usa el
// ciclo de control para pedir confirmaciones y mostrar mensajes, independiente de la
// implementación concreta. Hay dos implementaciones, elegidas al compilar (REDUX_UI):
//   - ui_basic (BASIC): pulsador GP3 + LED GP2 + monitor serie por USB.
//   - ui_panel_fw (PANEL): puente al panel LVGL del core 1 a través del contrato app/ui.h.
//
// El contrato cruzado de más alto nivel (ui_status_t, ui_app_*, modos y prompts) vive en
// app/ui.h; este header solo declara el enganche mínimo que app.c llama cada tick.
#ifndef REDUX_FW_UI_H
#define REDUX_FW_UI_H

#include <stdbool.h>

#include "app/ui.h" // ui_event_t

// Inicializa la implementación de la interfaz (pines/colas, arranque del core 1 en PANEL).
// Nombre con prefijo fw_ui_ (no ui_init) para no chocar con el ui_init() del export de
// SquareLine, que en PANEL se enlaza en el mismo binario (lo llama ui_panel_init en el core 1).
void fw_ui_init(void);

// Trabajo periódico de la interfaz: pensado para llamarse una vez por tick de 1 ms
// (antirrebote del pulsador en BASIC; sin trabajo en PANEL, que corre en el core 1).
void ui_tick(void);

// Devuelve el próximo evento pendiente del usuario, o UI_EV_NONE si no hay.
ui_event_t ui_poll(void);

// Muestra un mensaje de estado al usuario (una línea por USB en ambas implementaciones).
void ui_message(const char *msg);

// Indica si la interfaz está esperando una confirmación (en BASIC, parpadeo del LED).
void ui_set_waiting(bool waiting);

// Traduce una línea del monitor serie a un evento de la interfaz (sirve en las dos
// implementaciones, así los comandos por serie siguen funcionando igual):
//   "ok" -> UI_EV_CONFIRM   "cancel" -> UI_EV_CANCEL   "recal" -> UI_EV_RECALIBRATE
// Cualquier otra línea se ignora.
void ui_feed_line(const char *line);

// Solo PANEL (ui_panel_fw): true si el core 1 ya arrancó y hace más de
// UI_HEARTBEAT_TIMEOUT_MS que no late (core 1 colgado) => el core 0 fuerza FAULT.
bool ui_app_heartbeat_lost(void);

#endif // REDUX_FW_UI_H
