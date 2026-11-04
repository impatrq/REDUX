/**
 * @file ui_core1.c
 * @brief Bucle del core 1: LVGL y nada más.
 */
#include "ui/ui_core1.h"

#include <stddef.h>
#include <stdint.h>

#include "pico/multicore.h"
#include "pico/time.h"

#include "lvgl/lvgl.h"
#include "ui/lv_port_disp.h"
#include "ui/ui_config.h"

#define UI_CORE1_STACK_WORDS  (UI_CORE1_STACK_BYTES / sizeof(uint32_t))

static uint32_t s_core1_stack[UI_CORE1_STACK_WORDS];
static ui_core1_setup_fn_t s_setup = NULL; /* Se escribe antes de lanzar el core 1 */

static void ui_core1_entry(void)
{
    lv_init();
    lv_port_disp_init();
    if (s_setup != NULL) {
        s_setup();
    }

    for (;;) {
        (void)lv_timer_handler();
        lv_port_disp_poll();
        sleep_ms(UI_LVGL_HANDLER_PERIOD_MS);
    }
}

void ui_core1_start(ui_core1_setup_fn_t setup)
{
    s_setup = setup;
    multicore_launch_core1_with_stack(ui_core1_entry, s_core1_stack, sizeof(s_core1_stack));
}
