/**
 * @file lab_lvgl_test.c
 * @brief Pruebas de U2 (solo del laboratorio). Todo corre en el core 1.
 */
#include "lab/lab_lvgl_test.h"

#include <stdbool.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "ui/lv_port_disp.h"
#include "ui/squareline/ui.h"
#include "ui/ui_config.h"

#define LAB_TEST1_DURATION_MS    10000u
#define LAB_COUNTER_PERIOD_MS    100u
#define LAB_MEASURE_PERIOD_MS    2000u
#define LAB_COUNTER_OFFSET_Y_PX  24     /* Contador debajo del título */
#define LAB_TITLE_COLOR          0xFFFFFFu
#define LAB_COUNTER_COLOR        0x00FF00u
#define LAB_BG_COLOR             0x1E074Bu /* El violeta de fondo del export */

static lv_obj_t *s_test1_screen = NULL;
static lv_obj_t *s_counter_label = NULL;
static lv_timer_t *s_counter_timer = NULL;
static uint32_t s_counter = 0u;
static uint32_t s_start_ms = 0u;
static bool s_squareline_loaded = false;

static void lab_counter_cb(lv_timer_t *timer)
{
    (void)timer;
    s_counter++;
    lv_label_set_text_fmt(s_counter_label, "%lu", (unsigned long)s_counter);
}

/** Pasa a la prueba 2: carga el export y borra la pantalla de la prueba 1. */
static void lab_load_squareline(void)
{
    lv_timer_del(s_counter_timer);
    s_counter_timer = NULL;
    ui_init();
    lv_obj_del(s_test1_screen);
    s_test1_screen = NULL;
    s_counter_label = NULL;
    s_squareline_loaded = true;
    printf("U2 | prueba 2: pantalla Inicio del export de SquareLine\n");
}

static void lab_measure_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_squareline_loaded && (lv_tick_elaps(s_start_ms) >= LAB_TEST1_DURATION_MS)) {
        lab_load_squareline();
    }

    const uint32_t full_us = lv_port_disp_measure_full_us();

    lv_port_disp_stats_t stats;
    lv_port_disp_get_stats(&stats);
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);

    printf("U2 | prueba %u | refresco completo %lu us | errores flush %lu | init %d | "
           "LVGL libre %lu B (%u%% usado)\n",
           s_squareline_loaded ? 2u : 1u,
           (unsigned long)full_us,
           (unsigned long)stats.flush_errors,
           (int)stats.init_status,
           (unsigned long)mem.free_size,
           (unsigned)mem.used_pct);
}

static void lab_build_test1(void)
{
    s_test1_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_test1_screen, lv_color_hex(LAB_BG_COLOR), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(s_test1_screen);
    lv_label_set_text(title, "REDUX");
    lv_obj_set_style_text_color(title, lv_color_hex(LAB_TITLE_COLOR), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    s_counter_label = lv_label_create(s_test1_screen);
    lv_label_set_text(s_counter_label, "0");
    lv_obj_set_style_text_color(s_counter_label, lv_color_hex(LAB_COUNTER_COLOR), LV_PART_MAIN);
    lv_obj_align(s_counter_label, LV_ALIGN_CENTER, 0, LAB_COUNTER_OFFSET_Y_PX);

    lv_scr_load(s_test1_screen);
}

void lab_lvgl_test_setup(void)
{
    lab_build_test1();
    s_start_ms = lv_tick_get();
    s_counter_timer = lv_timer_create(lab_counter_cb, LAB_COUNTER_PERIOD_MS, NULL);
    (void)lv_timer_create(lab_measure_cb, LAB_MEASURE_PERIOD_MS, NULL);
    printf("U2 | prueba 1: etiqueta REDUX + contador (%u s)\n",
           (unsigned)(LAB_TEST1_DURATION_MS / 1000u));
}
