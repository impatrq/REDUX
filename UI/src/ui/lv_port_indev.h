/**
 * @file lv_port_indev.h
 * @brief Puente LVGL <-> teclado 4x4: dispositivo de entrada tipo KEYPAD.
 *
 * Solo se usa desde el core 1 (el de LVGL).
 *
 * Un timer de LVGL barre el teclado cada UI_KEYPAD_SCAN_PERIOD_MS y LVGL lee
 * los eventos con el mismo período. Teclas (docs/ui_contract.md §6):
 * UI_KEY_UP_CHAR = LV_KEY_PREV, UI_KEY_ENTER_CHAR = LV_KEY_ENTER,
 * UI_KEY_DOWN_CHAR = LV_KEY_NEXT, UI_KEY_BACK_CHAR = LV_KEY_ESC,
 * UI_KEY_ERASE_CHAR = LV_KEY_BACKSPACE, '0'…'9' = su carácter. UI_KEY_ROTATE_CHAR
 * no va a LVGL: se entrega al callback de lv_port_indev_set_key_cb().
 * Hay una sola tecla activa a la vez: mientras una está apretada, las demás se ignoran.
 */
#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#include <stdint.h>

#include "lvgl/lvgl.h"

typedef struct {
    uint32_t delivered_events;  /* Presionar/soltar entregados a LVGL */
    uint32_t ignored_events;    /* Teclas sin uso o apretadas con otra ya activa */
} lv_port_indev_stats_t;

/** Inicia el teclado y registra el dispositivo en LVGL. Después de lv_port_disp_init(). */
void lv_port_indev_init(void);

/** Recibe una tecla que no va a LVGL, una vez por pulsación. Corre dentro de LVGL (core 1). */
typedef void (*lv_port_indev_key_cb_t)(char key);

/** Registra quién recibe las teclas que no van a LVGL (NULL = nadie). */
void lv_port_indev_set_key_cb(lv_port_indev_key_cb_t cb);

/** Conecta el teclado al grupo de la pantalla visible (NULL = ninguno). */
void lv_port_indev_set_group(lv_group_t *group);

/**
 * Cantidad de pulsaciones entregadas a LVGL desde el arranque. Cambia una vez
 * por pulsación (no con la repetición de LVGL al mantener la tecla): sirve para
 * reaccionar una sola vez a una tecla mantenida.
 */
uint32_t lv_port_indev_get_press_count(void);

/** Copia los contadores del puerto. */
void lv_port_indev_get_stats(lv_port_indev_stats_t *stats);

#endif /* LV_PORT_INDEV_H */
