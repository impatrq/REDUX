/**
 * @file lv_port_disp.c
 * @brief Puente LVGL <-> st7735: dos buffers parciales y flush por DMA.
 */
#include "ui/lv_port_disp.h"

#include <stdbool.h>
#include <stddef.h>

#include "pico/time.h"

#include "lvgl/lvgl.h"
#include "ui/ui_config.h"

#if (LVGL_VERSION_MAJOR != UI_LVGL_VERSION_MAJOR) || \
    (LVGL_VERSION_MINOR != UI_LVGL_VERSION_MINOR) || \
    (LVGL_VERSION_PATCH != UI_LVGL_VERSION_PATCH)
#error "lib/lvgl no es la versión del export de SquareLine (UI_LVGL_VERSION_*)"
#endif

/* El buffer de LVGL se envía tal cual por DMA de 16 bits */
_Static_assert(sizeof(lv_color_t) == sizeof(uint16_t), "lv_color_t debe ser RGB565");

#define LV_PORT_DISP_BUF_PX  (UI_LCD_WIDTH_PX * UI_LVGL_BUF_LINES)

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_color_t s_buf_a[LV_PORT_DISP_BUF_PX];
static lv_color_t s_buf_b[LV_PORT_DISP_BUF_PX];
static lv_disp_t *s_disp = NULL;
static bool s_flush_pending = false;
static lv_port_disp_stats_t s_stats = { ST7735_ERR_BUS, 0u };

/** Cierra el envío en curso (espera con timeout) y le devuelve el buffer a LVGL. */
static void lv_port_disp_complete(lv_disp_drv_t *drv)
{
    if (st7735_write_finish() != ST7735_OK) {
        s_stats.flush_errors++;
    }
    s_flush_pending = false;
    lv_disp_flush_ready(drv);
}

static void lv_port_disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    const uint16_t x = (uint16_t)area->x1;
    const uint16_t y = (uint16_t)area->y1;
    const uint16_t w = (uint16_t)lv_area_get_width(area);
    const uint16_t h = (uint16_t)lv_area_get_height(area);

    if (st7735_write_pixels_start(x, y, w, h, &color_p->full) != ST7735_OK) {
        s_stats.flush_errors++;
        lv_disp_flush_ready(drv); /* Se pierde esta parte, pero LVGL no se traba */
        return;
    }
    s_flush_pending = true;
}

/** LVGL la llama mientras espera que se libere un buffer. */
static void lv_port_disp_wait_cb(lv_disp_drv_t *drv)
{
    if (s_flush_pending) {
        lv_port_disp_complete(drv);
    }
}

void lv_port_disp_init(void)
{
    s_stats.init_status = st7735_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf_a, s_buf_b, LV_PORT_DISP_BUF_PX);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = (lv_coord_t)UI_LCD_WIDTH_PX;
    s_disp_drv.ver_res = (lv_coord_t)UI_LCD_HEIGHT_PX;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp_drv.flush_cb = lv_port_disp_flush_cb;
    s_disp_drv.wait_cb = lv_port_disp_wait_cb;
    s_disp_drv.sw_rotate = 1;  /* Rotación por software: el panel y sus desplazamientos no cambian */
    s_disp = lv_disp_drv_register(&s_disp_drv);

    st7735_backlight_set(true);
}

void lv_port_disp_poll(void)
{
    if (s_flush_pending && !st7735_write_busy()) {
        lv_port_disp_complete(&s_disp_drv);
    }
}

uint32_t lv_port_disp_measure_full_us(void)
{
    const uint64_t t0_us = time_us_64();
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    if (s_flush_pending) {
        lv_port_disp_complete(&s_disp_drv);
    }
    return (uint32_t)(time_us_64() - t0_us);
}

void lv_port_disp_set_rotation(lv_disp_rot_t rotation)
{
    if (s_disp == NULL) {
        return;
    }
    lv_disp_set_rotation(s_disp, rotation);
    lv_obj_invalidate(lv_scr_act());  /* Pantalla cuadrada: el tamaño no cambia, hay que pedir el redibujo */
}

lv_disp_rot_t lv_port_disp_rotate_left(void)
{
    lv_disp_rot_t rotation = LV_DISP_ROT_NONE;
    switch (s_disp_drv.rotated) {
    case LV_DISP_ROT_NONE: rotation = LV_DISP_ROT_90;   break;
    case LV_DISP_ROT_90:   rotation = LV_DISP_ROT_180;  break;
    case LV_DISP_ROT_180:  rotation = LV_DISP_ROT_270;  break;
    case LV_DISP_ROT_270:
    default:               rotation = LV_DISP_ROT_NONE; break;
    }
    lv_port_disp_set_rotation(rotation);
    return rotation;
}

void lv_port_disp_get_stats(lv_port_disp_stats_t *stats)
{
    if (stats != NULL) {
        *stats = s_stats;
    }
}
