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
| LED de estado | GP2 | 4 | Solo desarrollo; no va en la PCB final |
| Pulsador | GP3 | 5 | Solo desarrollo; no va en la PCB final. Pull-up interno, activo en bajo |
| AS5600 SDA | GP4 | 6 | I2C0 |
| AS5600 SCL | GP5 | 7 | I2C0, 400 kHz |
| Libre | GP6 | 9 | R_EN/L_EN del BTS7960 van fijos a +5 V del lado del motor; el firmware no los maneja |
| Libre | GP7 | 10 | Ídem |
| BTS7960 RPWM (flexión) | GP8 | 11 | PWM slice 4A, 20 kHz, a través de un 6N137 (no invierte) |
| BTS7960 LPWM (extensión) | GP9 | 12 | PWM slice 4B, 20 kHz, a través de un 6N137 (no invierte) |
| ST7735 SCK | GP10 | 14 | SPI1 (Fase 9) |
| ST7735 MOSI | GP11 | 15 | SPI1 (Fase 9) |
| ST7735 DC | GP12 | 16 | Salida (Fase 9) |
| ST7735 CS | GP13 | 17 | Salida (Fase 9) |
| ST7735 RST | GP14 | 19 | Salida (Fase 9) |
| Libre | GP15 | 20 | La luz de fondo (LED) de la ST7735 va fija a 3V3 en la PCB |
| Teclado filas 1–4 | GP16–GP19 | 21, 22, 24, 25 | Salidas de barrido (Fase 9) |
| Teclado columnas 1–3 | GP20–GP22 | 26, 27, 29 | Entradas con pull-up (Fase 9) |
| EMG bíceps | GP26 / ADC0 | 31 | Señal ya rectificada (onda completa), sin suavizar; 0–3 V |
| EMG tríceps | GP27 / ADC1 | 32 | Ídem |
| Libre | GP1, GP28 | 2, 34 | — |

Los pines marcados **Fase 9** (pantalla ST7735 y teclado matricial) están documentados
en `board.h` pero el firmware **no los inicializa** todavía.

**Pines de solo desarrollo:** el LED de estado (GP2) y el pulsador (GP3) se usan en
los programas de banco (`fase1_check`, `ui_basic`), pero **no van en la PCB final**: la
caja definitiva lleva pantalla y teclado.

**Pines no disponibles:** GP23, GP24, GP25 y GP29 están tomados por el chip inalámbrico
(cyw43) de la Pico 2 W y no se pueden usar.

## Alimentación y aislación (dos masas)

La PCB tiene **dos masas que no se tocan**, para que el ruido del motor no contamine la
señal EMG ni la lógica:

- **GND1 (lado de potencia):** motor, driver BTS7960 y el lado de salida de los 6N137.
  Se alimenta desde los 12 V de la batería/fuente con un **LM2596** que baja a 5 V.
- **GND2 (lado de lógica):** Pico, AS5600, pantalla, teclado y EMG. Se alimenta con
  **5 V aislados** (módulo **B0505S** → VSYS de la Pico) y, para el EMG, con **±5 V
  aislados** aparte.

Entre los dos lados, el único cruce de señal son **RPWM y LPWM a través de
optoacopladores 6N137** (uno por señal). Los 6N137 **no invierten**: GPIO en alto →
salida en alto hacia el driver.

**Comportamiento con la Pico apagada o en reset:** los 6N137 dejan **RPWM y LPWM en
alto a la vez**, lo que para el BTS7960 equivale a **frenar** el motor (no moverlo). El
arranque es seguro sin depender de ningún enable.

## Notas de conexión por periférico

### BTS7960 (puente H del motor)
- **R_EN** (GP6) y **L_EN** (GP7) **ya no los maneja el firmware**: en la PCB van
  **fijos a +5 V del lado del motor** (siempre habilitados). Por eso GP6 y GP7 quedan
  libres y `bts7960_enable()`/`hal_gpio_motor_enable()` no tienen efecto por hardware
  (ver `src/actuators/bts7960.h`). El corte real del motor depende de dejar RPWM y LPWM
  en 0.
- **RPWM** (GP8, flexión) y **LPWM** (GP9, extensión): PWM a 20 kHz, cada una a través de
  un **6N137** (no invierte). Nunca con duty > 0 al mismo tiempo (invariante de
  seguridad, Fase 2).
- Alimentación y masa del driver en **GND1** (ver arriba).

### AS5600 (encoder magnético, I2C0)
- **SDA** = GP4, **SCL** = GP5, bus a 400 kHz. Dirección I2C 0x36. Alimentado en **GND2**.
- El pin **DIR del AS5600 va a GND2**: fija el sentido de conteo del encoder en hardware.
  La corrección de sentido para que la extensión sea positiva se hace por software
  (`sign` de la calibración, Fase 3), no cambiando este cableado.
- Imán montado en soporte rígido a 0.5–3 mm; el firmware chequea MD/ML/MH.

### EMG (canal analógico, Fase 4)
- La señal llega **ya rectificada (onda completa) y sin suavizar**, en el rango **0–3 V**,
  con **2k2 en serie** hacia la entrada del ADC. Su masa va a **AGND / GND2**.
- GP26 (ADC0) = bíceps, GP27 (ADC1) = tríceps.

### Pulsador (solo desarrollo)
- GP3 a GND a través del pulsador, con **pull-up interno** de la Pico habilitado: en
  reposo lee alto, presionado lee bajo (**activo en bajo**). Antirrebote por software.
- No va en la PCB final: lo reemplaza el teclado matricial.

### LED de estado (solo desarrollo)
- GP2 a LED + resistencia a GND. **Activo en alto.** En el firmware parpadea; en el
  programa de prueba `fase1_check` se maneja por PWM para variar el brillo.
- No va en la PCB final: lo reemplaza la pantalla.

## Banco de pruebas (protoboard)

El banco (`fase1_check`, `fase2_check`, `fase3_check`) corre sobre protoboard, no sobre
la PCB. Para que se comporte igual que la PCB:

- **R_EN y L_EN del BTS7960 van a +5 V del driver**, no a GP6/GP7. En el banco el
  firmware tampoco maneja los enable: quedan siempre habilitados y el control es solo
  por RPWM/LPWM, igual que en la PCB. GP6 y GP7 quedan sin conectar.
- Se puede probar con el motor **sin carga**. `fase2_check` limita el duty y aplica
  freno/hombre muerto; `fase3_check` deja el motor deshabilitado (no se usa).
- Si se dispone de optoacopladores en el banco, intercalar los 6N137 en RPWM/LPWM
  reproduce también el estado seguro con la Pico apagada (ambas señales en alto →
  freno). Sin ellos, las señales van directo al driver y el comportamiento al reset
  depende de los pull-down del propio driver.
