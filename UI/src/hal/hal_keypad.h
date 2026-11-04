/**
 * @file hal_keypad.h
 * @brief Teclado matricial 4x4: barrido, antirrebote y eventos, sin bloquear.
 *
 * Filas: en reposo son entradas (alta impedancia); para leer una fila se la
 * pone como salida en bajo. Columnas: entradas con pull-up (bajo = presionada).
 * Así, dos teclas de la misma columna nunca enfrentan dos salidas.
 *
 * La lógica (antirrebote, teclas fantasma, cola) está en hal_keypad_logic.c.
 *
 * No es seguro entre cores: lo usa un solo core (el de la UI).
 */
#ifndef HAL_KEYPAD_H
#define HAL_KEYPAD_H

#include <stdbool.h>
#include <stdint.h>

#include "hal/hal_keypad_logic.h"

typedef struct {
    uint32_t scans;           /* Barridos hechos */
    uint32_t ghost_scans;     /* Barridos descartados por posible tecla fantasma */
    uint32_t dropped_events;  /* Eventos perdidos con la cola llena */
} hal_keypad_stats_t;

/** Configura filas y columnas. Idempotente. */
void hal_keypad_init(void);

/**
 * Barre la matriz una vez (~4 × UI_KEYPAD_SETTLE_US) y actualiza el estado.
 * Llamar cada UI_KEYPAD_SCAN_PERIOD_MS.
 */
void hal_keypad_scan(void);

/** Saca el evento presionar/soltar más viejo. false si no hay. */
bool hal_keypad_get_event(hal_keypad_event_t *ev);

/** Máscara de teclas presionadas, ya sin rebote (bit = número de tecla). */
uint16_t hal_keypad_get_pressed(void);

/** Carácter impreso en la tecla ('1'..'9', '0', 'A'..'D', '*', '#'); '?' si no existe. */
char hal_keypad_key_char(uint8_t key);

void hal_keypad_get_stats(hal_keypad_stats_t *stats);

#endif /* HAL_KEYPAD_H */
