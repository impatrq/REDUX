# REDUX · Plan de la línea UI (pantalla + teclado + LVGL)

Este plan es una **línea de trabajo paralela** al firmware principal (`redux-firmware`).
Se desarrolla en un proyecto aparte, el **laboratorio de UI** (`redux-ui-lab`), y al final se
entrega al firmware como un paquete de carpetas cuyas rutas ya coinciden con las del firmware.

**Situación de trabajo:** `redux-firmware` y su `docs/PLAN.md` están **en la computadora de un
compañero de equipo**, no en esta. El laboratorio se desarrolla de forma independiente y solo se
sincroniza con el firmware en momentos puntuales y acordados (sección 5). Por eso el diseño de
esta línea busca **depender lo menos posible** de los archivos del firmware.

Objetivo: tener la pantalla ST7735 y el teclado 4×4 funcionando con LVGL en la Pico 2 W,
con las pantallas diseñadas en SquareLine Studio, y conectadas al firmware **solo** a través
del contrato `app/ui.h` (eventos que entran, mensajes de estado que salen).

**Punto de partida:** ya existe un diseño hecho en SquareLine Studio (`Redux.spj`). Esta línea
**no rediseña las pantallas desde cero**: parte de ese export, lo corrige donde haga falta, lo hace
funcionar en la Pico y después lo conecta al contrato.

---

## 0. Contexto del proyecto (para quien arranca en este plan)

- **REDUX** es un exoesqueleto robótico de codo. Lee la intención de movimiento con dos
  sensores EMG (bíceps y tríceps) y mueve la articulación con un motor DC (12 V, reductora,
  30 RPM) mediante un puente H BTS7960. Un encoder magnético AS5600 mide el ángulo.
- **Microcontrolador:** Raspberry Pi Pico 2 W (RP2350), programado en **C** con el Pico SDK 2.x,
  en VS Code con la extensión Raspberry Pi Pico y Claude Code. Windows 11.
- **Firmware principal (`redux-firmware`):** lo desarrolla un compañero en **su** computadora.
  Está organizado en capas (`hal/`, `sensors/`, `actuators/`, `control/`, `app/`) y avanza por
  fases. Último estado conocido: fases 0–2 terminadas (estructura, HAL, driver del motor a 20 kHz),
  fase 3 (encoder y calibración del rango) en curso. **Este estado puede estar desactualizado**:
  se confirma con el compañero en cada punto de sincronización.
- **Lógica de control (resumen):** XOR entre bíceps y tríceps (uno solo activo = mover; ambos =
  bloqueo por co-contracción; ninguno = reposo), potencia proporcional a la intensidad EMG, límites
  angulares calibrados por paciente (0° = flexión máxima, el ángulo crece al extender).
- **Rol de la UI:** la caja con pantalla y teclado guía la calibración (flexión máxima, extensión
  máxima, EMG en reposo y contracción), muestra el estado y el ángulo, y permite rearmar tras una
  falla. **Nunca** controla el motor directamente.
- **Mientras la UI no esté integrada**, el firmware usa un pulsador (GP3) y un LED (GP2) a través
  de la misma interfaz `app/ui.h`. La pantalla reemplaza a esa implementación sin tocar el resto.

### 0.1 Qué hay en esta computadora

