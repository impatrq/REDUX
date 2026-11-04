# REDUX · Contrato UI ↔ firmware

Estado: **propuesta v1**, pendiente de confirmar con el compañero (S1 y S2, ver `PLAN_UI.md` §5).
Archivo de código: `src/app/ui.h`. Si este documento y el `.h` difieren, se corrige uno de los
dos en el mismo commit.

---

## 1. Principios

- El contrato es **autocontenido**: `app/ui.h` solo incluye `<stdint.h>` y `<stdbool.h>`.
  No depende de `redux_types.h` ni de `config.h` del firmware.
- El firmware **traduce** sus tipos internos a los del contrato en el supervisor (tabla §5).
- Comunicación solo por colas: estado firmware → UI, eventos UI → firmware.
- LVGL corre solo en el core 1. Las tres funciones del contrato se llaman desde el core 0.
- Se incluye siempre como `"app/ui.h"`. El export de SquareLine trae su propio `ui.h` y su propia
  `ui_init()`; por eso las funciones del contrato llevan prefijo `ui_app_`.

## 2. API

| Función | Quién la llama | Comportamiento |
| --- | --- | --- |
| `void ui_app_init(void)` | Firmware, al arrancar (core 0) | Crea las colas; en `ui_panel` arranca el core 1 |
| `void ui_app_publish_status(const ui_status_t *s)` | Supervisor, ~20 Hz | No bloquea; si la cola está llena descarta el más viejo |
| `bool ui_app_poll_event(ui_event_t *ev)` | Supervisor, en cada ciclo | No bloquea; `false` si no hay eventos |

## 3. Eventos (UI → firmware)

| Evento | Origen | Uso |
| --- | --- | --- |
| `UI_EV_CONFIRM` | Botones "Comenzar" (menú), "Guardar" (calibración) y "Reintentar" (falla), apretados con B | Confirmar extremo de calibración, rearmar tras falla; en `UI_MODE_IDLE`, iniciar la sesión |
| `UI_EV_CANCEL` | Botón "Terminar" (marcha) o tecla D durante la calibración | Terminar la sesión, cancelar la calibración |
| `UI_EV_RECALIBRATE` | Ninguno desde el rediseño del 03/11/2026 (la calibración va dentro de cada sesión) | Pedir nueva calibración del rango; se mantiene en el enum |

Los eventos los genera `ui_panel` según la pantalla, no la tecla cruda: en un menú, B aprieta
el botón con foco; donde el firmware espera una confirmación, B envía `UI_EV_CONFIRM`.

## 4. Mensaje de estado (firmware → UI): `ui_status_t`

| Campo | Tipo | Contenido |
| --- | --- | --- |
| `mode` | `ui_mode_t` | `UI_MODE_BOOT`, `UI_MODE_CALIB_ROM`, `UI_MODE_CALIB_EMG`, `UI_MODE_RUN`, `UI_MODE_FAULT`, `UI_MODE_IDLE` (agregado en U5, al final: no cambia los valores anteriores) |
| `prompt` | `ui_prompt_t` | `UI_PROMPT_NONE`, `_GO_FLEX`, `_GO_EXT`, `_RELAX`, `_CONTRACT_BICEPS`, `_CONTRACT_TRICEPS` |
| `angle_deg` | `float` | Ángulo actual (0° = flexión máxima) |
| `rom_deg` | `float` | Rango calibrado (0 si no hay calibración) |
| `faults` | `uint32_t` | Máscara: `UI_FAULT_ENCODER`, `_OVERCURRENT`, `_ANGLE_LIMIT`, `_EMG`, `_OTHER` |
| `duty` | `float` | Potencia aplicada 0.0–1.0 (solo para mostrar) |

Tamaño actual: 24 bytes. Frecuencia: ~20 Hz. La UI muestra siempre el último mensaje.

## 5. Traducción firmware → contrato (la completa el compañero en S2)

