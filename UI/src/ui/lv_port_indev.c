/**
 * @file lv_port_indev.c
 * @brief Puente LVGL <-> teclado 4x4: dispositivo de entrada tipo KEYPAD.
 */
#include "ui/lv_port_indev.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal/hal_keypad.h"
#include "ui/ui_config.h"

#define LV_PORT_INDEV_NO_KEY  0u  /* Ninguna tecla de LVGL activa (los LV_KEY_* son > 0) */

static lv_indev_drv_t s_drv;
static lv_indev_t *s_indev = NULL;
static uint32_t s_active_key = LV_PORT_INDEV_NO_KEY;  /* Tecla de LVGL apretada ahora */
static uint32_t s_press_count = 0u;
static lv_port_indev_key_cb_t s_key_cb = NULL;  /* Teclas que no van a LVGL */
static lv_port_indev_stats_t s_stats = {0};

/** Tecla de LVGL para una tecla del teclado; LV_PORT_INDEV_NO_KEY si no tiene uso. */
static uint32_t lv_port_indev_map_key(uint8_t key)
{
    const char c = hal_keypad_key_char(key);
    if ((c >= '0') && (c <= '9')) {
        return (uint32_t)c;  /* Los dígitos no chocan con los LV_KEY_* */
    }
    switch (c) {
    case UI_KEY_UP_CHAR:    return LV_KEY_PREV;
    case UI_KEY_ENTER_CHAR: return LV_KEY_ENTER;
    case UI_KEY_DOWN_CHAR:  return LV_KEY_NEXT;
    case UI_KEY_BACK_CHAR:  return LV_KEY_ESC;
    case UI_KEY_ERASE_CHAR: return LV_KEY_BACKSPACE;
    default:                return LV_PORT_INDEV_NO_KEY;
    }
}

static void lv_port_indev_scan_cb(lv_timer_t *timer)
{
    (void)timer;
    hal_keypad_scan();
}

/**
 * Entrega a LVGL el próximo evento útil de la cola. Si no hay, repite el
 * estado actual. Pide otra lectura mientras queden eventos.
 */
static void lv_port_indev_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    hal_keypad_event_t ev;
    bool delivered = false;

    while (!delivered && hal_keypad_get_event(&ev)) {
        if (hal_keypad_key_char(ev.key) == UI_KEY_ROTATE_CHAR) {
            /* Al apretar, si no hay otra tecla activa; el soltar no hace nada. */
            if ((ev.type == HAL_KEYPAD_EV_PRESS) && (s_active_key == LV_PORT_INDEV_NO_KEY) &&
                (s_key_cb != NULL)) {
                s_key_cb(UI_KEY_ROTATE_CHAR);
            }
            continue;
        }
        const uint32_t lv_key = lv_port_indev_map_key(ev.key);
        if (ev.type == HAL_KEYPAD_EV_PRESS) {
            delivered = (lv_key != LV_PORT_INDEV_NO_KEY) && (s_active_key == LV_PORT_INDEV_NO_KEY);
            if (delivered) {
                s_active_key = lv_key;
                s_press_count++;
            }
        } else {
            delivered = (lv_key != LV_PORT_INDEV_NO_KEY) && (lv_key == s_active_key);
            if (delivered) {
                data->key = s_active_key;  /* Se suelta la misma tecla que se apretó */
                s_active_key = LV_PORT_INDEV_NO_KEY;
                data->state = LV_INDEV_STATE_RELEASED;
            }
        }
        if (delivered) {
            s_stats.delivered_events++;
        } else {
            s_stats.ignored_events++;
        }
    }

    if (s_active_key != LV_PORT_INDEV_NO_KEY) {
        data->key = s_active_key;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (!delivered) {
        data->state = LV_INDEV_STATE_RELEASED;  /* data->key queda con la última tecla */
    }
    data->continue_reading = delivered;
}

void lv_port_indev_init(void)
{
    if (s_indev != NULL) {
        return;
    }
    hal_keypad_init();
    (void)lv_timer_create(lv_port_indev_scan_cb, UI_KEYPAD_SCAN_PERIOD_MS, NULL);

    lv_indev_drv_init(&s_drv);
    s_drv.type = LV_INDEV_TYPE_KEYPAD;
    s_drv.read_cb = lv_port_indev_read_cb;
    s_indev = lv_indev_drv_register(&s_drv);
    lv_timer_set_period(s_drv.read_timer, UI_KEYPAD_SCAN_PERIOD_MS);
}

void lv_port_indev_set_group(lv_group_t *group)
{
    if (s_indev != NULL) {
        lv_indev_set_group(s_indev, group);
    }
}

void lv_port_indev_set_key_cb(lv_port_indev_key_cb_t cb)
{
    s_key_cb = cb;
}

uint32_t lv_port_indev_get_press_count(void)
{
    return s_press_count;
}

void lv_port_indev_get_stats(lv_port_indev_stats_t *stats)
{
    *stats = s_stats;
}
