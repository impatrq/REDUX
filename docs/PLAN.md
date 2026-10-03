# REDUX · Plan de implementación del firmware en C

Sep 30, 2026 · @Redux

## Objetivo, alcance y decisiones técnicas

El firmware se escribe en C11 sobre el Raspberry Pi Pico SDK 2.x, se compila con CMake para la placa `pico2_w` y se carga como UF2. Reemplaza al prototipo en MicroPython y conserva su lógica: EMG con XOR, PWM proporcional a la intensidad, límites articulares con el AS5600 y paradas de seguridad.

**Alcance de la versión 1**

- Lectura de dos canales EMG por ADC, con envolvente, umbral e histéresis.
- Lógica XOR de intención con bloqueo por co-contracción.
- Control del BTS7960 con PWM a 20 kHz y rampas de aceleración y frenado.
- Lectura del AS5600 por I2C, límites calibrados por paciente (flexión máxima = 0°, extensión máxima = rango) y detección de fallas del imán o del bus.
- Máquina de estados de seguridad, watchdog y telemetría por USB.

**Fuera de alcance (fases posteriores):** Wi-Fi y API Flask, control PID de posición, trayectorias preprogramadas.

| Tema | Decisión | Motivo |
| --- | --- | --- |
| Lenguaje | C11, sin memoria dinámica (`malloc` prohibido) | Comportamiento determinista y sin fragmentación |
| SDK y placa | Pico SDK 2.x, `PICO_BOARD=pico2_w`, `PICO_PLATFORM=rp2350-arm-s` | Soporte oficial del RP2350 y del Cortex-M33 con FPU |
| Build | CMake + Ninja, `-Wall -Wextra -Werror` | Los avisos se tratan como errores desde el día uno |
| Ritmo del control | Tick de 1 ms generado por `repeating_timer`; la ISR solo levanta una bandera y el lazo corre en `main` | Nada pesado dentro de interrupciones |
| Núcleos | Core 0: sensores, control y seguridad. Core 1: telemetría (y Wi-Fi a futuro) | Un `printf` lento nunca demora el control |
| Aritmética | `float` de 32 bits | El M33 tiene FPU; el código queda legible |
| Parámetros | Todos en `config.h`; la calibración se guarda en flash en la fase 7 | Un solo lugar para ajustar |
| LED de estado | LED externo en un GPIO | El LED integrado de la Pico 2 W depende del chip inalámbrico (`cyw43`), innecesario en la v1 |
| Pruebas | La lógica pura se compila también en la PC y se prueba con Unity | Se valida sin hardware y sin riesgo |

**Pinout definitivo (acordado para la PCB).** `hal/board.h` debe reflejar exactamente esta tabla; ningún otro archivo define números de pin.

| Señal | GPIO | Pin físico | Notas para el firmware |
| --- | --- | --- | --- |
| Teclado columna 4 | GP0 | 1 | Entrada con pull-up (Fase 9) |
| LED de estado | GP2 | 4 | Salida, activo en alto |
| Pulsador | GP3 | 5 | Entrada con pull-up interno, activo en bajo, antirrebote por software |
| AS5600 SDA | GP4 | 6 | I2C0 |
| AS5600 SCL | GP5 | 7 | I2C0, 400 kHz |
| BTS7960 L\_EN | GP6 | 9 | Salida; pull-down externo de 10 kΩ |
| BTS7960 R\_EN | GP7 | 10 | Salida; pull-down externo de 10 kΩ |
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
| Libre | GP1 | 2 | — |

El BTS7960 se alimenta con una fuente externa y comparte masa con la Pico. El DIR del AS5600 va a GND, así que el sentido del ángulo se corrige solo por software (`sign` de la calibración). GP23, GP24, GP25 y GP29 no están disponibles (chip inalámbrico).

## Estructura del repositorio

Cada carpeta de `src/` es una capa y cada par `.c/.h` tiene un único rol. Solo `hal/` y `main.c` incluyen headers del Pico SDK; el resto es C portable que también compila en la PC para las pruebas.