| Ubicación | Qué es | Uso en esta línea |
| --- | --- | --- |
| `C:\Users\ignac\SquareLine\Redux.spj` | Proyecto de SquareLine (fuente del diseño) | Se corrige y se vuelve a exportar acá |
| `C:\Users\ignac\OneDrive\Documentos\ReduxCode\ReduxUI\` | Export limpio de SquareLine (09/09/2026) | Fuente para `src/ui/squareline/` |
| `C:\Users\ignac\ReduxMain\` | Prototipo anterior | **No se reutiliza** (ver 0.2) |
| `redux-firmware` | — | **No está acá.** Lo tiene el compañero |

---

## 0.2 Revisión del export de SquareLine (hecha)

Resultado de la revisión. Se copia a `docs/ui_contract.md` en U0.

| Qué revisar | Encontrado | Estado |
| --- | --- | --- |
| Versión de SquareLine | 1.6.1 | — |
| Versión de LVGL | **8.3.11** | ✅ Es la que va en `lib/lvgl` |
| Resolución | 128 × 128 | ✅ |
| Profundidad de color | **32 bits**, swap 0 (`ui.c` tiene `#error` si no coincide) | ❌ Cambiar a **16 bits, swap 0** |
| Tipo de proyecto | "VS Code with SDL" | ✅ Indistinto, solo se usan los archivos de UI |
| Archivos exportados | `ui.c/.h`, `ui_helpers.*`, `ui_events.h`, `screens/` (3), `components/` (vacío), `images/` (1) | ✅ |
| Lógica en `ui_events.c` | No existe; los cambios de pantalla son eventos de SquareLine | ✅ Nada que mover |
| Fuentes | Solo la fuente por defecto (Montserrat 14) | ✅ |
| Imágenes | `Logotipo.png` 400×225 con alfa, con zoom al 29 % (~270 KB en 16 bits) | ⚠️ Achicar |
| Pantallas | `Inicio` (logo con fade → `Main`), `Main` (Ajustes / Comenzar / Perfil), `SettingScreen` (Volver) | Solo `Inicio` cubre una de las 5 de U5 |

**Correcciones a hacer en SquareLine (no en el código exportado):**
1. Project Settings → profundidad de color **16 bits**, swap **0**. El SPI se envía en tramas de
   16 bits, así que no hace falta intercambiar bytes.
2. Fondos `Fondo`, `Fondo1`, `Fondo2` miden 221×229 con desplazamiento: dejarlos en 128×128 en
   (0,0), o quitarlos y poner el color de fondo en la pantalla.
3. Logo: reemplazar el PNG por uno de ~120×68 y quitar el zoom (pasa de ~270 KB a ~16 KB de flash
   y LVGL deja de escalarlo en cada refresco).
