# Registro de sincronización con redux-firmware

El firmware está en la computadora del compañero. Cada intercambio de archivos o información se
anota acá (más reciente arriba). Los puntos de sincronización están en `PLAN_UI.md` §5.

Formato:

```text
## AAAA-MM-DD · S<n> · <resumen>
- Pedido:   qué se le pidió al compañero
- Recibido: qué llegó (archivos, versión/commit del firmware si lo sabe)
- Enviado:  qué se le mandó
- Cambios:  qué se modificó en el laboratorio por esto
- Pendiente: preguntas abiertas
```

---

## 2026-11-19 · Paso C · Envío de la sesión a la API por Wi-Fi (sin la PC por USB)

La Pico envía el resultado de la sesión a la API (`POST /api/sesion`) por Wi-Fi, desde el
core 1 (junto a LVGL y `hal_wifi`), sin bloquear el control del core 0. La línea `@SESION`
por USB **se mantiene siempre** como respaldo (no depende de esto). Compila en PANEL+SIM,
PANEL+AS5600 y BASIC; las pruebas unitarias siguen en verde (se sumaron 4 a
`test_session_json`). Decisiones tomadas sin frenar (como se acordó):

- **Config local:** `src/ui/api_config.h` (IP de la PC, puerto 5000, clave `X-Redux-Key`),
  junto a `wifi_redes.h` y **en el `.gitignore`** (no se sube: lleva la clave). Plantilla
  versionada `api_config.example.h`. Conecta por **IP directa** (sin DNS).
- **Cliente HTTP = `src/hal/hal_api.c`** (al lado de `hal_wifi`): lwIP **TCP raw**
  (`threadsafe_background`, ya en uso), máquina de estados por *polling* → **nunca bloquea**
  (el latido del panel sigue durante el envío y sus reintentos). Se lo bombea con un
  `lv_timer` creado en `ui_panel_fw` (core 1), así **no se tocó `ui_core1.c` y el laboratorio
  queda intacto**.
- **Reintentos (punto 3):** 200/201 = **enviado**; 4xx = **error definitivo** (se muestra el
  `{"error":...}` de la API); sin Wi-Fi / sin respuesta / 5xx = reintento cada 5 s, hasta 5
  veces, con el **mismo `id_envio`** (la API deduplica por `id_envio`, verificado en
  `api_pico.py`). Cuerpo grande enviado **por partes** respetando `tcp_sndbuf` (callback
  `sent`); el cuerpo se envía **sin copia** (persiste en el buffer compartido).
- **Paso core 0 → core 1:** el core 0 arma el cuerpo JSON en un **buffer compartido** (una
  sola copia, dimensionado para el peor caso `SESSION_MAX_REPS` ≈ **48 KB**; RAM verificada
  con `size`: `.bss` ≈ 178 KB sobre 520 KB del RP2350, entra con amplio margen) y lo entrega
  con `ui_app_tx_*` (contrato `app/ui.h`), protegido con `critical_section`. `session.c` suma
  `session_build_body()` (cuerpo HTTP sin el prefijo `@SESION` ni `\n`), reusando los mismos
  escritores de fragmentos que el emisor por USB (una sola fuente del formato). Si una sesión
  **no entra** en el buffer, no se envía por Wi-Fi: sale completa por USB y la pantalla de fin
  muestra **"Error: sesión muy larga, quedó en USB"** (nunca falla en silencio).
- **Pantalla de fin con estado REAL (punto 4):** `ScreenEnding` ya **no** usa el timer fijo de
  5 s; `ui_panel_set_tx_status()` refleja el estado real del cliente ("Enviando…", "Enviado",
  "Sin conexión, reintentando…", "Error: <motivo>"). **No** hizo falta agregar un campo a
  `ui_status_t`: el estado del envío **nace en el core 1** (donde está el cliente), no en el
  core 0, así que el panel lo lee directo y el contrato de estado queda igual.
- **Logs por USB (punto 7):** `API | POST /api/sesion -> <código>` en cada intento (con el
  motivo en los 4xx y el número de reintento en las fallas recuperables).