```text
redux-firmware/
├── CLAUDE.md                  # Reglas del proyecto para Claude Code
├── README.md                  # Cómo compilar, cargar y usar
├── CMakeLists.txt             # Build del firmware (pico2_w)
├── pico_sdk_import.cmake      # Copiado del SDK
├── .clang-format              # Estilo de código único
├── .gitignore                 # build/, *.uf2, etc.
├── docs/
│   ├── PLAN.md                # Este plan exportado
│   ├── hardware.md            # Pinout, conexiones, alimentación
│   ├── state_machine.md       # Estados y transiciones
│   └── decisions/             # ADR-001-*.md: una decisión por archivo
├── src/
│   ├── main.c                 # Punto de entrada: init + lazo principal
│   ├── common/
│   │   ├── config.h           # Parámetros ajustables (umbrales, límites, PWM)
│   │   ├── redux_types.h      # Tipos compartidos: direction_t, fault_t, status_t
│   │   └── util.h             # clamp, map lineal, slew (inline)
│   ├── hal/                   # Único lugar que toca el hardware
│   │   ├── board.h            # Pinout de la placa
│   │   ├── hal_time.c/.h      # Tick de 1 ms, tiempo en µs, delays
│   │   ├── hal_adc.c/.h       # Lectura de canales ADC
│   │   ├── hal_pwm.c/.h       # Configuración y duty de PWM
│   │   ├── hal_i2c.c/.h       # Lectura de registros con timeout
│   │   ├── hal_gpio.c/.h      # Pines de enable y LED
│   │   └── hal_wdt.c/.h       # Watchdog
│   ├── sensors/
│   │   ├── emg.c/.h           # Canal EMG: filtro, umbral, histéresis, intensidad
│   │   ├── emg_calib.c/.h     # Calibración de reposo y de contracción máxima
│   │   └── as5600.c/.h        # Driver del encoder: ángulo y estado del imán
│   ├── actuators/
│   │   └── bts7960.c/.h       # Driver del puente H
│   ├── control/
│   │   ├── intent.c/.h        # Lógica XOR: dirección deseada
│   │   ├── power.c/.h         # Intensidad → duty, rampas
│   │   └── limits.c/.h        # Límites articulares y zona de freno
│   └── app/
│       ├── supervisor.c/.h    # Máquina de estados de seguridad
│       ├── app.c/.h           # Orquesta un ciclo de control
│       └── telemetry.c/.h     # Cola de muestras y envío por USB (core 1)
├── tests/
│   ├── CMakeLists.txt         # Build de pruebas para la PC
│   ├── unity/                 # Framework Unity (vendorizado)
│   ├── mocks/                 # Dobles de hal_*
│   └── test_*.c               # Un archivo por módulo probado
└── tools/
    ├── flash.sh               # Compila y carga con picotool
    └── telemetry_plot.py      # Lee el USB y grafica en vivo
```

## Arquitectura por capas y rol de cada archivo

Las dependencias solo bajan: `app` usa `control`, `sensors` y `actuators`; estos usan `hal`; `hal` usa el SDK. Ninguna capa inferior conoce a una superior, y `control` no depende de nada de hardware.

&#91;embedded content: arquitectura del firmware · 4 capas + common\]

Cada flecha es un `#include` permitido; `control/` queda aislado del hardware y por eso se prueba completo en la PC.

**Reglas de dependencia**

- `control/` es lógica pura: recibe números, devuelve números. Sin `#include "pico/..."`, sin `hal_*`, sin estado global.
- `sensors/` y `actuators/` hablan con el hardware solo a través de `hal/`.
- `app/` es la única capa que conoce a todas las demás y decide el orden de un ciclo.
- `common/` lo puede incluir cualquiera; no incluye a nadie.

| Archivo | Rol | Depende de | Nunca debe |
| --- | --- | --- | --- |
| `main.c` | Inicializa el SDK y los módulos, arranca core 1 y corre el lazo esperando el tick | `app`, `hal_time` | Contener lógica de control |
| `common/config.h` | Todas las constantes ajustables con unidades en el nombre (`LIM_MAX_DEG`, `PWM_FREQ_HZ`) | — | Tener valores repetidos en otro archivo |
| `common/redux_types.h` | Enums y structs compartidos: `direction_t`, `sys_state_t`, `fault_t`, `status_t` | — | Incluir headers del SDK |
| `common/util.h` | Funciones `static inline`: `clampf`, `mapf`, `slewf` | — | Tener estado |
| `hal/board.h` | Números de GPIO y canales ADC | — | Estar duplicado en otro lado |
| `hal/hal_time.*` | Tick de 1 ms por `repeating_timer`, `hal_time_us()` | SDK | Ejecutar lógica en la ISR |
| `hal/hal_adc.*` | Iniciar el ADC y leer un canal en cuentas de 12 bits | SDK | Filtrar la señal |
| `hal/hal_pwm.*` | Configurar frecuencia y fijar duty de 0.0 a 1.0 | SDK | Conocer al BTS7960 |
| `hal/hal_i2c.*` | Leer y escribir registros con timeout; devuelve `status_t` | SDK | Bloquear sin límite de tiempo |
| `hal/hal_gpio.*` | Pines de enable y LED | SDK | — |
| `hal/hal_wdt.*` | Habilitar y alimentar el watchdog | SDK | — |
| `sensors/emg.*` | Un canal: rectificado opcional, envolvente, umbral con histéresis, intensidad 0–1 | `hal_adc`, `config` | Decidir la dirección del motor |
| `sensors/emg_calib.*` | Calibración de reposo (media y desvío) y de contracción máxima | `emg`, `hal_time` | Mover el motor |
| `sensors/as5600.*` | Ángulo articular en grados y estado del imán (MD, ML, MH) | `hal_i2c`, `config` | Aplicar límites |
| `actuators/bts7960.*` | Aplicar dirección y duty garantizando que nunca haya PWM en ambos lados | `hal_pwm`, `hal_gpio` | Leer sensores |
| `control/intent.*` | Lógica XOR: de (bíceps activo, tríceps activo) a FLEX, EXT, REPOSO o COCONTRACCIÓN | `redux_types` | Tocar hardware |
| `control/power.*` | Duty objetivo desde la intensidad; rampas de subida y bajada; frenar antes de invertir | `config`, `util` | Tocar hardware |
| `control/limits.*` | Bloquear el sentido que supera el límite y atenuar en la zona de freno | `config`, `util` | Tocar hardware |
| `app/supervisor.*` | Máquina de estados: arranque, calibración, rearme, marcha, falla | `redux_types`, `config` | Escribir en el PWM |
| `app/app.*` | Un ciclo: leer sensores → supervisor → intent → limits → power → driver → telemetría | Todas las anteriores | Tener lógica que podría vivir en `control/` |
| `app/telemetry.*` | Cola sin bloqueo entre núcleos y envío CSV por USB desde core 1 | `pico/multicore` vía `hal` | Bloquear a core 0 |