| `sys_state_t` del firmware | `ui_mode_t` | `ui_prompt_t` |
| --- | --- | --- |
| _pendiente_ | | |

| `fault_t` del firmware | Bit `UI_FAULT_*` |
| --- | --- |
| _pendiente_ | |

## 6. Asignación de teclas (decidida el 02/11/2026; se implementa en U4)

```text
 1   2   3   A        A = sube     (LV_KEY_PREV)
 4   5   6   B        B = acepta   (LV_KEY_ENTER) · UI_EV_CONFIRM donde se espera confirmación
 7   8   9   C        C = baja     (LV_KEY_NEXT)
 *   0   #   D        D = vuelve   (LV_KEY_ESC)   · UI_EV_CANCEL
```

Números y `*` (desde el 03/11/2026): solo en Perfil, para escribir el ID de usuario. Los dígitos
`0`–`9` llegan a LVGL como su carácter y `*` como `LV_KEY_BACKSPACE` (borra el último).
En las demás pantallas no hacen nada.

`#` (desde el 03/11/2026): gira toda la interfaz 90° a la izquierda en cualquier pantalla
(0° → 90° → 180° → 270° → 0°). No llega a LVGL: `lv_port_indev` lo entrega a `ui_panel`, que
llama a `lv_port_disp_rotate_left()`. La rotación es por software de LVGL (`sw_rotate`): el
panel, `MADCTL` y los desplazamientos de §8 no cambian, y las teclas tampoco (A sigue siendo
"sube"). No se guarda: cada arranque empieza en 0°. Con 90° y 270°, LVGL rota cada bloque en un
buffer temporal de `LV_DISP_ROT_MAX_BUF` (5 KB, del heap de LVGL) y no solapa dibujo y envío;
`lab_screens_test` imprime al arrancar el refresco completo a 0° y a 90°.

---

## 7. Revisión del export de SquareLine

