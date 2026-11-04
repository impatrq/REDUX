/**
 * @file board.h
 * @brief Asignación de pines de REDUX usada por la línea UI.
 *
 * PROVISORIO: armado desde la tabla "Pinout definitivo" del plan principal
 * (docs/PLAN_UI.md, sección 2). El board.h real está en redux-firmware, en la
 * computadora del compañero. Cuando llegue (sincronización S1), se reemplaza
 * este archivo por aquel y se anota en docs/sync_log.md.
 *
 * No cambiar pines acá por cuenta propia.
 */
#ifndef BOARD_H
#define BOARD_H

/* ---- Pantalla ST7735 128x128 por SPI1 ---- */
#define BOARD_LCD_SPI_INDEX      1u   /* SPI1 */
#define BOARD_PIN_LCD_SCK        10u  /* SPI1 SCK, pin físico 14 */
#define BOARD_PIN_LCD_MOSI       11u  /* SPI1 TX,  pin físico 15 */
#define BOARD_PIN_LCD_DC         12u  /* DC / A0,  pin físico 16 */
#define BOARD_PIN_LCD_CS         13u  /* CS manual, pin físico 17 */
#define BOARD_PIN_LCD_RST        14u  /* RST / RES, pin físico 19 */
#define BOARD_PIN_LCD_BL         15u  /* Luz de fondo (PWM), pin físico 20 */

/* ---- Teclado matricial 4x4 ---- */
#define BOARD_KEYPAD_ROWS        4u
#define BOARD_KEYPAD_COLS        4u
#define BOARD_PIN_KEY_ROW1       16u  /* Salidas de barrido */
#define BOARD_PIN_KEY_ROW2       17u
#define BOARD_PIN_KEY_ROW3       18u
#define BOARD_PIN_KEY_ROW4       19u
#define BOARD_PIN_KEY_COL1       20u  /* Entradas con pull-up */
#define BOARD_PIN_KEY_COL2       21u
#define BOARD_PIN_KEY_COL3       22u
#define BOARD_PIN_KEY_COL4       0u

/* ---- Pines del firmware que la UI NO debe usar ----
 * GP2 = LED de estado, GP3 = pulsador (ui_basic), I2C0 = encoder AS5600.
 * Puente H BTS7960 y entradas EMG: números a confirmar en S1. */
#define BOARD_PIN_STATUS_LED     2u
#define BOARD_PIN_USER_BUTTON    3u

#endif /* BOARD_H */
