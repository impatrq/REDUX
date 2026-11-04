/**
 * @file st7735.h
 * @brief Driver del panel ST7735 1,44" 128x128 (RGB565) sobre hal_spi.
 *
 * Coordenadas lógicas 0..UI_LCD_WIDTH_PX-1 / 0..UI_LCD_HEIGHT_PX-1; el
 * desplazamiento propio del módulo (UI_LCD_*_OFFSET_PX) se suma acá.
 * Colores en RGB565 nativo (uint16_t); el bus los envía MSB primero.
 */
#ifndef ST7735_H
#define ST7735_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hal/hal_spi.h"

/* Colores RGB565 básicos para pruebas */
#define ST7735_COLOR_BLACK   0x0000u
#define ST7735_COLOR_WHITE   0xFFFFu
#define ST7735_COLOR_RED     0xF800u
#define ST7735_COLOR_GREEN   0x07E0u
#define ST7735_COLOR_BLUE    0x001Fu

typedef enum {
    ST7735_OK = 0,
    ST7735_ERR_BUS,     /* Falló hal_spi (timeout, ocupado, sin iniciar) */
    ST7735_ERR_RANGE,   /* Rectángulo fuera de la pantalla o vacío */
    ST7735_ERR_BUSY,    /* Hay un envío sin terminar (falta st7735_write_finish()) */
} st7735_status_t;

/** Inicializa el bus, resetea el panel y lo deja encendido (luz de fondo apagada). */
st7735_status_t st7735_init(void);

/** Prende o apaga la luz de fondo. */
void st7735_backlight_set(bool on);

/** Rellena un rectángulo con un color. Bloquea hasta terminar (con timeout). */
st7735_status_t st7735_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/** Rellena toda la pantalla con un color. */
st7735_status_t st7735_fill_screen(uint16_t color);

/**
 * Escribe w×h píxeles (fila por fila) en el rectángulo dado.
 * Bloquea hasta terminar (con timeout).
 */
st7735_status_t st7735_write_pixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                    const uint16_t *pixels);

/**
 * Como st7735_write_pixels() pero sin esperar: fija la ventana, dispara el DMA
 * y vuelve. @p pixels debe seguir vivo hasta st7735_write_finish().
 * Un solo envío a la vez.
 */
st7735_status_t st7735_write_pixels_start(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                          const uint16_t *pixels);

/** true mientras el envío iniciado con st7735_write_pixels_start() siga en curso. */
bool st7735_write_busy(void);

/**
 * Espera (con timeout) a que termine el envío iniciado y libera CS.
 * Sin envío pendiente devuelve ST7735_OK sin hacer nada.
 */
st7735_status_t st7735_write_finish(void);

#endif /* ST7735_H */