- **Punto 6 (GET /api/paciente/<id> para el nombre en Perfil):** PENDIENTE (era "sólo si sobra
  tiempo"). El cliente y la ruta ya están contemplados (`UI_API_PATH_PACIENTE`); falta el
  segundo flujo HTTP y un label de nombre en `ScreenPerfil`. Se deja para una próxima entrada.
- **Alcance UI + firmware:** el Paso C toca `UI/` (config junto a `wifi_redes.h`, cliente HTTP
  junto al Wi-Fi) y `Firmware/` (emisor del cuerpo, puente entre núcleos). El `CLAUDE.md` del
  firmware dice "trabajar solo dentro de `Firmware/`", pero este paso es intrínsecamente
  UI+API; se siguió el pedido explícito del Paso C. Fuera de los commits: `wifi_redes.h`,
  `api_config.h` y los `CLAUDE.md`.

## 2026-11-19 · Paso D · Integración: un solo firmware con la UI del panel (core 1)

Un único firmware en `../Firmware/` que corre la UI real del panel (LVGL + SquareLine + teclado
+ Wi-Fi en el core 1) y el control en el core 0. Elegible por CMake: `REDUX_UI=PANEL|BASIC`
(default PANEL) y `REDUX_ENC=AS5600|SIM` (default AS5600). Compila en las dos combinaciones y en
PANEL+SIM; el laboratorio `redux_ui_lab` sigue compilando; las 13 pruebas unitarias siguen en
verde. Decisiones tomadas sin frenar (como se acordó):

- **Contrato único:** `app/ui.h` (v2) pasó a vivir en el firmware y es la única copia. `ui_event_t`
  salió de `redux_types.h` (ahora incluye `app/ui.h`); se agregó `src/` a los includes de las
  pruebas. Se borraron `UI/src/app/ui.h` y `UI/src/hal/board.h`; `redux_ui_lab` ahora apunta a
  `../Firmware/src` para `app/ui.h` y `hal/board.h`.
- **board.h único = el del firmware.** Ganó una sección de *alias* `BOARD_*` (mismos GPIO, sin
  números nuevos) para que el código de la UI compile sin cambiar sus nombres.
- **Enganche del core 0** separado en `app/fw_ui.h` (ui_init/tick/poll/message/set_waiting +
  `ui_feed_line`). `ui_init` se renombró a **`fw_ui_init`** para no chocar con el `ui_init()` del
  export de SquareLine (ahora están en el mismo binario).
- **Contrato cruzado `ui_app_*`** (no existía): implementado en `app/ui_panel_fw.c` con
  `critical_section` — mailbox de estado (último valor), cola de eventos y buffer de sesión. El
  core 1 corre `ui_core1` (LVGL + Wi-Fi) y un *setup* nuevo que cablea `ui_panel` (reemplaza a
  `lab_screens_test`): un `lv_timer` lee el estado y llama `ui_panel_show_status`, y el callback de
  eventos los encola al core 0.
- **Latido (ajuste D.1):** el core 1 marca un tiempo en cada pulso de estado; el core 0 lo vigila
  **solo después** de que el core 1 avisa que terminó de arrancar (LVGL + `hal_wifi_init`). Si se
  pierde >500 ms, el core 0 lo inyecta como `cycle_overrun` → el supervisor entra en FAULT
  (motor apagado) **sin tocar su lógica pura ni sus tests**. Verificado que el Wi-Fi del core 1 es
  asíncrono (`cyw43_arch_wifi_connect_async`, escaneo por *polling*, sólo *locks* lwIP cortos): ningún
  llamado bloquea >500 ms, así que no hizo falta subir el margen. El watchdog sigue igual.
- **Estado → UI (20 Hz):** `sys_state_t`→`ui_mode` (BOOT/CHECK_ENC→BOOT, IDLE→IDLE, CALIB_ROM→
  CALIB_ROM, CALIB_REST/MVC→CALIB_EMG, ARMING/RUN→RUN, FAULT→FAULT); prompt desde `g_rom.phase`/
  `g_mvc`; `fault_t`→`UI_FAULT_*` (ENC_MASK→ENCODER, EMG_SAT→EMG, OVERRUN→OTHER;
  ANGLE_LIMIT/OVERCURRENT quedan sin uso). `rep_count` y `last_rom_deg` desde la sesión/rep_stats
  (se agregó `session_last_rep_range`). **`duty` se publica normalizado** `duty/DUTY_CEIL` (0..1), así
  "100%" es la asistencia máxima de la build.
- **Eventos UI → supervisor:** START_SESSION (toma brazo+ID por `ui_app_take_session`, equivale a
  `sesion <id> <i|d>`), CONFIRM y CANCEL (= `fin`). Los comandos por serie siguen igual.
- **Arranque sin pulsador:** en PANEL el core 0 inyecta un CONFIRM inicial para que
  BOOT→CHECK_ENC→IDLE y el panel llegue solo al menú (Inicio→Main).
- **Encoder SIM:** el ángulo sigue la **convención del firmware** (0°=flexión; extender SUBE,
  flexionar BAJA), *no* la literal del pedido ("flexión sube"), para no romper rom_calib/limits/
  rep_stats. En marcha integra el duty/sentido aplicados (0–`ENC_SIM_MAX_DEG`=140°). Para calibrar
  el ROM sin hardware se agregó el comando serie **`ang <grados>`** (posiciona el encoder simulado).
- **Fin de sesión (punto 7):** el resultado sigue saliendo por USB con `@SESION` (Wi-Fi = Paso C). El
  panel ya muestra "Enviando datos..."→"Enviados!" al Terminar, **por tiempo (5 s)**, no atado al fin
  real del envío ni a si la sesión se guarda (paciente 0/sin reps): el contrato v2 no tiene campo para
  eso. Queda propuesto para el Paso C / contrato v3.
- **Punto 5 (calibración EMG simulada por pasos, ventana de 2 s):** PENDIENTE (era "sólo si sobra
  tiempo"). Con EMG SERIAL la calibración EMG se autocompleta como hasta ahora, así que el flujo se
  recorre igual (reposo/bíceps/tríceps pasan rápido).
- Los programas de banco (`Pruebas/banco`) siguen en BASIC + AS5600.

## 2026-11-19 · Paso B · Contrato v2 (brazo, ID, START_SESSION, reps/ROM) — sin sincronizar

- Pedido:   nada aún; avisar en S2 junto con el contrato.
- Recibido: nada.
- Enviado:  nada aún.
- Cambios:  contrato **v2** en `app/ui.h` (ver `ui_contract.md` §2–§4 y §12):
  - `ui_arm_t` (izquierdo/derecho) e ID de paciente (`UI_PATIENT_ID_MAX_LEN` dígitos).
  - `UI_EV_START_SESSION` **al final del enum** (no cambia los valores previos); payload
    `ui_event_msg_t` (type + arm + patient_id).
  - `ui_app_publish_session()` / `ui_app_take_session()`: la sesión viaja por un **buffer de
    último valor** (no cola), que el firmware protege con una `critical_section`. Propuesta,
    pendiente de aceptar.
  - `ui_status_t` suma `rep_count` y `last_rom_deg`, que se muestran en marcha.
  - Flujo: "Comenzar" → `ScreenBrazo` → elegir brazo manda `START_SESSION` con brazo + ID de
    Perfil → `CALIB_ROM` (flexión, extensión) → `CALIB_EMG` (reposo, bíceps, tríceps) → `RUN`.
    Cada "Guardar X" = `UI_EV_CONFIRM`; la UI muestra "Midiendo..." hasta que el estado avanza.
  - Fuente `Font14Latin` (0x20-0xFF) como default; textos de la UI en UTF-8 (ñ y tildes). El
    tilde de la red conectada pasó de `LV_SYMBOL_OK` a "»" (la fuente no trae FontAwesome).
  - Proyecto de SquareLine versionado en `UI/squareline_proyecto/` (sin `Redux.slp`, `backup/`,
    `cache/`, ni los `.c`/`.bin` generados en `assets/`).
- Pendiente: las preguntas nuevas de "Preguntas abiertas para S2" (brazo, ID, buffer de sesión,
  reps/ROM, y los 2 s de medición).

## 2026-11-19 · S1 · Wi-Fi probado en la placa y afinado

- Pedido:   —
- Recibido: —
- Enviado:  —
- Cambios:  tras probar con varias redes (abiertas y WPA2), ajustes de robustez en `hal_wifi.c`
            y `ui_config.h`:
  - Timeout de conexión 15 s → **30 s** (`UI_WIFI_CONNECT_TIMEOUT_MS`): algunas redes tardan
    >15 s en asociar + DHCP.
  - Ahorro de energía del CYW43 **desactivado** (`cyw43_wifi_pm(..., CYW43_PERFORMANCE_PM)`):
    la radio no duerme entre balizas (causa de asociaciones lentas y caídas). Se eligió
    `PERFORMANCE_PM` sobre `NONE_PM` (ambos sin ahorro) por ser el de timings recomendados.
  - **Antirrebote de pérdida** de 5 s (`UI_WIFI_LINK_LOST_MS`): el enlace se da por perdido solo
    si sigue caído 5 s seguidos; los JOIN/NOIP intermedios de una renovación de DHCP no cuentan.
  - **Mensajes** por USB: al conectar, RSSI + IP; al fallar/perder, el `link status` del CYW43
    y el RSSI.
  - Resultado: estable en la placa (incluidas las redes que antes tardaban o se caían). Ver
    `docs/ui_contract.md` §13.
- Pendiente:
  - [ ] Confirmar con el firmware que GP15 efectivamente queda libre (no lo usa para otra cosa).
  - [ ] ¿El firmware necesita saber el estado del Wi-Fi (conectado/SSID) por el contrato, o es
        solo asunto de la UI?

## 2026-11-18 · S1 · El firmware ya está en el repo (`../Firmware`); Wi-Fi al core 1

- Pedido:   —
- Recibido: el firmware ahora vive en `../Firmware` (mismo repo). Su `board.h`
            (`../Firmware/src/hal/board.h`) es el que manda. Se puede leer, no se modifica desde `UI/`.
- Enviado:  —
- Cambios:
  - Pines de `src/hal/board.h` (UI) reconciliados con el del firmware: coinciden (pantalla
    GP10–GP14, teclado GP16–GP22 + GP0, LED GP2, botón GP3). Se mantienen los nombres
    `BOARD_PIN_LCD_*` / `BOARD_PIN_KEY_*` que usa el código de la UI.
  - **Luz de fondo:** el LED de la ST7735 va fijo a 3V3 en la PCB. Se sacó `BOARD_PIN_LCD_BL`
    (GP15) y su manejo en `st7735.c` / `lv_port_disp.c`. **GP15 queda libre.**
  - **Wi-Fi al core 1:** `hal_wifi_init()` y `hal_wifi_poll()` corren en `ui_core1` (mismo lazo
    que LVGL); el CYW43 se inicia en el core 1 para que sus IRQ caigan ahí. El core 0 ya no toca
    el Wi-Fi (en el firmware ese núcleo maneja el motor cada 1 ms). Se eliminaron las colas entre
    núcleos: la UI usa getters + contador de generación de escaneo.
  - **Redes guardadas:** `src/ui/wifi_redes.h` (lista SSID+clave WPA2, en `.gitignore`) con
    plantilla `wifi_redes.example.h`. Autoconexión al arrancar a la primera red guardada que
    aparezca; en `WifiScreen` las guardadas se marcan en verde y conectan con B usando su clave;
    reconexión automática cada 15 s a la última red conectada.
  - Ver `docs/ui_contract.md` §13 (reescrito).
- Pendiente:
  - [ ] Confirmar con el firmware que GP15 efectivamente queda libre (no lo usa para otra cosa).
  - [x] Probar el Wi-Fi en el hardware: escaneo, autoconexión y reconexión sin trabar la
        animación ni el teclado. Probado el 19/11 (estable); afinado en la entrada de ese día.
  - [ ] ¿El firmware necesita saber el estado del Wi-Fi (conectado/SSID) por el contrato, o es
        solo asunto de la UI?

## Preguntas abiertas para S1

- [ ] Enviar `src/hal/board.h` del firmware (o la tabla de pines vigente).
- [ ] ¿Los pines de pantalla (GP10–GP15) y teclado (GP16–GP22, GP0) siguen libres y sin cambios?
- [ ] Números de GPIO del puente H BTS7960 y de los EMG (para no usarlos desde la UI).
- [ ] ¿Ya existe `app/ui.h` en el firmware (implementación `ui_basic`)? Si existe, mandarlo.
- [ ] ¿Está de acuerdo con el contrato propuesto (`docs/ui_contract.md`)?
- [ ] Estado actual de las fases del firmware.

## Preguntas abiertas para S2

- [ ] Tabla de traducción `sys_state_t → ui_mode_t / ui_prompt_t` y `fault_t → UI_FAULT_*`.
- [ ] ¿Qué corre hoy en el core 1 (telemetría)? ¿Cada cuánto y cuánto tarda?
- [ ] ¿Acepta el modo `UI_MODE_IDLE` (listo, sin sesión) y que la sesión arranque con
      `UI_EV_START_SESSION` (v2; antes era `UI_EV_CONFIRM` en IDLE)?
- [ ] **Brazo (v2):** ¿el firmware necesita saber el brazo (izquierdo/derecho) de la sesión?
      Llega en `ui_event_msg_t.arm` con `START_SESSION`.
- [ ] **Buffer de sesión (v2):** ¿acepta `ui_app_publish_session()`/`ui_app_take_session()` con
      buffer de último valor + `critical_section`, en vez de meter brazo + ID en la cola de eventos?
- [ ] ~~Al iniciar la sesión sin rango calibrado, ¿pasa primero por `CALIB_ROM`?~~ Rediseño 03/11:
      cada sesión arranca con `CALIB_ROM`. ¿Le sirve al firmware calibrar el rango en cada sesión?
- [ ] ¿La calibración EMG sigue en el flujo? La UI la recorre en `ScreenBegin` como CALIB_EMG
      (reposo → bíceps → tríceps), después de CALIB_ROM (flexión → extensión). ¿Va en ese orden?
- [ ] **Medición por paso (v2):** cada "Guardar X" manda `UI_EV_CONFIRM` y la UI muestra
      "Midiendo..." hasta que el firmware **avanza el estado** (cambia `prompt` o `mode`). ¿Le sirve
      ese contrato (la UI no usa timeout propio; el firmware marca el fin moviendo el estado)?
      ¿Cuánto dura la medición? (en el lab se simulan 2 s.)
- [ ] ¿Qué pasa al guardar cada paso? ¿El firmware toma el ángulo del encoder / la señal EMG al
      recibir `UI_EV_CONFIRM`?
- [ ] **ID de paciente (v2):** llega en `ui_event_msg_t.patient_id` con `START_SESSION` (dígitos,
      `UI_PATIENT_ID_MAX_LEN` máx.). ¿Lo usa el firmware? ¿Alcanza el largo?
- [ ] **reps/ROM (v2):** ¿qué cuenta como repetición (`rep_count`) y qué es `last_rom_deg` (ROM de
      la última repetición, en grados)? ¿El firmware los publica en `ui_status_t` a ~20 Hz?
- [ ] Fin de sesión ("Enviando datos..."): ¿qué datos se envían, adónde, y cómo avisa el firmware
      que terminó? Hoy la UI muestra el texto por tiempo (5 s).
- [ ] `UI_EV_RECALIBRATE` quedó sin uso: ¿se mantiene en el contrato o se quita?
- [ ] Wi-Fi (`ui_contract.md` §13): ¿el firmware usa el CYW43 (por ejemplo, para enviar los datos
      de la sesión)? En el laboratorio lo maneja `hal_wifi` en el core 0. ¿Lo adopta el firmware
      (llamando a `hal_wifi_init()` y `hal_wifi_poll()`) o ya tiene su propio manejo del Wi-Fi?
      ¿Tiene su propio `lwipopts.h`? ¿Usa `threadsafe_background` o `poll`? ¿Hay algún PIO o
      canal DMA reservado que choque con el del CYW43?

---

## 2026-11-04 · Lista de redes Wi-Fi en WifiScreen (sin sincronizar todavía)
- Pedido:   nada.
- Recibido: nada.
- Enviado:  nada aún; avisar en S2.
- Cambios:  `hal_wifi` (core 0) escanea y conecta a redes abiertas; la UI pide y recibe por
            colas propias, fuera de `app/ui.h`. `main.c` del laboratorio llama a
            `hal_wifi_init()` y `hal_wifi_poll()`. Se suman `src/lwipopts.h` y la librería
            `pico_cyw43_arch_lwip_threadsafe_background`. Detalle en `ui_contract.md` §13.
- Pendiente: la pregunta del Wi-Fi en "Preguntas abiertas para S2".

## 2026-11-03 · Rediseño de las pantallas (sin sincronizar todavía)
- Pedido:   nada.
- Recibido: nada.
- Enviado:  nada aún; avisar en S1/S2 junto con el contrato.
- Cambios:  export nuevo de SquareLine (v18). "Comenzar" (`UI_EV_CONFIRM`) lleva directo a
            `CALIB_ROM` (flexión y extensión, cada una con "Guardar" = `UI_EV_CONFIRM`) y de ahí a
            `RUN`. "Terminar" envía `UI_EV_CANCEL` y la UI muestra el fin de sesión. La barra de
            marcha muestra la potencia (`duty`), ya no el ángulo. Ya no hay "Recalibrar":
            `UI_EV_RECALIBRATE` queda en el enum sin uso. `CALIB_EMG` se muestra en la misma
            pantalla de calibración. Perfil tiene un campo de ID numérico (teclas 0–9 y `*`).
            Los valores de los enums no cambian. Detalle en `ui_contract.md` §3, §6 y §12.
- Pendiente: las preguntas nuevas de "Preguntas abiertas para S2".

## 2026-11-02 · U5 · Modo IDLE y flujo de sesión (sin sincronizar todavía)
- Pedido:   nada.
- Recibido: nada.
- Enviado:  nada aún; avisar en S1/S2 junto con el contrato.
- Cambios:  `app/ui.h` suma `UI_MODE_IDLE` al final del enum (los valores anteriores no cambian):
            tras `BOOT` la UI muestra su menú. "Comenzar" envía `UI_EV_CONFIRM` e inicia la sesión
            (EMG y marcha); la calibración del rango sale de Ajustes > Recalibrar. Comentarios de
            `UI_PROMPT_GO_*`: "apretar B" (antes A). Flujo completo en `ui_contract.md` §12.
- Pendiente: las tres preguntas nuevas de "Preguntas abiertas para S2".

## 2026-11-02 · Cambio de teclas en el contrato (sin sincronizar todavía)
- Pedido:   nada.
- Recibido: nada.
- Enviado:  nada aún; avisar en S1 junto con el contrato.
- Cambios:  navegación A = sube, C = baja, B = acepta, D = vuelve. `UI_EV_CONFIRM` sale de B,
            `UI_EV_CANCEL` de D y `UI_EV_RECALIBRATE` de un botón en pantalla (ya no hay tecla fija).
            Solo cambian comentarios de `app/ui.h`; los valores del enum siguen iguales.
- Pendiente: que el compañero lo acepte junto con el resto del contrato.

## 2026-10-05 · U0 · Laboratorio creado, sin sincronizar todavía
- Pedido:   nada aún.
- Recibido: nada aún.
- Enviado:  nada aún.
- Cambios:  `board.h` provisorio desde `PLAN_UI.md` §2; contrato v1 propuesto en `app/ui.h`.
- Pendiente: todo lo de "Preguntas abiertas para S1".