4. Volver a exportar a `C:\Users\ignac\OneDrive\Documentos\ReduxCode\ReduxUI\` (la carpeta de
   export que ya tiene configurada el proyecto) y copiar de ahí al laboratorio.

**Por qué no se reutiliza `ReduxMain`:** trae LVGL **9.6.0** (el export es 8.3.11 y no compila con
la 9), tiene `ui.h` del export editado a mano, usa `ui_SelectionAnimator` que no existe en el
diseño actual y conecta el teclado a GP0–GP7, que no coincide con la tabla de pines.

---

## 1. Decisiones de esta línea

| Tema | Decisión | Motivo |
| --- | --- | --- |
| Dónde se trabaja | `C:\Users\ignac\redux-ui-lab`, creado con la extensión Raspberry Pi Pico (placa Pico 2 W). Fuera de OneDrive | No interfiere con el firmware; OneDrive no sincroniza `build/` |
| Independencia del firmware | El laboratorio **no necesita** el código del firmware para avanzar. Solo depende de la tabla de pines y del contrato `app/ui.h` | El firmware está en otra computadora |
| Contrato autocontenido | `app/ui.h` define **sus propios** tipos (`ui_mode_t`, `ui_prompt_t`, máscara de fallas de UI). No incluye `redux_types.h` | Se compila igual en las dos máquinas; el firmware traduce sus estados al contrato |
| Constantes de la UI | Van en `src/ui/ui_config.h` (archivo propio de la UI), **no** en el `config.h` del firmware | Al integrar no hay que fusionar `config.h` |
| Pines | `src/hal/board.h` del laboratorio es **provisorio**, armado con la tabla de la sección 2. Lo que manda es el `board.h` del compañero | No tenemos el original |
| Rutas | Las carpetas a entregar usan **las mismas rutas** que tendrán en `redux-firmware` | Integrar = copiar carpetas |
| Lenguaje y estilo | C11, snake_case con prefijo, tipos con `_t`, sin `malloc` propio, constantes con unidad, sin avisos en código propio | Las mismas reglas que el firmware |
| Librería gráfica | **LVGL 8.3.11**, igual que el export | Con otra versión el export no compila |
| Dónde corre LVGL | **Solo en el core 1**, también en el laboratorio | En el firmware el core 0 es del lazo de control |
| Comunicación con el firmware | Solo por colas (`pico/util/queue.h`) | Nada de variables compartidas ni llamadas cruzadas |
| Pantalla | ST7735 1,44" 128×128 RGB565 por **SPI1** con DMA | Bus propio, no comparte con el encoder (I2C0) |
| Teclado | Matricial 4×4 por barrido, dispositivo `KEYPAD` de LVGL | Navegación estándar de LVGL con grupos |
| Firmware falso | `src/lab/fake_core` simula al firmware (estados, ángulo, fallas) | Se prueba todo sin el firmware real |
| Intercambio con el compañero | Por **paquete** (zip o carpeta compartida) que mueve el usuario a mano. Sin remotos de git | Regla del proyecto: commits solo locales |

### Reglas que no se negocian
1. El core 0 nunca llama funciones de LVGL.
2. La UI nunca comanda el motor ni lee sensores: solo envía eventos y muestra mensajes.
3. Los archivos exportados por SquareLine **no se editan a mano**. La lógica propia va en `ui_panel.c`.
4. Toda espera de hardware (SPI, DMA) tiene timeout.
5. Ningún archivo del laboratorio inventa detalles del firmware más allá del contrato y la tabla
   de pines. Si hace falta algo del firmware, se anota como pregunta para el compañero (sección 5).

---

## 2. Pines (provisorios hasta confirmar con el compañero)

Tomados de la tabla "Pinout definitivo" del plan principal (versión conocida). Se confirman con el
`board.h` del compañero en el punto de sincronización S1.

| Señal | GPIO | Pin físico | Notas |
| --- | --- | --- | --- |
| ST7735 SCK | GP10 | 14 | SPI1 SCK |
| ST7735 MOSI (SDA en el módulo) | GP11 | 15 | SPI1 TX |
| ST7735 DC / A0 | GP12 | 16 | Salida |
| ST7735 CS | GP13 | 17 | Salida (manejo manual) |
| ST7735 RST / RES | GP14 | 19 | Salida |
| ST7735 BL / LED | GP15 | 20 | PWM para brillo (ver nota) |
| Teclado filas 1–4 | GP16, GP17, GP18, GP19 | 21, 22, 24, 25 | Salidas de barrido |
| Teclado columnas 1–3 | GP20, GP21, GP22 | 26, 27, 29 | Entradas con pull-up |
| Teclado columna 4 | GP0 | 1 | Entrada con pull-up |
| Pantalla VCC | 3V3 OUT | 36 | |
| Pantalla GND | GND | 18 | |

Pines que el firmware ya usa y la UI **no debe tocar**: GP2 (LED), GP3 (pulsador), I2C0 (encoder),
y los del puente H y los EMG (confirmar números en S1).

Nota de luz de fondo: un GPIO entrega pocos mA. Si el módulo no trae transistor para BL y la luz
consume más, conectar BL a 3V3 o usar un transistor.

---

## 3. Estructura del laboratorio

Marcas: ★ = se entrega al firmware tal cual · ◇ = provisorio, se reemplaza por el del compañero ·
sin marca = solo del laboratorio.

```text
redux-ui-lab/
├── CLAUDE.md                     # Reglas del laboratorio (sección 8)
├── CMakeLists.txt                # Build del laboratorio
├── docs/
│   ├── PLAN_UI.md                # Este archivo
│   ├── ui_contract.md            # ★ Contrato + resultado de la revisión 0.2 + asignación de teclas
│   ├── sync_log.md               # Registro de cada sincronización con el compañero
│   └── INTEGRACION.md            # ★ Pasos para que el compañero integre (se escribe en U7)
├── lib/
│   └── lvgl/                     # ★ LVGL 8.3.11 sin modificar
├── src/
│   ├── main.c                    # Arranca core 1 + fake_core
│   ├── hal/
│   │   ├── board.h               # ◇ Pines provisorios (sección 2)
│   │   ├── hal_spi.c/.h          # ★ SPI1 + DMA con timeout
│   │   └── hal_keypad.c/.h       # ★ Barrido 4×4 + antirrebote, no bloqueante
│   ├── ui/
│   │   ├── ui_config.h           # ★ Constantes de la UI (frecuencias, tamaños, timeouts)
│   │   ├── lv_conf.h             # ★ Configuración de LVGL
│   │   ├── st7735.c/.h           # ★ Driver de la pantalla
│   │   ├── lv_port_disp.c/.h     # ★ Puente LVGL ↔ st7735 (flush con DMA)
│   │   ├── lv_port_indev.c/.h    # ★ Puente LVGL ↔ teclado (KEYPAD)
│   │   ├── ui_panel.c/.h         # ★ Implementación de app/ui.h con pantalla + teclado
│   │   ├── ui_core1.c/.h         # ★ Bucle del core 1: lv_timer_handler + colas
│   │   └── squareline/           # ★ Export de SquareLine (no se edita a mano)
│   ├── app/
│   │   └── ui.h                  # ★ Contrato (autocontenido, sección 4)
│   └── lab/
│       └── fake_core.c/.h        # Simula al firmware
├── tools/
│   └── empaquetar.ps1            # Arma el zip de entrega con las carpetas ★
└── tests/
    └── test_keypad.c             # Pruebas en PC del barrido y antirrebote
