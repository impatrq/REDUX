// board.h · Números de GPIO y canales ADC de la placa (pinout). No debe estar
// duplicado en ningún otro archivo. Refleja exactamente la tabla "Pinout definitivo"
// de docs/PLAN.md.
#ifndef REDUX_HAL_BOARD_H
#define REDUX_HAL_BOARD_H

// --- v1: pines activos --------------------------------------------------------
// Señal                      GPIO   Pin físico   Notas
#define PIN_LED 2u          // LED de estado        4    salida, activo en alto
#define PIN_BUTTON 3u       // Pulsador             5    entrada, pull-up interno, activo en bajo

// AS5600 por I2C0 (Fase 3). DIR del encoder va a GND: el sentido se corrige por software.
#define PIN_I2C_SDA 4u      // AS5600 SDA           6    I2C0
#define PIN_I2C_SCL 5u      // AS5600 SCL           7    I2C0, 400 kHz
#define BOARD_I2C_INST i2c0 // instancia de I2C de los pines de arriba

// BTS7960 (Fase 2). R_EN/L_EN con pull-down externo de 10 kΩ (apagado al reset).
#define PIN_MOTOR_L_EN 6u   // BTS7960 L_EN         9    salida
#define PIN_MOTOR_R_EN 7u   // BTS7960 R_EN        10    salida
#define PIN_MOTOR_RPWM 8u   // BTS7960 RPWM (flex) 11    PWM slice 4A, 20 kHz
#define PIN_MOTOR_LPWM 9u   // BTS7960 LPWM (ext)  12    PWM slice 4B, 20 kHz

// EMG por ADC (Fase 4).
#define PIN_EMG_BICEPS 26u  // EMG bíceps          31    ADC0
#define PIN_EMG_TRICEPS 27u // EMG tríceps         32    ADC1
#define ADC_CH_BICEPS 0u    // canal ADC de GP26
#define ADC_CH_TRICEPS 1u   // canal ADC de GP27

// --- v1: reservado / libre ----------------------------------------------------
#define PIN_MOTOR_IS 28u    // Corriente IS del BTS7960  34   ADC2, sin uso en la v1
#define ADC_CH_MOTOR_IS 2u  // canal ADC de GP28 (reservado)
// GP1 (pin 2): libre.
// GP23, GP24, GP25 y GP29 no están disponibles (chip inalámbrico cyw43).

// --- Fase 9: documentados, sin inicializar ------------------------------------
// Pantalla TFT ST7735 por SPI1 y teclado matricial de membrana 4x4.
#define PIN_KEYPAD_COL4 0u  // Teclado columna 4    1    entrada con pull-up (Fase 9)
#define PIN_TFT_SCK 10u     // ST7735 SCK          14    SPI1 (Fase 9)
#define PIN_TFT_MOSI 11u    // ST7735 MOSI         15    SPI1 (Fase 9)
#define PIN_TFT_DC 12u      // ST7735 DC           16    salida (Fase 9)
#define PIN_TFT_CS 13u      // ST7735 CS           17    salida (Fase 9)
#define PIN_TFT_RST 14u     // ST7735 RST          19    salida (Fase 9)
#define PIN_TFT_BL 15u      // ST7735 luz de fondo 20    PWM (Fase 9)
#define PIN_KEYPAD_ROW1 16u // Teclado fila 1      21    salida de barrido (Fase 9)
#define PIN_KEYPAD_ROW2 17u // Teclado fila 2      22    salida de barrido (Fase 9)
#define PIN_KEYPAD_ROW3 18u // Teclado fila 3      24    salida de barrido (Fase 9)
#define PIN_KEYPAD_ROW4 19u // Teclado fila 4      25    salida de barrido (Fase 9)
#define PIN_KEYPAD_COL1 20u // Teclado columna 1   26    entrada con pull-up (Fase 9)
#define PIN_KEYPAD_COL2 21u // Teclado columna 2   27    entrada con pull-up (Fase 9)
#define PIN_KEYPAD_COL3 22u // Teclado columna 3   29    entrada con pull-up (Fase 9)

#endif // REDUX_HAL_BOARD_H
