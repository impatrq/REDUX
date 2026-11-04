/**
 * @file hal_keypad.c
 * @brief Teclado matricial 4x4: barrido, antirrebote y eventos, sin bloquear.
 */
#include "hal/hal_keypad.h"

#include <stddef.h>

#include "hardware/gpio.h"
#include "pico/time.h"

#include "hal/board.h"
#include "ui/ui_config.h"

#define HAL_KEYPAD_UNKNOWN_CHAR  '?'

static const uint s_row_pins[BOARD_KEYPAD_ROWS] = {
    BOARD_PIN_KEY_ROW1, BOARD_PIN_KEY_ROW2, BOARD_PIN_KEY_ROW3, BOARD_PIN_KEY_ROW4,
};

static const uint s_col_pins[BOARD_KEYPAD_COLS] = {
    BOARD_PIN_KEY_COL1, BOARD_PIN_KEY_COL2, BOARD_PIN_KEY_COL3, BOARD_PIN_KEY_COL4,
};

/* Teclas impresas, fila 1 (GP16) arriba y columna 1 (GP20) a la izquierda.
 * Si el módulo está cableado al revés, se corrige acá, no en los pines. */
static const char s_key_chars[HAL_KEYPAD_KEY_COUNT] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D',
};

static hal_keypad_logic_t s_logic;
static bool s_initialized = false;

/** Lee la matriz completa: bit (fila × columnas + columna) en 1 = se lee presionada. */
static uint16_t hal_keypad_read_raw(void)
{
    uint16_t raw = 0u;
    for (uint32_t row = 0u; row < BOARD_KEYPAD_ROWS; row++) {
        gpio_set_dir(s_row_pins[row], GPIO_OUT); /* Valor de salida ya en 0 */
        busy_wait_us_32(UI_KEYPAD_SETTLE_US);
        for (uint32_t col = 0u; col < BOARD_KEYPAD_COLS; col++) {
            if (!gpio_get(s_col_pins[col])) {
                raw |= (uint16_t)(1u << (row * BOARD_KEYPAD_COLS + col));
            }
        }
        gpio_set_dir(s_row_pins[row], GPIO_IN);
    }
    return raw;
}

void hal_keypad_init(void)
{
    if (s_initialized) {
        return;
    }
    for (uint32_t row = 0u; row < BOARD_KEYPAD_ROWS; row++) {
        gpio_init(s_row_pins[row]);
        gpio_disable_pulls(s_row_pins[row]);
        gpio_put(s_row_pins[row], false);
        gpio_set_dir(s_row_pins[row], GPIO_IN);
    }
    for (uint32_t col = 0u; col < BOARD_KEYPAD_COLS; col++) {
        gpio_init(s_col_pins[col]);
        gpio_set_dir(s_col_pins[col], GPIO_IN);
        gpio_pull_up(s_col_pins[col]);
    }
    hal_keypad_logic_init(&s_logic);
    s_initialized = true;
}

void hal_keypad_scan(void)
{
    if (!s_initialized) {
        return;
    }
    hal_keypad_logic_feed(&s_logic, hal_keypad_read_raw());
}

bool hal_keypad_get_event(hal_keypad_event_t *ev)
{
    return hal_keypad_logic_pop(&s_logic, ev);
}

uint16_t hal_keypad_get_pressed(void)
{
    return s_logic.stable;
}

char hal_keypad_key_char(uint8_t key)
{
    return (key < HAL_KEYPAD_KEY_COUNT) ? s_key_chars[key] : HAL_KEYPAD_UNKNOWN_CHAR;
}

void hal_keypad_get_stats(hal_keypad_stats_t *stats)
{
    stats->scans = s_logic.scans;
    stats->ghost_scans = s_logic.ghost_scans;
    stats->dropped_events = s_logic.dropped_events;
}