Revisado el 05/10/2026 sobre `C:\Users\ignac\OneDrive\Documentos\ReduxCode\ReduxUI\`
(exportado el 09/09/2026 desde `C:\Users\ignac\SquareLine\Redux.spj`).
Actualizado el 02/11/2026 con el re-export de ese día (proyecto v6): correcciones 1 y 2 hechas.
Actualizado el 02/11/2026 (proyecto v12, export 15:53) con las pantallas de U5. Desde este export,
la carpeta de export es `C:\Users\ignac\OneDrive\Documentos\Squareline\`.

| Qué | Valor | Estado |
| --- | --- | --- |
| SquareLine Studio | 1.6.1 | — |
| LVGL | **8.3.11** | ✅ Versión fija de `lib/lvgl` |
| Resolución | 128 × 128 | ✅ |
| Profundidad de color | **16 bits**, swap 0 | ✅ Corregido el 02/11/2026 |
| Plantilla | VS Code with SDL | ✅ Indistinto |
| Archivos | `ui.c/.h`, `ui_helpers.*`, `ui_events.h`, `screens/` (7), `components/` (vacío), `images/` (1) | ✅ No copiar `images/ui_img_logotipo_png.c`: es la imagen vieja (1,3 MB) y no está en `filelist.txt` |
| `ui_events.c` con lógica | No existe | ✅ |
| Fuentes | Solo la de defecto (Montserrat 14) | ✅ |
| Imágenes | `Logotipo_nuevo.png` 120×68 ARGB, sin zoom (~24 KB) | ✅ Corregido el 02/11/2026 (antes: 400×225 con zoom, ~270 KB) |
| Fondos | `Fondo` … `Fondo6` de 128×128, centrados | ✅ |
| Pantallas | `Inicio`, `Main` (Ajustes/Comenzar/Perfil), `SettingScreen` (Recalibrar/Volver), `ScreenRango`, `ScreenEMG`, `ScreenMarcha`, `ScreenFalla` | ✅ Cubren las 5 de U5 |
| Eventos de SquareLine | `Inicio`: solo fade del logo. `Main` → Ajustes y `SettingScreen` → Volver cambian de pantalla | ✅ `Inicio` ya no pasa sola a `Main` |
| Etiquetas con texto variable | 120 px de ancho, wrap (modo por defecto de LVGL) | ✅ |

Widgets que usa `ui_panel` (U5):

| Pantalla | Widgets |
| --- | --- |
| `Inicio` | `ui_logo` |
| `SettingScreen` | `ui_RecalButton`, `ui_BackButton` |
| `ScreenRango` | `ui_Lblfuncion`, `ui_Lblrango` |
| `ScreenEMG` | `ui_LblInstruccion` |
| `ScreenMarcha` | `ui_Lblangulo`, `ui_BarRango`, `ui_LblEstado` |
| `ScreenFalla` | `ui_LblFalla`, `ui_LblRearm` |

Pendiente de diseño (no bloquea): `RecalText` dice "Reclaibrar"; etiquetas sin centrar el texto.

Correcciones de SquareLine terminadas. En cada re-export, reemplazar
`src/ui/squareline/` completo y actualizar esta tabla.

---

## 8. Ajustes del panel ST7735 (verificados en U1)

Verificado el 02/11/2026 en el hardware (módulo 1,44" 128×128, tipo "green tab"), con el patrón
de prueba de U1: colores correctos, marco de 1 px completo en los cuatro bordes, origen (0,0)
arriba a la izquierda. Valores en `src/ui/ui_config.h`:

| Ajuste | Valor |
| --- | --- |
| SPI1 | 15 MHz reales; relleno de pantalla completa ≈ 19,1 ms |
| Desplazamiento columna / fila | 2 / 3 px |
| `MIRROR_X` / `MIRROR_Y` / `SWAP_XY` | 1 / 1 / 0 |
| Orden de color | BGR |
| Inversión | No |
| Formato de píxel | RGB565, enviado MSB primero (SPI en modo 16 bits) |

Si se cambia el módulo de pantalla, repetir la prueba de U1 antes de seguir.

---

## 9. LVGL sobre la pantalla (U2)

`lib/lvgl` es el tag `v8.3.11` del repositorio oficial de LVGL, completo y sin modificar (sin
`.git`). Configuración en `src/ui/lv_conf.h` (copia de la plantilla; cambios marcados `REDUX:`).

| Ajuste | Valor |
| --- | --- |
| Color | 16 bits, `LV_COLOR_16_SWAP 0` (el SPI envía tramas de 16 bits, MSB primero) |
| Memoria de LVGL | 48 KB (`LV_MEM_SIZE`) |
| Tick | `LV_TICK_CUSTOM` con `to_ms_since_boot()` del SDK (1 ms, sin interrupción) |
| Buffers | 2 × 128 × 20 líneas (5 KB c/u), flush por DMA, cierre con timeout en `wait_cb` |
| `lv_timer_handler()` | Cada 5 ms, en el core 1 (`ui_core1`, pila propia de 8 KB) |
| Refresco completo medido | **21,9 ms** con el export (prueba 2), 02/11/2026; 0 errores de flush, LVGL usa 17 % (41 KB libres) |

---

## 10. Teclado 4×4 (U3)

Módulos: `src/hal/hal_keypad.c` (GPIO y barrido) y `src/hal/hal_keypad_logic.c` (antirrebote,
teclas fantasma y cola; sin SDK, probado en PC con `tests/test_keypad.c`). Valores en
`src/ui/ui_config.h`:

| Ajuste | Valor |
| --- | --- |
| Filas (GP16–GP19) | Entradas sin pull en reposo; la fila leída pasa a salida en bajo |
| Columnas (GP20–GP22, GP0) | Entradas con pull-up; bajo = presionada |
| Barrido | Cada 10 ms, en el core 1 (timer de LVGL); ~40 µs por barrido (4 × 10 µs de asentamiento) |
| Antirrebote | 3 barridos iguales (30 ms) para aceptar un cambio |
| Teclas fantasma | Si dos filas comparten dos columnas (3 teclas en L), el barrido se descarta |
| Cola de eventos | 8 eventos presionar/soltar; si se llena, se cuentan los perdidos |
| Disposición | `1 2 3 A / 4 5 6 B / 7 8 9 C / * 0 # D`, fila 1 arriba y columna 1 a la izquierda (verificada en el hardware el 02/11/2026: 16 teclas correctas) |