## Contratos entre módulos

Cada header expone solo lo que la capa de arriba necesita; el estado interno vive en structs que el llamador pasa por puntero, sin variables globales ocultas. Las funciones de `control/` y las conversiones de los drivers son puras para poder probarlas en la PC.

**Tipos compartidos (`redux_types.h`)**

| Tipo | Valores | Uso |
| --- | --- | --- |
| `direction_t` | `DIR_STOP = 0`, `DIR_FLEX = 1`, `DIR_EXT = -1` | Sentido pedido o aplicado al motor |
| `intent_t` | `INTENT_REST`, `INTENT_FLEX`, `INTENT_EXT`, `INTENT_COCONTRACTION` | Resultado de la lógica XOR |
| `sys_state_t` | `ST_BOOT`, `ST_CHECK_ENC`, `ST_CALIB_REST`, `ST_CALIB_MVC`, `ST_ARMING`, `ST_RUN`, `ST_FAULT` | Estado del supervisor |
| `fault_t` | Máscara de bits: `FAULT_I2C`, `FAULT_MAGNET`, `FAULT_ANGLE_JUMP`, `FAULT_OVERRUN` | Causa de una falla; se pueden combinar |
| `status_t` | `STATUS_OK`, `STATUS_TIMEOUT`, `STATUS_NACK`, `STATUS_BAD_ARG` | Resultado de toda operación de hardware |

**Interfaces**

| Módulo | Función | Contrato |
| --- | --- | --- |
| `emg` | `emg_init(ch, adc_input)` | Deja el canal en reposo, sin umbrales válidos |
| `emg` | `emg_update(ch, raw)` → envolvente | Recibe la muestra cruda ya leída (no lee el ADC), así se prueba con datos sintéticos |
| `emg` | `emg_is_active(ch)`, `emg_intensity(ch)` | Activo con histéresis; intensidad acotada a 0.0–1.0 |
| `emg_calib` | `rest_begin`, `rest_add(sample)`, `rest_finish(ch)` | No bloqueante: se alimenta una muestra por tick; al terminar fija base, umbral de activación y de desactivación |
| `emg_calib` | `mvc_begin`, `mvc_add`, `mvc_finish(ch)` → `bool` | Devuelve `false` si la contracción fue demasiado débil y deja el rango por defecto |
| `as5600` | `as5600_read_raw(&raw)` → `status_t` | Lee 0x0C–0x0D; nunca bloquea más que el timeout de I2C |
| `as5600` | `as5600_read_magnet(&m)` → `status_t` | Decodifica MD, ML y MH del registro 0x0B |
| `as5600` | `as5600_raw_to_deg(raw)` | Pura: diferencia envuelta (sin overflow) respecto de la referencia calibrada, con sentido |
| `bts7960` | `bts7960_init()`, `bts7960_enable(bool)` | Arranca deshabilitado y con duty 0 |
| `bts7960` | `bts7960_apply(dir, duty)` | Pone a 0 el lado opuesto antes de escribir el activo; recorta el duty a `DUTY_MAX` |
| `bts7960` | `bts7960_stop()` | Ambos PWM a 0 en la misma llamada |
| `intent` | `intent_decide(bic_on, tri_on)` → `intent_t` | Tabla XOR completa; sin estado |
| `limits` | `limits_factor(dir, angle_deg)` → 0.0–1.0 | 0.0 si el sentido supera el límite; menor a 1.0 dentro de la zona de freno |
| `power` | `power_target(intensity, factor)` → duty | `DUTY_MIN + intensidad × (DUTY_MAX − DUTY_MIN) × factor` |
| `power` | `power_step(ps, dir_obj, duty_obj, hard_stop)` | Aplica rampas; si cambia el sentido, primero baja a 0; `hard_stop` corta al instante |
| `supervisor` | `supervisor_step(sv, &in, &out)` | Entradas: estado del encoder, fallas, actividad EMG, fin de calibración. Salidas: estado y `motor_allowed` |
| `app` | `app_init()`, `app_tick()` | Un ciclo completo por llamada; tiempo máximo medido en telemetría |
| `telemetry` | `telemetry_push(&sample)` → `bool` | Nunca bloquea: si la cola está llena descarta y cuenta la pérdida |

