/**
 * @file hal_keypad_logic.c
 * @brief Lógica del teclado 4x4 sin hardware: antirrebote, teclas fantasma y
 *        cola de eventos.
 */
#include "hal/hal_keypad_logic.h"

#include <stddef.h>

#define HAL_KEYPAD_ROW_MASK  ((1u << BOARD_KEYPAD_COLS) - 1u)

_Static_assert(HAL_KEYPAD_KEY_COUNT <= 16u, "La máscara cruda es de 16 bits");
_Static_assert(UI_KEYPAD_EVENT_QUEUE_LEN <= UINT8_MAX, "Índices de la cola en uint8_t");
_Static_assert(UI_KEYPAD_DEBOUNCE_SCANS >= 1u, "El antirrebote necesita al menos un barrido");

static uint16_t hal_keypad_logic_row_bits(uint16_t raw, uint32_t row)
{
    return (uint16_t)((raw >> (row * BOARD_KEYPAD_COLS)) & HAL_KEYPAD_ROW_MASK);
}

/** true si @p bits tiene dos o más bits en 1. */
static bool hal_keypad_logic_has_two_bits(uint16_t bits)
{
    return (bits & (uint16_t)(bits - 1u)) != 0u;
}

static void hal_keypad_logic_push(hal_keypad_logic_t *k, uint8_t key, hal_keypad_ev_type_t type)
{
    if (k->queue_count >= UI_KEYPAD_EVENT_QUEUE_LEN) {
        k->dropped_events++;
        return;
    }
    const uint32_t tail = ((uint32_t)k->queue_head + k->queue_count) % UI_KEYPAD_EVENT_QUEUE_LEN;
    k->queue[tail].key = key;
    k->queue[tail].type = type;
    k->queue_count++;
}

void hal_keypad_logic_init(hal_keypad_logic_t *k)
{
    k->stable = 0u;
    for (uint32_t i = 0u; i < HAL_KEYPAD_KEY_COUNT; i++) {
        k->counter[i] = 0u;
    }
    k->queue_head = 0u;
    k->queue_count = 0u;
    k->scans = 0u;
    k->ghost_scans = 0u;
    k->dropped_events = 0u;
}

bool hal_keypad_logic_is_ghost(uint16_t raw)
{
    for (uint32_t r1 = 0u; r1 < BOARD_KEYPAD_ROWS; r1++) {
        const uint16_t bits1 = hal_keypad_logic_row_bits(raw, r1);
        for (uint32_t r2 = r1 + 1u; r2 < BOARD_KEYPAD_ROWS; r2++) {
            const uint16_t shared = bits1 & hal_keypad_logic_row_bits(raw, r2);
            if (hal_keypad_logic_has_two_bits(shared)) {
                return true;
            }
        }
    }
    return false;
}

void hal_keypad_logic_feed(hal_keypad_logic_t *k, uint16_t raw)
{
    k->scans++;

    if (hal_keypad_logic_is_ghost(raw)) {
        /* Lectura ambigua: no se sabe qué teclas están de verdad. Se mantiene el
         * estado anterior y el antirrebote empieza de nuevo. */
        k->ghost_scans++;
        for (uint32_t i = 0u; i < HAL_KEYPAD_KEY_COUNT; i++) {
            k->counter[i] = 0u;
        }
        return;
    }

    for (uint32_t i = 0u; i < HAL_KEYPAD_KEY_COUNT; i++) {
        const uint16_t bit = (uint16_t)(1u << i);
        if ((raw & bit) == (k->stable & bit)) {
            k->counter[i] = 0u;
            continue;
        }
        k->counter[i]++;
        if (k->counter[i] >= UI_KEYPAD_DEBOUNCE_SCANS) {
            k->counter[i] = 0u;
            k->stable ^= bit;
            hal_keypad_logic_push(k, (uint8_t)i,
                                  ((k->stable & bit) != 0u) ? HAL_KEYPAD_EV_PRESS
                                                            : HAL_KEYPAD_EV_RELEASE);
        }
    }
}

bool hal_keypad_logic_pop(hal_keypad_logic_t *k, hal_keypad_event_t *ev)
{
    if (k->queue_count == 0u) {
        return false;
    }
    *ev = k->queue[k->queue_head];
    k->queue_head = (uint8_t)((k->queue_head + 1u) % UI_KEYPAD_EVENT_QUEUE_LEN);
    k->queue_count--;
    return true;
}
