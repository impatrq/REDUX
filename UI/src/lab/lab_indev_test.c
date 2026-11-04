/**
 * @file lab_indev_test.c
 * @brief Prueba de U4 (solo del laboratorio). Todo corre en el core 1.
 */
#include "lab/lab_indev_test.h"

#include <stdint.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "hal/hal_keypad.h"
#include "ui/lv_port_indev.h"
#include "ui/ui_config.h"

#define LAB_INDEV_STATS_PERIOD_MS  10000u
#define LAB_BUTTON_COUNT           2u
#define LAB_BUTTON_WIDTH_PX        96
#define LAB_BUTTON_HEIGHT_PX       28
#define LAB_BUTTON1_OFFSET_Y_PX    (-20)  /* Botón 1 arriba del centro */
#define LAB_BUTTON2_OFFSET_Y_PX    16     /* Botón 2 abajo del centro */
#define LAB_TITLE_COLOR            0xFFFFFFu
#define LAB_ACTION_COLOR           0x00FF00u
#define LAB_BG_COLOR               0x1E074Bu /* El violeta de fondo del export */

static lv_obj_t *s_action_label = NULL;
static uint32_t s_click_count[LAB_BUTTON_COUNT] = {0u};

/** user_data del botón = su número (1 o 2). */
static uint32_t lab_button_number(lv_event_t *e)
{
    return (uint32_t)(uintptr_t)lv_event_get_user_data(e);
}

static void lab_button_event_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const uint32_t number = lab_button_number(e);

    if (code == LV_EVENT_CLICKED) {
        const uint32_t count = ++s_click_count[number - 1u];
        lv_label_set_text_fmt(s_action_label, "Boton %lu (%lu)", (unsigned long)number,
                              (unsigned long)count);
        printf("U4 | %c: boton %lu presionado (%lu veces)\n", UI_KEY_ENTER_CHAR,
               (unsigned long)number, (unsigned long)count);
    } else if (code == LV_EVENT_FOCUSED) {
        printf("U4 | foco en boton %lu\n", (unsigned long)number);
    } else if ((code == LV_EVENT_KEY) && (lv_event_get_key(e) == LV_KEY_ESC)) {
        lv_label_set_text(s_action_label, "Volver (D)");
        printf("U4 | %c: volver (desde boton %lu)\n", UI_KEY_BACK_CHAR, (unsigned long)number);
    } else {
        /* Los demás eventos no se usan en la prueba. */
    }
}

static void lab_stats_cb(lv_timer_t *timer)
{
    (void)timer;
    hal_keypad_stats_t keypad;
    hal_keypad_get_stats(&keypad);
    lv_port_indev_stats_t indev;
    lv_port_indev_get_stats(&indev);
    printf("U4 | barridos %lu | fantasmas %lu | perdidos %lu | a LVGL %lu | ignorados %lu\n",
           (unsigned long)keypad.scans, (unsigned long)keypad.ghost_scans,
           (unsigned long)keypad.dropped_events, (unsigned long)indev.delivered_events,
           (unsigned long)indev.ignored_events);
}

static lv_obj_t *lab_create_button(lv_obj_t *screen, lv_group_t *group, uint32_t number,
                                   lv_coord_t offset_y)
{
    lv_obj_t *button = lv_btn_create(screen);
    lv_obj_set_size(button, LAB_BUTTON_WIDTH_PX, LAB_BUTTON_HEIGHT_PX);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, offset_y);
    lv_obj_add_event_cb(button, lab_button_event_cb, LV_EVENT_ALL, (void *)(uintptr_t)number);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text_fmt(label, "Boton %lu", (unsigned long)number);
    lv_obj_center(label);

    lv_group_add_obj(group, button);
    return button;
}

static void lab_build_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(LAB_BG_COLOR), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "U4 teclado");
    lv_obj_set_style_text_color(title, lv_color_hex(LAB_TITLE_COLOR), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_group_t *group = lv_group_create();
    (void)lab_create_button(screen, group, 1u, LAB_BUTTON1_OFFSET_Y_PX);
    (void)lab_create_button(screen, group, 2u, LAB_BUTTON2_OFFSET_Y_PX);

    s_action_label = lv_label_create(screen);
    lv_label_set_text(s_action_label, "-");
    lv_obj_set_style_text_color(s_action_label, lv_color_hex(LAB_ACTION_COLOR), LV_PART_MAIN);
    lv_obj_align(s_action_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_port_indev_set_group(group);
    lv_scr_load(screen);
}

void lab_indev_test_setup(void)
{
    lv_port_indev_init();
    lab_build_screen();
    (void)lv_timer_create(lab_stats_cb, LAB_INDEV_STATS_PERIOD_MS, NULL);
    printf("U4 | %c sube, %c baja, %c acepta, %c vuelve\n", UI_KEY_UP_CHAR, UI_KEY_DOWN_CHAR,
           UI_KEY_ENTER_CHAR, UI_KEY_BACK_CHAR);
}