**Orden fijo de un ciclo en `app_tick()`**

1. Alimentar el watchdog.
2. Leer ADC de ambos canales y actualizar `emg`.
3. Leer el ángulo del AS5600 (y el estado del imán cada 50 ciclos); marcar fallas.
4. `supervisor_step` decide el estado y si el motor puede moverse.
5. En `ST_RUN`: `intent_decide` → `limits_factor` → `power_target` → `power_step`.
6. `bts7960_apply` con el resultado, o `bts7960_stop` si el supervisor no lo permite.
7. Armar la muestra y hacer `telemetry_push` cada 100 ciclos.

## Máquina de estados, lógica XOR y seguridad

El motor solo recibe PWM en `ST_RUN`; cualquier falla lleva a `ST_FAULT` con parada inmediata, y para volver a moverse el sistema exige pasar por `ST_ARMING` con ambos músculos relajados.

&#91;embedded content: máquina de estados del supervisor · 7 estados\]

Una falla del encoder en `ST_ARMING` o en `ST_RUN` lleva a `ST_FAULT`; desde ahí el único camino de vuelta al motor pasa por `ST_ARMING`.

Entre `ST_CHECK_ENC` y `ST_CALIB_REST` se agrega `ST_CALIB_ROM`: calibración del rango del paciente con el driver deshabilitado (ver Fase 3). Una calibración inválida no avanza.

**Tabla de verdad de `intent_decide`**

| Bíceps activo | Tríceps activo | Resultado XOR | Intención | Acción sobre el motor |
| --- | --- | --- | --- | --- |
| No | No | 0 | `INTENT_REST` | Frenado con rampa hasta 0 |
| Sí | No | 1 | `INTENT_FLEX` | RPWM con duty proporcional; LPWM en 0 |
| No | Sí | 1 | `INTENT_EXT` | LPWM con duty proporcional; RPWM en 0 |
| Sí | Sí | 0 | `INTENT_COCONTRACTION` | Corte inmediato (`hard_stop`) |

**Reglas de seguridad (invariantes que el código debe garantizar)**

1. RPWM y LPWM nunca tienen duty mayor a 0 al mismo tiempo. Lo asegura `bts7960_apply` y lo verifica una prueba unitaria.
2. Antes de invertir el sentido, el duty baja a 0 con rampa; nunca hay un salto directo de FLEX a EXT.
3. Si el ángulo alcanza un límite en el sentido de giro actual, el corte es inmediato, aunque el motor esté en rampa de frenado.
4. Fuera de rango, solo se permite el sentido que vuelve hacia adentro.
5. Falla de I2C, imán fuera de rango o salto de ángulo mayor a `ANGLE_JUMP_MAX_DEG` entre ciclos → `ST_FAULT` y motor parado.
6. Desde `ST_FAULT` se pasa a `ST_ARMING` recién tras `REARM_OK_TICKS` lecturas estables; a `ST_RUN`, solo con ambos EMG en reposo.
7. El watchdog (500 ms) se habilita después de la calibración y se alimenta una vez por ciclo; si el lazo se cuelga, la Pico se reinicia con el driver deshabilitado.
8. Los pines R\_EN y L\_EN llevan pull-down externo de 10 kΩ para que el driver quede apagado durante el reinicio.
9. Un ciclo que excede su presupuesto de 1 ms se cuenta; tres seguidos generan `FAULT_OVERRUN`.

Nota técnica: en el BTS7960, ambos PWM en alto frenan el motor (los dos bornes quedan en Vcc) en lugar de causar un cortocircuito, porque cada medio puente tiene protección propia. La regla 1 se mantiene igual porque la co-contracción es una intención ambigua y el exoesqueleto no debe ejercer fuerza.

