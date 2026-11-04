/**
 * @file ui_panel.h
 * @brief Pantallas de SquareLine conectadas al flujo de REDUX (pantalla + teclado).
 *
 * Solo se usa desde el core 1 (el de LVGL).
 *
 * Elige la pantalla según el modo del mensaje de estado (app/ui.h) y llena sus
 * textos. Arma un grupo de teclado por pantalla y lo conecta al cargarla.
 * Traduce teclas y botones a eventos ui_event_t, que entrega por un callback
 * (desde U6, el que los encola hacia el firmware). Ver docs/ui_contract.md §12.
 */
#ifndef UI_PANEL_H
#define UI_PANEL_H

#include "app/ui.h"

/** Recibe cada evento de la UI. Corre en el core 1, dentro de LVGL: no bloquear. */
typedef void (*ui_panel_event_cb_t)(ui_event_t event);

/** Crea las pantallas del export, los grupos y el teclado. Después de lv_port_disp_init(). */
void ui_panel_init(void);

/** Registra quién recibe los eventos (NULL = nadie). */
void ui_panel_set_event_cb(ui_panel_event_cb_t cb);

/** Muestra un mensaje de estado: cambia de pantalla si cambió el modo y actualiza los textos. */
void ui_panel_show_status(const ui_status_t *status);

#endif /* UI_PANEL_H */
