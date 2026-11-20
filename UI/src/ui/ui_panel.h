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

/** Recibe cada evento de la UI (con su payload). Corre en el core 1, dentro de LVGL: no bloquear. */
typedef void (*ui_panel_event_cb_t)(const ui_event_msg_t *msg);

/** Crea las pantallas del export, los grupos y el teclado. Después de lv_port_disp_init(). */
void ui_panel_init(void);

/** Registra quién recibe los eventos (NULL = nadie). */
void ui_panel_set_event_cb(ui_panel_event_cb_t cb);

/** Muestra un mensaje de estado: cambia de pantalla si cambió el modo y actualiza los textos. */
void ui_panel_show_status(const ui_status_t *status);

/** Estado REAL del envío del resultado a la API (Paso C), para la pantalla de fin. */
typedef enum {
    UI_PANEL_TX_IDLE = 0,  /**< Sin envío (no se muestra nada nuevo) */
    UI_PANEL_TX_SENDING,   /**< "Enviando..." */
    UI_PANEL_TX_RETRYING,  /**< "Sin conexión, reintentando..." */
    UI_PANEL_TX_SENT,      /**< "Enviado" */
    UI_PANEL_TX_ERROR      /**< "Error: <motivo>" */
} ui_panel_tx_t;

/**
 * Fija el estado del envío que muestra la pantalla de fin (ScreenEnding). Lo llama el
 * firmware (core 1) con el estado real del cliente HTTP; `motivo` solo se usa con
 * UI_PANEL_TX_ERROR. En el laboratorio (sin firmware) no se llama: queda el texto inicial.
 */
void ui_panel_set_tx_status(ui_panel_tx_t state, const char *motivo);

#endif /* UI_PANEL_H */
