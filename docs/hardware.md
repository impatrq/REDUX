<!-- hardware.md · Pinout, conexiones y alimentación de la placa. Refleja la tabla
     "Pinout definitivo" de docs/PLAN.md; el código lo define en src/hal/board.h. -->

# Hardware

Pinout, conexiones y alimentación del exoesqueleto REDUX. La placa es una Raspberry
Pi Pico 2 W (RP2350). Este documento y `src/hal/board.h` son la única fuente de
verdad del pinout; ningún otro archivo define números de pin.

## Pinout definitivo (acordado para la PCB)

| Señal | GPIO | Pin físico | Notas para el firmware |
| --- | --- | --- | --- |
| Teclado columna 4 | GP0 | 1 | Entrada con pull-up (Fase 9) |
| Libre | GP1 | 2 | — |
| LED de estado | GP2 | 4 | Salida, activo en alto |
| Pulsador | GP3 | 5 | Entrada con pull-up interno, activo en bajo, antirrebote por software |
| AS5600 SDA | GP4 | 6 | I2C0 |
| AS5600 SCL | GP5 | 7 | I2C0, 400 kHz |
| BTS7960 L_EN | GP6 | 9 | Salida; pull-down externo de 10 kΩ |
| BTS7960 R_EN | GP7 | 10 | Salida; pull-down externo de 10 kΩ |
| BTS7960 RPWM (flexión) | GP8 | 11 | PWM slice 4A, 20 kHz |
| BTS7960 LPWM (extensión) | GP9 | 12 | PWM slice 4B, 20 kHz |
| ST7735 SCK | GP10 | 14 | SPI1 (Fase 9) |
| ST7735 MOSI | GP11 | 15 | SPI1 (Fase 9) |
| ST7735 DC | GP12 | 16 | Salida (Fase 9) |
| ST7735 CS | GP13 | 17 | Salida (Fase 9) |
| ST7735 RST | GP14 | 19 | Salida (Fase 9) |
| ST7735 luz de fondo | GP15 | 20 | PWM (Fase 9) |
| Teclado filas 1–4 | GP16–GP19 | 21, 22, 24, 25 | Salidas de barrido (Fase 9) |
| Teclado columnas 1–3 | GP20–GP22 | 26, 27, 29 | Entradas con pull-up (Fase 9) |
| EMG bíceps | GP26 / ADC0 | 31 | Entrada analógica |
| EMG tríceps | GP27 / ADC1 | 32 | Entrada analógica |
| Reservado: corriente IS del BTS7960 | GP28 / ADC2 | 34 | Sin uso en la v1 |

Los pines marcados **Fase 9** (pantalla ST7735 y teclado matricial) están documentados
en `board.h` pero el firmware **no los inicializa** todavía.

**Pines no disponibles:** GP23, GP24, GP25 y GP29 están tomados por el chip inalámbrico
(cyw43) de la Pico 2 W y no se pueden usar.

## Alimentación y masas

- El **BTS7960 se alimenta con una fuente externa** (batería/fuente del motor), no desde
  la Pico. La Pico se alimenta por USB (bring-up) o su propio regulador.
- **Masa común:** la masa de la fuente externa del BTS7960 y la GND de la Pico se unen.
  Las masas se juntan en estrella para que el ruido del motor no contamine la señal EMG
  (ver "Riesgos" en `docs/PLAN.md`).
- La señal EMG y el AS5600 comparten la GND de la Pico.

## Notas de conexión por periférico

### BTS7960 (puente H del motor)
- **R_EN** (GP7) y **L_EN** (GP6) llevan cada uno un **pull-down externo de 10 kΩ a GND**.
  Así el driver queda apagado durante el reinicio o si la Pico no maneja el pin, antes de
  que el firmware tome el control. El firmware además los fuerza a bajo en el arranque
  (`hal_gpio_init`) y, en la Fase 1, nunca los pone en alto: el motor no se mueve.
- **RPWM** (GP8, flexión) y **LPWM** (GP9, extensión): PWM a 20 kHz. Nunca con duty > 0 al
  mismo tiempo (invariante de seguridad, Fase 2).

### AS5600 (encoder magnético, I2C0)
- **SDA** = GP4, **SCL** = GP5, bus a 400 kHz. Dirección I2C 0x36.
- El pin **DIR del AS5600 va a GND**: fija el sentido de conteo del encoder en hardware.
  La corrección de sentido para que la extensión sea positiva se hace por software
  (`sign` de la calibración, Fase 3), no cambiando este cableado.
- Imán montado en soporte rígido a 0.5–3 mm; el firmware chequea MD/ML/MH.

### Pulsador
- GP3 a GND a través del pulsador, con **pull-up interno** de la Pico habilitado: en
  reposo lee alto, presionado lee bajo (**activo en bajo**). Antirrebote por software.

### LED de estado
- GP2 a LED + resistencia a GND. **Activo en alto.** En el firmware parpadea; en el
  programa de prueba `fase1_check` se maneja por PWM para variar el brillo.
