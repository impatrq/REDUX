/**
 * @file lab_screens_test.c
 * @brief Prueba de las pantallas (solo del laboratorio). Todo corre en el core 1.
 */
#include "lab/lab_screens_test.h"

#include <stdint.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "app/ui.h"
#include "ui/lv_port_disp.h"
#include "ui/ui_config.h"
#include "ui/ui_panel.h"

#define LAB_BOOT_MS            2000u
#define LAB_RUN_FAULT_MS       15000u   /* Marcha antes de la falla simulada */
#define LAB_ROM_DEG            120.0f   /* Rango "medido" en la calibración falsa */
#define LAB_ANGLE_STEP_DEG     3.0f     /* Por cada UI_STATUS_PERIOD_MS en marcha */

static const uint32_t s_lab_faults[] = {
    UI_FAULT_OVERCURRENT, UI_FAULT_ENCODER, UI_FAULT_ANGLE_LIMIT, UI_FAULT_EMG, UI_FAULT_OTHER,
};
#define LAB_FAULT_COUNT        (sizeof(s_lab_faults) / sizeof(s_lab_faults[0]))

static const char *const s_mode_names[] = {
    [UI_MODE_BOOT] = "BOOT", [UI_MODE_CALIB_ROM] = "CALIB_ROM", [UI_MODE_CALIB_EMG] = "CALIB_EMG",
    [UI_MODE_RUN] = "RUN", [UI_MODE_FAULT] = "FAULT", [UI_MODE_IDLE] = "IDLE",
};
static const char *const s_event_names[] = {
    [UI_EV_NONE] = "NONE", [UI_EV_CONFIRM] = "CONFIRM", [UI_EV_CANCEL] = "CANCEL",
    [UI_EV_RECALIBRATE] = "RECALIBRATE",
};

static ui_status_t s_status = {
    .mode = UI_MODE_BOOT, .prompt = UI_PROMPT_NONE, .angle_deg = 0.0f,
    .rom_deg = LAB_ROM_DEG, .faults = UI_FAULT_NONE, .duty = 0.0f,
};
static uint32_t s_mode_start_ms = 0u;
static float s_angle_dir = 1.0f;     /* +1 extiende, -1 flexiona */
static uint32_t s_next_fault = 0u;   /* Índice en s_lab_faults */

static void lab_set_mode(ui_mode_t mode, ui_prompt_t prompt)
{
    s_status.mode = mode;
    s_status.prompt = prompt;
    s_mode_start_ms = lv_tick_get();
    printf("UI | modo %s, prompt %u\n", s_mode_names[mode], (unsigned)prompt);
}

/** Lo que haría el firmware con cada evento de la UI. */
static void lab_on_event(ui_event_t event)
{
    printf("UI | evento UI_EV_%s\n", s_event_names[event]);

    switch (s_status.mode) {
    case UI_MODE_IDLE:
        if (event == UI_EV_CONFIRM) {
            lab_set_mode(UI_MODE_CALIB_ROM, UI_PROMPT_GO_FLEX);
        }
        break;

    case UI_MODE_CALIB_ROM:
        if (event == UI_EV_CANCEL) {
            lab_set_mode(UI_MODE_IDLE, UI_PROMPT_NONE);
        } else if (s_status.prompt == UI_PROMPT_GO_FLEX) {
            printf("UI | flexion maxima (el guardado lo hace el firmware)\n");
            lab_set_mode(UI_MODE_CALIB_ROM, UI_PROMPT_GO_EXT);
        } else {
            printf("UI | extension maxima (el guardado lo hace el firmware)\n");
            s_status.rom_deg = LAB_ROM_DEG;
            s_status.angle_deg = 0.0f;
            s_angle_dir = 1.0f;
            lab_set_mode(UI_MODE_RUN, UI_PROMPT_NONE);
        }
        break;

    case UI_MODE_RUN:
        if (event == UI_EV_CANCEL) {
            s_status.duty = 0.0f;
            lab_set_mode(UI_MODE_IDLE, UI_PROMPT_NONE);
        }
        break;

    case UI_MODE_FAULT:
        if (event == UI_EV_CONFIRM) {
            s_status.faults = UI_FAULT_NONE;
            lab_set_mode(UI_MODE_IDLE, UI_PROMPT_NONE);
        }
        break;

    case UI_MODE_BOOT:
    default:
        break;
    }
}

/** Ángulo que sube y baja entre 0 y el rango; la potencia falsa lo sigue (0 a 100 %). */
static void lab_update_run(void)
{
    s_status.angle_deg += s_angle_dir * LAB_ANGLE_STEP_DEG;
    if (s_status.angle_deg >= s_status.rom_deg) {
        s_status.angle_deg = s_status.rom_deg;
        s_angle_dir = -1.0f;
    } else if (s_status.angle_deg <= 0.0f) {
        s_status.angle_deg = 0.0f;
        s_angle_dir = 1.0f;
    } else {
        /* Sigue en la misma dirección. */
    }
    s_status.duty = s_status.angle_deg / s_status.rom_deg;

    if (lv_tick_elaps(s_mode_start_ms) >= LAB_RUN_FAULT_MS) {
        s_status.faults = s_lab_faults[s_next_fault];
        s_next_fault = (s_next_fault + 1u) % (uint32_t)LAB_FAULT_COUNT;
        s_status.duty = 0.0f;
        lab_set_mode(UI_MODE_FAULT, UI_PROMPT_NONE);
        printf("UI | falla simulada 0x%08lX\n", (unsigned long)s_status.faults);
    }
}

static void lab_status_cb(lv_timer_t *timer)
{
    (void)timer;
    if ((s_status.mode == UI_MODE_BOOT) && (lv_tick_elaps(s_mode_start_ms) >= LAB_BOOT_MS)) {
        lab_set_mode(UI_MODE_IDLE, UI_PROMPT_NONE);
    } else if (s_status.mode == UI_MODE_RUN) {
        lab_update_run();
    } else {
        /* Los demás modos esperan eventos. */
    }
    ui_panel_show_status(&s_status);
}

void lab_screens_test_setup(void)
{
    ui_panel_init();
    ui_panel_set_event_cb(lab_on_event);
    lab_set_mode(UI_MODE_BOOT, UI_PROMPT_NONE);
    ui_panel_show_status(&s_status);
    (void)lv_timer_create(lab_status_cb, UI_STATUS_PERIOD_MS, NULL);
    printf("UI | %c sube, %c baja, %c acepta, %c vuelve, %c borra (Perfil), %c gira\n", UI_KEY_UP_CHAR,
           UI_KEY_DOWN_CHAR, UI_KEY_ENTER_CHAR, UI_KEY_BACK_CHAR, UI_KEY_ERASE_CHAR, UI_KEY_ROTATE_CHAR);

    /* Costo de la rotación por software: refresco completo a 0° y a 90° (vuelve a 0°). */
    const uint32_t full_0_us = lv_port_disp_measure_full_us();
    lv_port_disp_set_rotation(LV_DISP_ROT_90);
    const uint32_t full_90_us = lv_port_disp_measure_full_us();
    lv_port_disp_set_rotation(LV_DISP_ROT_NONE);
    printf("UI | refresco completo: %lu us a 0 grados, %lu us a 90 grados\n",
           (unsigned long)full_0_us, (unsigned long)full_90_us);

    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    printf("UI | 9 pantallas creadas | LVGL libre %lu B (%u%% usado)\n",
           (unsigned long)mem.free_size, (unsigned)mem.used_pct);
}