## Plan paso a paso por fases

Son nueve fases; cada una termina con un criterio verificable y un commit, y ninguna fase mueve el motor con el brazo colocado hasta la fase 8. Las fases 2 a 5 se pueden repartir entre integrantes porque sus módulos no dependen entre sí.

### Fase 0 · Entorno y esqueleto

- **Archivos:** `CMakeLists.txt`, `pico_sdk_import.cmake`, `main.c`, `CLAUDE.md`, `.clang-format`, `.gitignore`, `README.md`, `docs/hardware.md`.
- **Pasos:**
  1. Instalar el Pico SDK 2.x, el toolchain `arm-none-eabi-gcc`, CMake, Ninja y picotool.
  2. Crear el repositorio con el árbol de carpetas completo y archivos vacíos con su comentario de rol.
  3. Configurar CMake con `PICO_BOARD=pico2_w`, flags de advertencia y salida por USB (`pico_enable_stdio_usb`).
  4. `main.c` mínimo: imprime la versión y hace parpadear el LED externo.
  5. Escribir `CLAUDE.md` (ver la sección de Claude Code) y `docs/hardware.md` con el pinout.
- **Terminado cuando:** `cmake --build` compila sin avisos, el UF2 se carga y el monitor serie muestra el mensaje.

### Fase 1 · Configuración y HAL

- **Archivos:** `common/*`, `hal/*`.
- **Pasos:**
  1. Volcar en `config.h` todos los parámetros del prototipo, con unidades en el nombre.
  2. Definir `redux_types.h` y `util.h`.
  3. Implementar cada `hal_*` con una función de inicio y funciones mínimas; `hal_i2c` con timeout obligatorio.
  4. `hal_time`: `repeating_timer` de 1 ms que solo levanta una bandera `volatile`.
- **Terminado cuando:** un programa de prueba imprime lecturas del ADC, mide 1000 ticks en 1 s ±1 % y cambia un duty de PWM visible con osciloscopio o LED.

### Fase 2 · Driver BTS7960

- **Archivos:** `actuators/bts7960.*`, `tests/test_bts7960.c`.
- **Pasos:**
  1. Iniciar ambos PWM a 20 kHz y los enable en bajo.
  2. Implementar `apply` con el orden “apagar opuesto, luego activar”.
  3. Prueba en PC con mock de `hal_pwm` que registra cada escritura y verifica que nunca haya dos duties positivos.
  4. Prueba en banco: motor sin carga, comandos por serie (`f 30`, `e 30`, `s`).
- **Terminado cuando:** la prueba unitaria pasa y el motor gira en ambos sentidos en banco.

### Fase 3 · Driver AS5600 y calibración del rango del paciente