Los teclados de membrana comunes no traen diodos: dos teclas a la vez se leen bien, pero tres en L
no se distinguen de cuatro en rectángulo y se ignoran.

---

## 11. Teclado como entrada de LVGL (U4)

Módulo: `src/ui/lv_port_indev.c` (dispositivo `LV_INDEV_TYPE_KEYPAD`). Prueba: `src/lab/lab_indev_test.c`.
Estado: **verificado en el hardware el 02/11/2026** (navegación, aceptar y volver solo con el teclado).

| Ajuste | Valor |
| --- | --- |
| Teclas | Las de §6, en `ui_config.h` por carácter (`UI_KEY_UP_CHAR`, `_ENTER_`, `_DOWN_`, `_BACK_`, `_ERASE_`); dígitos tal cual; `UI_KEY_ROTATE_CHAR` va al callback de `lv_port_indev_set_key_cb()` |
| Barrido | Timer de LVGL cada `UI_KEYPAD_SCAN_PERIOD_MS` (10 ms), en el core 1 |
| Lectura de LVGL | Cada 10 ms (en vez de 30 ms); vacía la cola en una sola pasada |
| Una tecla activa | Mientras una tecla con uso está apretada, las demás se ignoran; LVGL siempre recibe el soltar de la misma tecla |
| Grupos | Uno por pantalla; `lv_port_indev_set_group()` al cambiar de pantalla |
| Mantener apretado | Comportamiento de LVGL: tras 400 ms, A/C repiten el movimiento de foco y D repite `LV_EVENT_KEY` cada 100 ms. En U6, `UI_EV_CANCEL` sale solo del primer evento |

`lv_port_indev_init()` lo llama la función de arranque de la pantalla (hoy la prueba; en U5/U6
`ui_panel`), no `ui_core1`: así la prueba de U3, que lee el teclado directo, sigue disponible.

---

## 12. Pantallas y flujo (rediseño del 03/11/2026)

Módulo: `src/ui/ui_panel.c`. Prueba: `src/lab/lab_screens_test.c` (firmware falso mínimo, sin colas).
Estado: **verificado en el hardware el 03/11/2026** (flujo completo con el teclado); falta
confirmar el borrado con `*` en Perfil. Reemplaza el flujo de U5 (02/11/2026), que
tenía `ScreenRango`, `ScreenEMG` y "Recalibrar" en Ajustes.

```text
BOOT (Inicio, fade del logo) --fin del arranque--> IDLE (Main)
IDLE: Main > Perfil / Ajustes (> Wifi): la UI navega sola, con las animaciones del export
IDLE: "Comenzar" (UI_EV_CONFIRM) --> CALIB_ROM: GO_FLEX --Guardar--> GO_EXT --Guardar--> RUN
RUN: "Terminar" --> la UI pasa a ScreenEnding y envía UI_EV_CANCEL; el firmware vuelve a IDLE
cualquier modo --falla--> FAULT: "Reintentar" (UI_EV_CONFIRM) --> IDLE (Main)
```

