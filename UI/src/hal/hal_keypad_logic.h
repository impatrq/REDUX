/**
 * @file hal_keypad_logic.h
 * @brief Lógica del teclado 4x4 sin hardware: antirrebote, teclas fantasma y
 *        cola de eventos.
 *
 * No incluye nada del SDK: se compila igual en la Pico y en la PC
 * (tests/test_keypad.c). hal_keypad.c lee la matriz y le pasa cada barrido.
 *
 * Una lectura cruda es una máscara de 16 bits: el bit (fila × columnas + columna)
 * vale 1 si esa tecla se lee presionada.
 */
#ifndef HAL_KEYPAD_LOGIC_H
#define HAL_KEYPAD_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "hal/board.h"
#include "ui/ui_config.h"

#define HAL_KEYPAD_KEY_COUNT  (BOARD_KEYPAD_ROWS * BOARD_KEYPAD_COLS)

typedef enum {
    HAL_KEYPAD_EV_PRESS = 0,
    HAL_KEYPAD_EV_RELEASE,
} hal_keypad_ev_type_t;

typedef struct {
    uint8_t key;                /* 0..HAL_KEYPAD_KEY_COUNT-1 = fila × columnas + columna */
    hal_keypad_ev_type_t type;
} hal_keypad_event_t;

typedef struct {
    uint16_t stable;                                  /* Estado aceptado (tras el antirrebote) */
    uint8_t counter[HAL_KEYPAD_KEY_COUNT];            /* Barridos seguidos distintos de stable */
    hal_keypad_event_t queue[UI_KEYPAD_EVENT_QUEUE_LEN];
    uint8_t queue_head;                               /* Próximo evento a leer */
    uint8_t queue_count;
    uint32_t scans;                                   /* Barridos recibidos */
    uint32_t ghost_scans;                             /* Barridos descartados por tecla fantasma */
    uint32_t dropped_events;                          /* Eventos perdidos con la cola llena */
} hal_keypad_logic_t;

/** Deja todo en reposo: ninguna tecla presionada, cola vacía, contadores en 0. */
void hal_keypad_logic_init(hal_keypad_logic_t *k);

/**
 * true si la lectura cruda puede tener una tecla fantasma: dos filas con dos o
 * más columnas en común (tres teclas en L se leen igual que cuatro en rectángulo).
 */
bool hal_keypad_logic_is_ghost(uint16_t raw);

/**
 * Procesa un barrido. Una tecla cambia de estado cuando la lectura distinta se
 * repite UI_KEYPAD_DEBOUNCE_SCANS barridos seguidos; ahí se encola un evento.
 * Un barrido con posible fantasma se descarta entero.
 */
void hal_keypad_logic_feed(hal_keypad_logic_t *k, uint16_t raw);

/** Saca el evento más viejo. false si la cola está vacía. */
bool hal_keypad_logic_pop(hal_keypad_logic_t *k, hal_keypad_event_t *ev);

#endif /* HAL_KEYPAD_LOGIC_H */