- **Archivos:** `sensors/as5600.*`, `sensors/rom_calib.*`, `tests/test_as5600.c`, `tests/test_rom_calib.c`.
- **Convención (reemplaza a la del resto del plan):** 0° = flexión máxima del paciente; el ángulo crece al extender; `rom_deg` = extensión máxima. Los límites dejan de ser fijos (0°–130°) y pasan a ser `[ROM_MARGIN_DEG, rom_deg − ROM_MARGIN_DEG]`.
- **Causa del overflow a evitar:** la lectura cruda va de 0 a 4095 y vuelve a 0. Si ese salto cae dentro del recorrido del brazo, restar dos lecturas directamente (sobre todo en `uint16_t`) da saltos enormes. Nunca se restan lecturas crudas sin envolver.
- **Pasos:**
  1. Lectura de `RAW ANGLE` (0x0C–0x0D) y `STATUS` (0x0B) con `status_t`, siempre en `int32_t`.
  2. Función pura `as5600_wrap_diff(raw, ref)` → `((raw − ref + 2048) mod 4096) − 2048`, resultado de −2048 a +2047 cuentas (±180°). Funciona esté donde esté el cero del imán.
  3. Lectura filtrada para calibrar: promedio de 32 muestras y rechazo si la dispersión supera `ROM_STABLE_MAX_DEG`.
  4. `rom_calib` con el motor deshabilitado: el usuario o terapeuta lleva el brazo a flexión máxima y confirma (`raw_flex`), luego a extensión máxima y confirma (`raw_ext`).
  5. Calcular `sign` (para que la extensión sea positiva) y `rom_deg = |wrap_diff(raw_ext, raw_flex)| × 360/4096`.
  6. Validar: imán OK, `ROM_MIN_DEG ≤ rom_deg ≤ ROM_MAX_DEG` (por ejemplo 20° y 160°); si falla, repetir la calibración.
  7. Ángulo de trabajo: `θ = sign × wrap_diff(raw, raw_flex) × 360/4096`.
  8. Las confirmaciones y mensajes pasan por la interfaz \`app/ui.h\` (eventos CONFIRMAR, CANCELAR, RECALIBRAR y mensajes de estado), nunca por el pulsador directamente. Por ahora la implementa \`ui\_basic.c\` (pulsador GPIO 3, LED GPIO 2 y monitor serie); más adelante la caja con teclado y pantalla la reemplaza sin tocar la calibración ni el supervisor.
  9. Pruebas en PC: referencia cerca de 4095 con el recorrido cruzando el 0, sentido invertido, rango fuera de límites rechazado, lectura inestable rechazada.
- **Terminado cuando:** el ángulo no salta al cruzar el cero del imán, coincide con un goniómetro en ambos extremos con error menor a 1° y una calibración inválida se rechaza.
- **Estado (2026-10-03):** código y pruebas en PC OK; validación en hardware pendiente. El módulo AS5600 se dañó (pista de SDA levantada), así que la prueba en banco queda pendiente hasta conseguir otro.

### Fase 4 · Canal EMG y calibración

- **Archivos:** `sensors/emg.*`, `sensors/emg_calib.*`, `tests/test_emg.c`.
- **Pasos:**
  1. Envolvente exponencial, rectificado opcional según `EMG_RAW_SIGNAL`.
  2. Histéresis de activación y cálculo de intensidad.
  3. Calibración de reposo y de contracción máxima, no bloqueantes.
  4. Pruebas en PC con señales sintéticas: reposo con ruido, escalón, oscilación alrededor del umbral.
- **Terminado cuando:** con el sensor puesto, la telemetría muestra la envolvente y el estado activo cambia solo con contracciones reales.

### Fase 5 · Lógica de control pura

- **Archivos:** `control/intent.*`, `control/limits.*`, `control/power.*`, sus `test_*.c`.
- **Pasos:**
  1. `intent_decide` con la tabla XOR.
  2. `limits_factor` con bloqueo y zona de freno.
  3. `power_target` y `power_step` con rampas y frenado antes de invertir.
- **Terminado cuando:** todas las pruebas de la sección de pruebas pasan en la PC.

### Fase 6 · Supervisor e integración

- **Archivos:** `app/supervisor.*`, `app/app.*`, `main.c`, `hal_wdt.*`, `tests/test_supervisor.c`.
- **Pasos:**
  1. Implementar los estados y transiciones del diagrama de la máquina de estados.
  2. `app_tick` con el orden fijo de ciclo.
  3. Medir el tiempo de cada ciclo y detectar sobrepasos.
  4. Habilitar el watchdog después de la calibración.
- **Terminado cuando:** en banco, sin brazo, se reproducen a mano las fallas (desconectar el imán, el bus I2C, co-contracción) y el motor se detiene en cada caso.

### Fase 7 · Telemetría y persistencia

- **Archivos:** `app/telemetry.*`, `tools/telemetry_plot.py`, flash de calibración.
- **Pasos:**
  1. Cola circular de muestras entre núcleos, con contador de pérdidas.
  2. Core 1 envía líneas CSV: tiempo, ángulo, envolventes, estado, dirección, duty, fallas.
  3. Guardar la calibración en el último sector de flash, con número mágico y CRC.
- **Terminado cuando:** el script grafica en vivo y un reinicio reutiliza la calibración guardada.

### Fase 8 · Pruebas con el usuario y ajuste

- **Pasos:**
  1. Arrancar con `DUTY_MAX` bajo (0.3) y límites reducidos (10°–100°).
  2. Ajustar umbrales, rampas y zona de freno con la telemetría.
  3. Ampliar gradualmente hasta los valores finales.
- **Terminado cuando:** se cumple la lista de aceptación del final.

### Fase 9 · Futuro

- Wi-Fi con `pico_cyw43_arch` en core 1 y envío de telemetría a la API Flask.
- Control PID de posición sobre el error `θ_deseada − θ_actual` para trayectorias preprogramadas.

* Caja de calibración en la misma Pico: teclado matricial de membrana 4×4 y pantalla TFT ST7735 de 128×128 por SPI, con la interfaz diseñada en SquareLine Studio (LVGL) por otro integrante. Se integra como `ui_panel.c`, otra implementación de `app/ui.h`. LVGL corre solo en el core 1; el core 0 nunca llama funciones de LVGL y se comunica con la pantalla solo por colas de eventos y mensajes.

## Estrategia de pruebas

La lógica se prueba primero en la PC, donde un error no mueve nada; el hardware se prueba después en banco y sin brazo, y recién al final con el usuario.

| Nivel | Dónde corre | Qué cubre | Herramienta |
| --- | --- | --- | --- |
| Unitarias | PC (`tests/`, build con gcc nativo) | `intent`, `limits`, `power`, `emg`, `emg_calib`, `as5600_raw_to_deg`, `supervisor`, invariante de `bts7960` | Unity + mocks de `hal_*` |
| Integración en banco | Pico + driver + motor sin carga | Sentidos de giro, rampas, lectura del encoder, fallas provocadas | Monitor serie y `telemetry_plot.py` |
| Aceptación | Exoesqueleto con usuario | Umbrales, comodidad, límites reales | Lista de aceptación y registro de telemetría |

**Casos mínimos por módulo**

- `intent`: las cuatro combinaciones de la tabla XOR.
- `limits`: dentro del rango, en la zona de freno, en el límite, fuera de rango en ambos sentidos.
- `power`: rampa de subida, rampa de bajada, inversión de sentido (debe pasar por 0), `hard_stop`, duty nunca mayor a `DUTY_MAX`.
- `emg`: reposo con ruido no activa; escalón activa; oscilación en el umbral no genera parpadeo; intensidad acotada a 0–1.
- `as5600_raw_to_deg`: cero, envoltura en 4095→0, sentido invertido.
- `supervisor`: arranque completo, falla en `ST_RUN`, rearme que exige reposo, co-contracción en marcha.
- `bts7960`: secuencias aleatorias de `apply` nunca dejan dos duties positivos.

**Reglas**

- Cada módulo nuevo llega con su `test_*.c` en el mismo commit.
- Las pruebas en PC se corren antes de cada carga a la Pico (`ctest` en `build-tests/`).
- Una falla encontrada en hardware se reproduce primero con una prueba unitaria y después se corrige.

## Buenas prácticas y convenciones

Estas reglas van copiadas en `CLAUDE.md` para que Claude Code y cada integrante las apliquen igual.

**Nombres**

- Archivos y funciones en `snake_case`, con prefijo del módulo: `emg_update`, `bts7960_apply`.
- Tipos terminan en `_t`; constantes y macros en `MAYUSCULAS` con unidad al final: `LIM_MAX_DEG`, `PWM_FREQ_HZ`, `CTRL_PERIOD_US`.
- Variables con unidad cuando la tienen: `angle_deg`, `duty`, `elapsed_us`.

**Código**

- Un header por módulo con include guard, comentario de rol en la primera línea y documentación breve de cada función pública (qué hace, rangos, unidades).
- Todo lo interno al módulo es `static`.
- Nada de `malloc`, recursión ni `float` en ISR.
- Tipos de ancho fijo (`uint16_t`, `int32_t`) para registros y muestras.
- Toda operación de hardware devuelve `status_t` y el llamador la revisa.
- Ningún bucle de espera sin timeout.
- Ningún número mágico: todo valor ajustable vive en `config.h`.
- `const` en todo parámetro puntero que no se modifica.
- Funciones cortas (menos de 40 líneas como guía) y un solo nivel de abstracción por función.

**Estilo y herramientas**

- `.clang-format` basado en LLVM, 4 espacios, 100 columnas; se aplica antes de cada commit.
- Compilación con `-Wall -Wextra -Werror -Wshadow -Wconversion`.
- `cppcheck` como análisis estático en la fase 6.

**Git**

- Rama `main` siempre compila; cada fase en su rama `fase-N-nombre`.
- Commits pequeños en imperativo con prefijo: `feat(emg): agregar histéresis`, `fix(bts7960): apagar lado opuesto primero`, `test(limits): casos fuera de rango`.
- Un cambio de parámetro en `config.h` se explica en el mensaje del commit.
- Decisiones de diseño importantes van a `docs/decisions/ADR-00N-*.md`.

## Cómo trabajar con Claude Code

Se trabaja una fase por sesión: Claude Code lee `CLAUDE.md` y `docs/PLAN.md`, propone un plan en modo plan, implementa solo los archivos de esa fase con sus pruebas y termina con un commit.

**Flujo por fase**

1. Exportar este documento como Markdown a `docs/PLAN.md` y mantenerlo actualizado.
2. Abrir la sesión en la raíz del repositorio y pedir la fase con el prompt de abajo.
3. Revisar el plan que propone antes de aprobarlo; corregir si toca archivos de otra fase.
4. Pedir que compile el firmware y corra las pruebas en PC antes de dar la fase por terminada.
5. Probar en hardware con el criterio de terminado y recién entonces hacer merge a `main`.
6. Si durante la fase cambia una decisión, actualizar `docs/PLAN.md` o agregar un ADR en el mismo commit.

**Contenido de `CLAUDE.md`**

```markdown
# REDUX firmware

