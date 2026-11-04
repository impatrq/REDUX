/**
 * @file lv_port_disp.h
 * @brief Puente LVGL <-> st7735: dos buffers parciales y flush por DMA.
 *
 * Solo se usa desde el core 1 (el de LVGL).
 *
 * El flush dispara el DMA y vuelve: LVGL dibuja en el otro buffer mientras
 * tanto. El envío se cierra (con timeout) cuando LVGL necesita el buffer
 * (wait_cb) o cuando el bucle del core 1 llama a lv_port_disp_poll().
 */
#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include <stdint.h>

#include "lvgl/lvgl.h"
#include "ui/st7735.h"

typedef struct {
    st7735_status_t init_status;  /* Resultado de st7735_init() */
    uint32_t flush_errors;        /* Envíos fallidos o con timeout desde el arranque */
} lv_port_disp_stats_t;

/** Inicia el panel, registra el display en LVGL y prende la luz de fondo. Después de lv_init(). */
void lv_port_disp_init(void);

/** Cierra el envío en curso si el DMA ya terminó. No bloquea. Llamar en cada vuelta del bucle. */
void lv_port_disp_poll(void);

/**
 * Invalida toda la pantalla, la redibuja y espera el último envío.
 * Devuelve el tiempo total en µs (dibujo + envío). No llamar desde un flush.
 */
uint32_t lv_port_disp_measure_full_us(void);

/**
 * Gira toda la interfaz por software (LV_DISP_ROT_90 = 90° a la izquierda) y la redibuja.
 * Con 90° y 270° LVGL rota cada bloque en un buffer temporal (LV_DISP_ROT_MAX_BUF) y
 * envía los bloques uno por uno, sin solapar dibujo y envío.
 */
void lv_port_disp_set_rotation(lv_disp_rot_t rotation);

/** Gira 90° más a la izquierda (0° -> 90° -> 180° -> 270° -> 0°). Devuelve la rotación nueva. */
lv_disp_rot_t lv_port_disp_rotate_left(void);

/** Copia el estado del puerto. */
void lv_port_disp_get_stats(lv_port_disp_stats_t *stats);

#endif /* LV_PORT_DISP_H */