```

No hay `common/config.h` ni `common/redux_types.h` en el laboratorio: la UI no los necesita.

---

## 4. Contrato UI ↔ firmware (`app/ui.h`)

Único punto de contacto. Es **autocontenido**: solo incluye `<stdint.h>`, `<stdbool.h>` y
`pico/util/queue.h`. El firmware traduce sus tipos (`sys_state_t`, `fault_t`) a los del contrato
en el supervisor; esa tabla de traducción la escribe el compañero y se anota en `ui_contract.md`.

Si en S1 el compañero ya tiene un `app/ui.h` propio (pulsador + LED), se comparan y se acuerda una
sola versión antes de U6. Mientras tanto, el laboratorio usa esta.

**Eventos (UI → firmware, por cola):**

| Evento | Origen | Uso |
| --- | --- | --- |
| `UI_EV_CONFIRM` | Tecla B en pantallas que esperan confirmación | Confirmar extremo de calibración, aceptar |
| `UI_EV_CANCEL` | Tecla D | Cancelar, volver |
| `UI_EV_RECALIBRATE` | Botón en pantalla (elegido con A/C, apretado con B) | Pedir nueva calibración del rango |

Navegación entre widgets (decidida el 02/11/2026): A = sube (`LV_KEY_PREV`),
C = baja (`LV_KEY_NEXT`), B = acepta (`LV_KEY_ENTER`), D = vuelve (`LV_KEY_ESC`).
Detalle en `docs/ui_contract.md` §3 y §6.

**Mensaje de estado (firmware → UI, por cola, struct de tamaño fijo `ui_status_t`):**

| Campo | Tipo | Contenido |
| --- | --- | --- |
| `mode` | `ui_mode_t` | `UI_MODE_BOOT`, `UI_MODE_CALIB_ROM`, `UI_MODE_CALIB_EMG`, `UI_MODE_RUN`, `UI_MODE_FAULT` |
| `prompt` | `ui_prompt_t` | `UI_PROMPT_NONE`, `_GO_FLEX`, `_GO_EXT`, `_RELAX`, `_CONTRACT_BICEPS`, `_CONTRACT_TRICEPS` |
| `angle_deg` | `float` | Ángulo actual (0° = flexión máxima) |
| `rom_deg` | `float` | Rango calibrado (0 si no hay calibración) |
| `faults` | `uint32_t` | Máscara `UI_FAULT_*` (encoder, sobrecorriente, límite, EMG, otra) |
| `duty` | `float` | Potencia aplicada, 0.0–1.0 (solo para mostrar) |

Frecuencia: el firmware publica a ~20 Hz. La UI siempre muestra el **último** mensaje (si la cola
se llena, se descartan los viejos).

**API que el firmware llama** (nombres propuestos, se acuerdan en S1):
`ui_init()`, `ui_publish_status(const ui_status_t *)`, `ui_poll_event(ui_event_t *) → bool`.

---

## 5. Sincronización con el compañero

Hay tres puntos de sincronización. En cada uno se anota en `docs/sync_log.md` la fecha, qué se
pidió, qué se recibió y qué cambió. El usuario mueve los archivos; Claude no configura remotos.

| Punto | Cuándo | Pedirle al compañero | Darle al compañero |
| --- | --- | --- | --- |
| **S1** | Antes de U1 (puede hacerse mientras se avanza U0) | `hal/board.h`, la tabla de pines vigente, si ya existe `app/ui.h` y cómo es, estado de fases | `docs/ui_contract.md` (contrato propuesto) |
| **S2** | Antes de U6 | Confirmación del contrato, tabla de traducción `sys_state_t → ui_mode_t` y `fault_t → UI_FAULT_*`, cómo usa hoy el core 1 (telemetría) | Versión final de `app/ui.h` |
| **S3** | U7 (cuando el firmware tenga el supervisor, fase 6) | Que confirme que puede integrar | Paquete de entrega (`tools/empaquetar.ps1`) + `INTEGRACION.md` |

Si al recibir `board.h` hay diferencias con la sección 2: **gana el del compañero**. Se reemplaza
el `board.h` del laboratorio, se actualiza la sección 2 de este plan y se anota en `sync_log.md`.

---

## 6. Fases de la línea UI

Cada fase termina con un criterio verificable y un commit local en el laboratorio.

### U0 · Proyecto de laboratorio
- Crear `C:\Users\ignac\redux-ui-lab` con la extensión Raspberry Pi Pico (C/C++, Pico 2 W, SDK
  2.x, salida por USB) y comprobar que el ejemplo compila y carga.
- `git init` y primer commit local.
- Crear la estructura de la sección 3. Escribir `hal/board.h` **provisorio** desde la sección 2,
  con un comentario arriba que diga que es provisorio hasta S1.
- Escribir `app/ui.h` según la sección 4 y `docs/ui_contract.md` con: el contrato, la revisión 0.2
  y la asignación de teclas propuesta. Crear `docs/sync_log.md` vacío con su formato.
- Copiar el export de SquareLine a `src/ui/squareline/` (si ya se hicieron las correcciones de 0.2,
  el nuevo; si no, el actual, y se reemplaza antes de U2). Todavía no se compila.
- Escribir `CLAUDE.md` (sección 8) y copiar este plan a `docs/PLAN_UI.md`.
- **Terminado cuando:** compila sin avisos, el monitor serie muestra un mensaje de arranque, y el
  contrato está listo para mandarle al compañero en S1.

### U1 · SPI y driver ST7735 sin LVGL
- Requiere S1 hecho, o al menos los pines de pantalla confirmados.
- `hal_spi`: SPI1 en GP10/GP11, CS/DC/RST como GPIO, envío por DMA con timeout, tramas de 16 bits
  para píxeles. Empezar a 15 MHz; probar hasta ~30 MHz.
- `st7735`: reset, secuencia de inicio, ventana de dibujo, envío de píxeles RGB565.
- Prueba: pantalla roja, verde y azul; marco de 1 px en el borde.
- Encontrar acá el **desplazamiento** de columnas/filas (típico 2 px en X y 1–3 px en Y), el
  **orden RGB/BGR** y la orientación; guardarlos en `ui_config.h`.
- **Terminado cuando:** los tres colores son correctos y el marco se ve completo en los cuatro bordes.

### U2 · LVGL sobre la pantalla
- Antes de empezar: el export corregido (16 bits, swap 0) tiene que estar en `src/ui/squareline/`.
- Agregar **LVGL 8.3.11** en `lib/lvgl` (tag `v8.3.11` del repositorio oficial) y compilarlo con
  CMake; `lv_conf.h` con `LV_COLOR_DEPTH 16`, `LV_COLOR_16_SWAP 0`, memoria ~48 KB, sin SO.
  `LV_CONF_INCLUDE_SIMPLE` se define en CMake, no en el export.
- `lv_port_disp`: buffer parcial de 128 × 20 líneas (doble buffer), flush por DMA que avisa a LVGL
  al terminar.
- Tick de 1 ms y `lv_timer_handler()` cada ~5 ms, **en el core 1** (`ui_core1`).
- Prueba 1: etiqueta "REDUX" y un número que se actualiza.
- Prueba 2: compilar `src/ui/squareline/` y llamar a `ui_init()`.
- **Terminado cuando:** ambas pruebas se ven nítidas y con colores correctos, y el tiempo de un
  refresco completo se mide e imprime por USB.

### U3 · Teclado 4×4
- `hal_keypad`: filas como salidas, columnas con pull-up; barrido cada 10 ms, antirrebote, sin
  bloquear; eventos de presionar/soltar.
- Pruebas en PC del antirrebote y de dos teclas a la vez.
- Prueba: imprimir por USB cada tecla una sola vez por pulsación.
- **Terminado cuando:** las 16 teclas se identifican bien, sin repeticiones ni teclas fantasma.

### U4 · Teclado como entrada de LVGL
- `lv_port_indev`: dispositivo `KEYPAD` con un `lv_group` por pantalla; mapeo de teclas según
  `docs/ui_contract.md` §6: A = `LV_KEY_PREV`, C = `LV_KEY_NEXT`, B = `LV_KEY_ENTER`,
  D = `LV_KEY_ESC`; el resto sin uso.
- Prueba: el menú `Main` del export (Ajustes / Comenzar / Perfil) navegado solo con el teclado.
- **Terminado cuando:** se mueve el foco y se activan los botones solo con el teclado.

### U5 · Adaptar las pantallas de SquareLine al flujo de REDUX
- Comparar las pantallas existentes con la lista mínima y agregar en SquareLine solo las que
  falten. Decidir qué pasa con `Main` / `SettingScreen` / "Perfil" (mantener como menú, o quitar
  si no tienen función en la v1). Volver a exportar.
- Los eventos de SquareLine solo cambian de pantalla o llaman funciones de `ui_panel`.
- Pantallas mínimas de la v1:
  1. Inicio / verificando encoder (existe: `Inicio`).
  2. Calibración: "Llevá el brazo a flexión máxima y apretá B", luego extensión, luego resultado
     (rango en grados) o error.
  3. Calibrando EMG: "Relajá el brazo" y "Contraé el bíceps/tríceps".
  4. En marcha: ángulo actual, barra de rango, estado.
  5. Falla: texto de la falla y "B para rearmar".
- Todos los widgets interactivos van al grupo del teclado. Cuidar el tamaño de texto a 128 px.
- **Terminado cuando:** las pantallas se ven correctas en el hardware y se navega entre ellas.
- **Rediseño (03/11/2026):** nuevas pantallas Begin (calibración del rango en cada sesión),
  Perfil (ID numérico), Ending (fin de sesión) y Wifi; sin Rango, EMG ni Recalibrar.
  Flujo vigente en `docs/ui_contract.md` §12.

### U6 · Contrato y firmware falso
- Requiere S2 hecho (contrato confirmado por el compañero).
- `ui_panel`: recibe `ui_status_t` y elige/actualiza la pantalla; traduce teclas a `UI_EV_*`.
- `fake_core` (core 0): recorre calibración, marcha con ángulo que sube y baja, y falla; responde a
  los eventos como lo haría el supervisor, usando **solo** los tipos del contrato.
- **Terminado cuando:** se recorre todo el flujo de la v1 con el teclado, el ángulo se actualiza a
  ~20 Hz y ningún código de LVGL corre en el core 0.

### U7 · Entrega al firmware
La integración la hace **el compañero en su computadora** (o los dos juntos), cuando el firmware
tenga el supervisor (fase 6). El laboratorio entrega:
1. `tools/empaquetar.ps1`: genera `redux-ui-entrega-<fecha>.zip` con las carpetas ★ y sus rutas.
2. `docs/INTEGRACION.md`, con los pasos para el compañero:
   - copiar las carpetas del zip a las mismas rutas de `redux-firmware`;
   - agregar al `CMakeLists.txt`: LVGL, `src/ui/`, `hal_spi`, `hal_keypad`, la definición
     `LV_CONF_INCLUDE_SIMPLE` y las librerías `hardware_spi`, `hardware_dma`, `pico_multicore`;
   - unir el bucle del core 1: `ui_core1` y la telemetría comparten el core 1 sin bloquearse;
   - en el supervisor, traducir `sys_state_t`/`fault_t` a `ui_status_t`, publicarlo a ~20 Hz y
     consumir los eventos;
   - elegir en `config.h` la implementación de `ui.h`: `ui_basic` (pulsador + LED) o `ui_panel`;
   - verificar: calibración y marcha con pantalla y teclado, lazo de control en 1 ms, pulsador
     como respaldo.
3. Registrar la entrega en `sync_log.md`.
- **Terminado cuando:** el compañero confirma que el firmware completo funciona con la pantalla y
  el teclado y que el lazo sigue cumpliendo 1 ms. Los problemas que aparezcan se corrigen en el
  laboratorio y se manda un paquete nuevo.

---

## 7. Riesgos de esta línea

| Riesgo | Efecto | Mitigación |
| --- | --- | --- |
| Versión de LVGL distinta a la de SquareLine | El export no compila | LVGL 8.3.11 fija; no reutilizar la 9.6 de `ReduxMain` |
| Pines provisorios distintos de los reales | Fallas al integrar o choque con el firmware | S1 antes de U1; gana el `board.h` del compañero |
| El contrato cambia en el firmware sin aviso | `ui_panel` deja de calzar | Contrato autocontenido; S2 antes de U6; cambios solo vía `ui_contract.md` |
| Información del firmware desactualizada | Decisiones sobre supuestos viejos | Confirmar estado en cada sincronización; registrar en `sync_log.md` |
| Desplazamiento/orden de color del ST7735 | Imagen corrida o colores cambiados | Resolverlo en U1, antes de LVGL |
| Dibujar demora el control | Lazo de 1 ms irregular | LVGL solo en core 1; flush por DMA |
| Uso del core 1 por la telemetría del firmware | Conflicto al integrar | Preguntarlo en S2; `ui_core1` no bloqueante |
| Luz de fondo consume más de lo que da el GPIO | Pin forzado, brillo bajo | Medir; transistor o BL a 3V3 |
| Editar a mano el export de SquareLine | Se pierde en el próximo export | Lógica solo en `ui_panel.c` |

---

## 8. `CLAUDE.md` del laboratorio

```markdown
# REDUX UI lab