Exoesqueleto de codo. Raspberry Pi Pico 2 W (RP2350), C11, Pico SDK 2.x.
EMG bíceps/tríceps → lógica XOR → BTS7960. Encoder AS5600 por I2C para límites.

## Comandos
- Firmware: cmake -B build -G Ninja -DPICO_BOARD=pico2_w && cmake --build build
- Pruebas PC: cmake -B build-tests -S tests && cmake --build build-tests && ctest --test-dir build-tests
- Carga: tools/flash.sh

## Arquitectura (ver docs/PLAN.md)
- Capas: app → control / sensors / actuators → hal → SDK. Las dependencias solo bajan.
- Solo hal/ y main.c incluyen headers del SDK.
- control/ es lógica pura y se prueba en PC.

## Reglas de seguridad (no negociables)
- RPWM y LPWM nunca con duty > 0 a la vez.
- Frenar a 0 antes de invertir el sentido.
- Co-contracción, límite alcanzado o falla del encoder → corte inmediato.
- Rearme solo con ambos EMG en reposo.
- Toda espera de hardware tiene timeout.

## Convenciones
- snake_case con prefijo de módulo; tipos con _t; constantes con unidad (LIM_MAX_DEG).
- Sin malloc, sin números mágicos (todo en config.h), internos static.
- Cada módulo nuevo llega con tests/test_<modulo>.c.
- Formatear con clang-format; compilar sin avisos.