| `mode` | Pantalla | Qué muestra | B | D |
| --- | --- | --- | --- | --- |
| `BOOT` | `Inicio` | Logo | — | — |
| `IDLE` | `Main`, `SettingScreen`, `WifiScreen`, `ScreenPerfil`, `ScreenEnding` | Menú y fin de sesión (la UI navega sola entre ellas) | Botón con foco | Volver, donde lo hay |
| `CALIB_ROM` | `ScreenBegin` | `LblInstruccion`: "Flexiona lo mas posible" / "Extiende lo mas posible" | Guardar = `CONFIRM` | `CANCEL` |
| `CALIB_EMG` | `ScreenBegin` | `LblInstruccion`: relaja el brazo / contrae el bíceps / el tríceps | Guardar = `CONFIRM` | `CANCEL` |
| `RUN` | `ScreenMarcha` | `Lblangulo`: "N grados"; `LblEstado`: "Potencia P%"; `BarRango`: P de 0 a 100 | Terminar | Terminar |
| `FAULT` | `ScreenFalla` | Falla de mayor prioridad: sobrecorriente, encoder, fuera de rango, EMG, otra | Reintentar = `CONFIRM` | — |

- `ScreenEnding`: `LblEnvio` muestra "Enviando datos..." durante `UI_SEND_MSG_MS` (5 s) y luego
  "Enviados!". Por ahora no se envía nada: es solo el texto.
- `ScreenPerfil`: el campo de ID (`TextArea1`) acepta solo `0`–`9`, sin límite de largo; `*` borra.
  Arranca con el foco; A/C pasan a Volver. El ID todavía no sale de la UI.
- La pantalla cambia solo cuando cambia `mode`; en `IDLE` se queda en la pantalla del menú en
  la que esté. Los textos se reescriben solo si cambian. `P` sale de `duty` redondeado, y el
  texto y la barra usan el mismo número.
- Un grupo de teclado por pantalla (en `user_data` de la pantalla); se conecta al cargarla.
- El selector del diseño (`*Select*`, `MainSelec`) sigue al botón con foco; el borde de foco del
  tema está apagado en los botones. En Perfil, con el foco en el campo, el selector se oculta.
- Cada `SCREEN_LOADED` del export arranca una animación infinita del selector; `ui_panel` las
  borra al empezar a descargar la pantalla, para que no se acumulen.
- Mantener una tecla apretada genera un solo evento (contador de pulsaciones de `lv_port_indev`).

---

## 13. Wi-Fi (04/11/2026)

Módulos: `src/hal/hal_wifi.c` (core 0, CYW43 con `pico_cyw43_arch_lwip_threadsafe_background`)
y la pantalla `WifiScreen` en `src/ui/ui_panel.c` (core 1). Configuración de lwIP: `src/lwipopts.h`.
Estado: **compila; falta probar en el hardware**. No forma parte de `app/ui.h`: quién maneja
el Wi-Fi en el firmware está en `sync_log.md`.

```text
core 1 (UI)  --pedidos: ESCANEAR, CONECTAR(ssid)-->  core 0 (hal_wifi_poll cada 10 ms)
core 1 (UI)  <--reportes: estado + lista de redes--  core 0
```

| Qué | Cómo |
| --- | --- |
| Arranque | `hal_wifi_init()` en el core 0 antes de lanzar el core 1 (crea las colas); si el chip no arranca, la UI muestra "Wifi apagado" |
| Escaneo | Cada vez que se entra a Wifi. Sin redes ocultas; igual SSID = una sola (la de mejor señal); máximo `UI_WIFI_MAX_NETS` (8); timeout 10 s |
| Orden | Primero las abiertas, después las que piden clave; en cada grupo, de mejor a peor señal |
| Botones | Abiertas en violeta, con clave en gris. La conectada lleva un tilde. A/C recorren redes y Volver; D vuelve |
| B en una abierta | Conecta (asociación y DHCP, timeout 15 s). Si había otra conexión, se suelta antes |
| B en una con clave | El título dice "Requiere clave"; no intenta conectar (no hay cómo escribir la clave) |
| Título (`LblRedes`) | "Buscando...", "Redes", "Sin redes", "Error al buscar", "Conectando...", "Conectado", "Fallo conexion", "Desconectado" (se cayó), "Wifi apagado" |

Colas: 4 pedidos y 4 reportes; con la cola de reportes llena se pierde el más viejo. Cada
reporte lleva la lista completa, así que la UI siempre se queda con el último.
