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
- [ ] ¿Acepta el modo nuevo `UI_MODE_IDLE` (listo, sin sesión) y que `UI_EV_CONFIRM` en IDLE inicie la sesión?
- [ ] ~~Al iniciar la sesión sin rango calibrado, ¿pasa primero por `CALIB_ROM`?~~ Rediseño 03/11:
      cada sesión arranca con `CALIB_ROM`. ¿Le sirve al firmware calibrar el rango en cada sesión?
- [ ] ¿La calibración EMG sigue en el flujo? Si sigue, ¿va antes o después de `CALIB_ROM`, y cada
      paso termina con "Guardar" (`UI_EV_CONFIRM`) o por tiempo? La UI la muestra en `ScreenBegin`.
- [ ] ¿Qué pasa al guardar cada extremo? ¿El firmware toma el ángulo del encoder al recibir `UI_EV_CONFIRM`?
- [ ] ID de usuario (Perfil): ¿lo usa el firmware? Si sí, hace falta un evento o campo nuevo en el contrato.
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