## Cómo trabajar
- Una fase por sesión; no tocar archivos de otras fases sin avisar.
- Antes de terminar: compilar firmware, correr pruebas, resumir cambios.
- Commits: feat(modulo): ..., fix(modulo): ..., test(modulo): ...
```

**Prompt modelo para iniciar una fase**

```text
Leé CLAUDE.md y la sección "Fase N" de docs/PLAN.md.
Implementá solo los archivos listados en esa fase, respetando los contratos
de la sección "Contratos entre módulos".
Antes de escribir código, mostrame el plan de archivos y funciones.
Incluí las pruebas unitarias de la fase, compilá el firmware y corré ctest.
Al final, resumí qué quedó hecho y qué hay que probar en hardware.
```

**Consejos**

- Pegar la salida del monitor serie cuando algo falla en hardware; es el dato más útil para depurar.
- No pedir “hacé todo el firmware” en una sola sesión: se pierde el control de los contratos.
- Cuando un parámetro se ajusta en banco, pedir el cambio en `config.h` y no en el módulo.

## Riesgos y aceptación

El riesgo principal es que el ruido del motor contamine la señal EMG; se mitiga separando masas y validando con telemetría antes de cualquier prueba con el usuario.

| Riesgo | Efecto | Mitigación |
| --- | --- | --- |
| Ruido del motor en el EMG | Activaciones falsas | Masas separadas en estrella, cables trenzados, alimentación aislada para los sensores, histéresis |
| Imán desalineado o fuera de distancia | Ángulo erróneo, límites inútiles | Soporte rígido a 0.5–3 mm, chequeo de MD/ML/MH, detección de saltos |
| Lazo que excede 1 ms | Control irregular | Medición por ciclo, telemetría en core 1, falla por sobrepaso |
| Inercia que pasa el límite | Sobreflexión | Zona de freno, corte inmediato en el límite, límites de software 5° antes del tope mecánico |
| Reinicio con driver habilitado | Movimiento inesperado | Pull-down en R\_EN y L\_EN, arranque con enable en bajo |
| Ruido del ADC del RP2350 | Umbrales inestables | Salida del sensor de baja impedancia, promedio de muestras, calibración por sesión |
| Parámetros dispersos en el código | Ajustes inconsistentes | Todo en `config.h`, revisión en cada commit |

**Lista de aceptación final**

- [ ] Compila sin avisos y todas las pruebas en PC pasan.
- [ ] Nunca hay PWM en ambos lados (prueba unitaria + osciloscopio).
- [ ] Flexión y extensión responden a bíceps y tríceps con potencia proporcional.
- [ ] Co-contracción detiene el motor en menos de 5 ms.
- [ ] El motor se detiene en los límites calibrados del paciente y solo permite volver hacia adentro.
- [ ] Desconectar el imán o el bus I2C detiene el motor y exige rearme.
- [ ] Tras un reinicio el sistema no se mueve hasta ver ambos músculos en reposo.
- [ ] El watchdog reinicia la Pico si el lazo se bloquea a propósito.
- [ ] La telemetría muestra ángulo, envolventes, estado y duty sin afectar el lazo.
- [ ] `docs/PLAN.md`, `docs/hardware.md` y `CLAUDE.md` reflejan el código final.
