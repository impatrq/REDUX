// ui.h · Contrato mínimo de la interfaz de usuario. Las capas que piden
// confirmaciones (p. ej. la calibración del rango) dependen solo de este header y
// de los eventos ui_event_t (definidos en redux_types.h), nunca del pulsador ni
// del monitor serie. Hoy lo implementa ui_basic (pulsador + LED + USB); más adelante
// la caja con teclado y pantalla ST7735 implementa el mismo contrato sin tocar la
// calibración ni el supervisor.
#ifndef REDUX_UI_H
#define REDUX_UI_H

#include <stdbool.h>

#include "redux_types.h" // ui_event_t

// Inicializa la implementación de la interfaz (pines, estado interno).
void ui_init(void);

// Trabajo periódico de la interfaz: pensado para llamarse una vez por tick de 1 ms
// (antirrebote del pulsador, parpadeo del LED mientras se espera una confirmación).
void ui_tick(void);

// Devuelve el próximo evento pendiente del usuario, o UI_EV_NONE si no hay.
ui_event_t ui_poll(void);

// Muestra un mensaje de estado al usuario (en ui_basic, una línea por USB).
void ui_message(const char *msg);

// Indica si la interfaz está esperando una confirmación. En ui_basic esto enciende
// el parpadeo lento del LED; al dejar de esperar, el LED se apaga.
void ui_set_waiting(bool waiting);

#endif // REDUX_UI_H
