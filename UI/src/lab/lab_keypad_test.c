/**
 * @file lab_keypad_test.c
 * @brief Prueba de U3 (solo del laboratorio). Todo corre en el core 1.
 */
#include "lab/lab_keypad_test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "hal/hal_keypad.h"
#include "ui/ui_config.h"

#define LAB_KEYPAD_STATS_PERIOD_MS  10000u
#define LAB_KEY_OFFSET_Y_PX         (-8)   /* Tecla un poco arriba del centro */
#define LAB_COUNT_OFFSET_Y_PX       16     /* Contador debajo de la tecla */
#define LAB_TITLE_COLOR             0xFFFFFFu
#define LAB_KEY_COLOR               0x00FF00u
#define LAB_BG_COLOR                0x1E074Bu /* El violeta de fondo del export */

static lv_obj_t *s_key_label = NULL;
static lv_obj_t *s_count_label = NULL;
static uint32_t s_press_count = 0u;
static uint32_t s_last_ghost_scans = 0u;
static bool s_in_ghost = false;

static void lab_keypad_scan_cb(lv_timer_t *timer)
{
    (void)timer;
    hal_keypad_scan();

    hal_keypad_event_t ev;
    while (hal_keypad_get_event(&ev)) {
        const char key_char = hal_keypad_key_char(ev.key);
        if (ev.type == HAL_KEYPAD_EV_PRESS) {
            s_press_count++;
            lv_label_set_text_fmt(s_key_label, "Tecla %c", key_char);
            lv_label_set_text_fmt(s_count_label, "%lu pulsaciones", (unsigned long)s_press_count);
        }
        printf("U3 | %c (tecla %2u) %s | pulsaciones %lu | presionadas 0x%04X\n",
               key_char, (unsigned)ev.key,
               (ev.type == HAL_KEYPAD_EV_PRESS) ? "presionada" : "soltada  ",
               (unsigned long)s_press_count, (unsigned)hal_keypad_get_pressed());
    }

    /* Un aviso por racha de barridos descartados, no uno cada 10 ms. */
    hal_keypad_stats_t stats;
    hal_keypad_get_stats(&stats);
    const bool ghost_now = (stats.ghost_scans != s_last_ghost_scans);
    if (ghost_now && !s_in_ghost) {
        printf("U3 | posible tecla fantasma (3 teclas en L): barridos descartados\n");
    }
    s_in_ghost = ghost_now;
    s_last_ghost_scans = stats.ghost_scans;
}

static void lab_keypad_stats_cb(lv_timer_t *timer)
{
    (void)timer;
    hal_keypad_stats_t stats;
    hal_keypad_get_stats(&stats);
    printf("U3 | barridos %lu | descartados por fantasma %lu | eventos perdidos %lu\n",
           (unsigned long)stats.scans, (unsigned long)stats.ghost_scans,
           (unsigned long)stats.dropped_events);
}

static void lab_build_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(LAB_BG_COLOR), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "U3 teclado");
    lv_obj_set_style_text_color(title, lv_color_hex(LAB_TITLE_COLOR), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    s_key_label = lv_label_create(screen);
    lv_label_set_text(s_key_label, "Tecla -");
    lv_obj_set_style_text_color(s_key_label, lv_color_hex(LAB_KEY_COLOR), LV_PART_MAIN);
    lv_obj_align(s_key_label, LV_ALIGN_CENTER, 0, LAB_KEY_OFFSET_Y_PX);

    s_count_label = lv_label_create(screen);
    lv_label_set_text(s_count_label, "0 pulsaciones");
    lv_obj_set_style_text_color(s_count_label, lv_color_hex(LAB_TITLE_COLOR), LV_PART_MAIN);
    lv_obj_align(s_count_label, LV_ALIGN_CENTER, 0, LAB_COUNT_OFFSET_Y_PX);

    lv_scr_load(screen);
}

void lab_keypad_test_setup(void)
{
    hal_keypad_init();
    lab_build_screen();
    (void)lv_timer_create(lab_keypad_scan_cb, UI_KEYPAD_SCAN_PERIOD_MS, NULL);
    (void)lv_timer_create(lab_keypad_stats_cb, LAB_KEYPAD_STATS_PERIOD_MS, NULL);
    printf("U3 | teclado: barrido cada %u ms, antirrebote %u barridos\n",
           (unsigned)UI_KEYPAD_SCAN_PERIOD_MS, (unsigned)UI_KEYPAD_DEBOUNCE_SCANS);
}
