// board.h · Números de GPIO y canales ADC de la placa (pinout). No debe estar
// duplicado en ningún otro archivo. Refleja exactamente la tabla "Pinout definitivo"
// de docs/PLAN.md.
#ifndef REDUX_HAL_BOARD_H
#define REDUX_HAL_BOARD_H

// --- v1: pines activos --------------------------------------------------------
// Señal                      GPIO   Pin físico   Notas
#define PIN_LED 2u          // LED de estado        4    SOLO DESARROLLO, no va en la PCB final; salida activo en alto
#define PIN_BUTTON 3u       // Pulsador             5    SOLO DESARROLLO, no va en la PCB final; pull-up interno, activo en bajo

// AS5600 por I2C0 (Fase 3). DIR del encoder va a GND: el sentido se corrige por software.
#define PIN_I2C_SDA 4u      // AS5600 SDA           6    I2C0
#define PIN_I2C_SCL 5u      // AS5600 SCL           7    I2C0, 400 kHz
#define BOARD_I2C_INST i2c0 // instancia de I2C de los pines de arriba

// BTS7960 (Fase 2). R_EN y L_EN NO los maneja el firmware: en la PCB van fijos a +5 V
// del lado del motor, así que GP6 y GP7 quedan libres. El driver solo se controla por
// RPWM/LPWM, que pasan por optoacopladores 6N137 (no invierten).
#define PIN_MOTOR_RPWM 8u   // BTS7960 RPWM (flex) 11    PWM slice 4A, 20 kHz, vía 6N137
#define PIN_MOTOR_LPWM 9u   // BTS7960 LPWM (ext)  12    PWM slice 4B, 20 kHz, vía 6N137

// EMG por ADC (Fase 4). Señal ya rectificada (onda completa), sin suavizar, 0–3 V.
#define PIN_EMG_BICEPS 26u  // EMG bíceps          31    ADC0
#define PIN_EMG_TRICEPS 27u // EMG tríceps         32    ADC1
#define ADC_CH_BICEPS 0u    // canal ADC de GP26
#define ADC_CH_TRICEPS 1u   // canal ADC de GP27

// --- v1: libres ---------------------------------------------------------------
// GP1 (pin 2) y GP28 (pin 34, ADC2): libres, sin uso en la v1.
// GP6 (pin 9) y GP7 (pin 10): libres; R_EN/L_EN del BTS7960 van fijos a +5 V del motor.
// GP23, GP24, GP25 y GP29 no están disponibles (chip inalámbrico cyw43).

// --- Fase 9: documentados, sin inicializar ------------------------------------
// Pantalla TFT ST7735 por SPI1 y teclado matricial de membrana 4x4. La luz de fondo
// de la ST7735 va fija a 3V3 en la PCB, por eso GP15 queda libre (no se define aquí).
#define PIN_KEYPAD_COL4 0u  // Teclado columna 4    1    entrada con pull-up (Fase 9)
#define PIN_TFT_SCK 10u     // ST7735 SCK          14    SPI1 (Fase 9)
#define PIN_TFT_MOSI 11u    // ST7735 MOSI         15    SPI1 (Fase 9)
#define PIN_TFT_DC 12u      // ST7735 DC           16    salida (Fase 9)
#define PIN_TFT_CS 13u      // ST7735 CS           17    salida (Fase 9)
#define PIN_TFT_RST 14u     // ST7735 RST          19    salida (Fase 9)
#define PIN_KEYPAD_ROW1 16u // Teclado fila 1      21    salida de barrido (Fase 9)
#define PIN_KEYPAD_ROW2 17u // Teclado fila 2      22    salida de barrido (Fase 9)
#define PIN_KEYPAD_ROW3 18u // Teclado fila 3      24    salida de barrido (Fase 9)
#define PIN_KEYPAD_ROW4 19u // Teclado fila 4      25    salida de barrido (Fase 9)
#define PIN_KEYPAD_COL1 20u // Teclado columna 1   26    entrada con pull-up (Fase 9)
#define PIN_KEYPAD_COL2 21u // Teclado columna 2   27    entrada con pull-up (Fase 9)
#define PIN_KEYPAD_COL3 22u // Teclado columna 3   29    entrada con pull-up (Fase 9)

#endif // REDUX_HAL_BOARD_H
