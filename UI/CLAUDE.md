# REDUX UI lab

Laboratorio de la pantalla ST7735 (128x128, SPI1) y el teclado 4x4 con LVGL 8.3.11
para el exoesqueleto REDUX. Raspberry Pi Pico 2 W (RP2350), C11, Pico SDK 2.x.
Plan: docs/PLAN_UI.md. Contrato con el firmware: docs/ui_contract.md.
Sincronización con el firmware: docs/sync_log.md.

El firmware (redux-firmware) NO está en esta computadora: lo tiene un compañero.
No asumir nada de su código más allá de docs/ui_contract.md y la tabla de pines.

## Reglas
- LVGL corre solo en el core 1. El core 0 nunca llama funciones de LVGL.
- La UI no toca motor ni sensores: solo envía eventos UI_EV_* y muestra mensajes.
- src/app/ui.h es autocontenido: no incluye archivos del firmware. Incluirlo como "app/ui.h"
  (el export de SquareLine tiene su propio ui.h y su propia ui_init()).
- src/hal/board.h es provisorio; si llega el del compañero, ese manda. No cambiar pines por cuenta propia.
- Constantes de la UI en src/ui/ui_config.h, no en un config.h del firmware.
- No editar src/ui/squareline/ a mano; la lógica va en src/ui/ui_panel.c.
- Toda espera de hardware (SPI, DMA) tiene timeout.
- Las rutas de src/ deben coincidir con las de redux-firmware.
- Lo que haga falta saber del firmware se anota como pregunta en docs/sync_log.md.

## Convenciones (iguales al firmware)
- snake_case con prefijo de módulo; tipos con _t; constantes con unidad.
- Sin números mágicos, internos static, compilar sin avisos en código propio
  (el código propio se lista en REDUX_UI_LAB_SOURCES del CMakeLists.txt, con -Werror).
- Una fase U* por sesión; mostrar el plan antes de escribir código.

## Compilar
- Desde VS Code: "Compile Project" de la extensión Raspberry Pi Pico.
- Salida: build/redux_ui_lab.uf2. Mensajes por USB (stdio USB activo, UART apagado).

## Git
- Commits solo en local. Nunca hacer git push ni configurar remotos.