Laboratorio de la pantalla ST7735 (128x128, SPI1) y el teclado 4x4 con LVGL 8.3.11
para el exoesqueleto REDUX. Raspberry Pi Pico 2 W (RP2350), C11, Pico SDK 2.x.
Plan: docs/PLAN_UI.md. Contrato con el firmware: docs/ui_contract.md.

El firmware (redux-firmware) NO está en esta computadora: lo tiene un compañero.
No asumir nada de su código más allá de docs/ui_contract.md y la tabla de pines.

## Reglas
- LVGL corre solo en el core 1. El core 0 nunca llama funciones de LVGL.
- La UI no toca motor ni sensores: solo envía eventos UI_EV_* y muestra mensajes.
- src/app/ui.h es autocontenido: no incluye archivos del firmware.
- src/hal/board.h es provisorio; si llega el del compañero, ese manda. No cambiar pines por cuenta propia.
- Constantes de la UI en src/ui/ui_config.h, no en un config.h del firmware.
- No editar src/ui/squareline/ a mano; la lógica va en src/ui/ui_panel.c.
- Toda espera de hardware (SPI, DMA) tiene timeout.
- Las rutas de src/ deben coincidir con las de redux-firmware.
- Lo que haga falta saber del firmware se anota como pregunta en docs/sync_log.md.

## Convenciones (iguales al firmware)
- snake_case con prefijo de módulo; tipos con _t; constantes con unidad.
- Sin números mágicos, internos static, compilar sin avisos en código propio.
- Una fase U* por sesión; mostrar el plan antes de escribir código.

## Git
- Commits solo en local. Nunca hacer git push ni configurar remotos.
```

---

## 9. Primer mensaje en un chat o sesión nueva

```text
Te paso PLAN_UI.md: es el plan de la línea UI del exoesqueleto REDUX
(pantalla ST7735 + teclado 4x4 + LVGL 8.3.11 en una Raspberry Pi Pico 2 W, en C).
Leé las secciones 0 a 0.2 y la 5. El firmware lo tiene mi compañero en otra
computadora; el laboratorio avanza solo con el contrato y la tabla de pines.
Empecemos por la fase U0.
```

## 10. Prompt para iniciar cada fase

```text
Leé CLAUDE.md y la sección "U<N>" de docs/PLAN_UI.md.
Revisá en docs/sync_log.md si la fase requiere una sincronización pendiente.
Implementá solo lo de esa fase, respetando docs/ui_contract.md y las rutas de la sección 3.
Antes de escribir código, mostrame el plan.
Al final compilá y resumí qué tengo que verificar en el hardware.
```
