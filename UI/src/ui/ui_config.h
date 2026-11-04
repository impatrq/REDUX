/**
 * @file ui_config.h
 * @brief Constantes de la línea UI (pantalla, teclado, LVGL, colas).
 *
 * Archivo propio de la UI: se entrega al firmware tal cual y no se mezcla con
 * su config.h. Los valores marcados "a ajustar" se fijan en la fase indicada.
 */
#ifndef UI_CONFIG_H
#define UI_CONFIG_H

/* ---- LVGL ---- */
#define UI_LVGL_VERSION_STR          "8.3.11"  /* Debe coincidir con el export de SquareLine */
#define UI_LVGL_VERSION_MAJOR        8
#define UI_LVGL_VERSION_MINOR        3
#define UI_LVGL_VERSION_PATCH        11
#define UI_LVGL_HANDLER_PERIOD_MS    5u        /* Cada cuánto corre lv_timer_handler() */
#define UI_LVGL_BUF_LINES            20u       /* Alto de cada buffer parcial (hay dos) */

/* ---- Core 1 ---- */
#define UI_CORE1_STACK_BYTES         8192u     /* Pila propia: la de defecto no alcanza a LVGL */

/* ---- Pantalla ST7735 ---- */
#define UI_LCD_WIDTH_PX              128u
#define UI_LCD_HEIGHT_PX             128u
#define UI_LCD_SPI_BAUD_HZ           15000000u /* Verificado en U1; probar hasta ~30 MHz */
#define UI_LCD_COL_OFFSET_PX         2u        /* Verificado en U1 (green tab) */
#define UI_LCD_ROW_OFFSET_PX         3u        /* Verificado en U1 (green tab) */

/* Orientación y color (registro MADCTL). Verificados en U1 con el módulo actual:
 * - MIRROR_X / MIRROR_Y: espejan columnas / filas (1 = espejado).
 * - SWAP_XY: intercambia filas y columnas (rota 90°).
 * - COLOR_BGR: 1 si al pedir rojo se ve azul con 0 (orden BGR del panel).
 * - INVERT: 1 si los colores salen en negativo (blanco se ve negro). */
#define UI_LCD_MIRROR_X              1u
#define UI_LCD_MIRROR_Y              1u
#define UI_LCD_SWAP_XY               0u
#define UI_LCD_COLOR_BGR             1u
#define UI_LCD_INVERT                0u

/* Tiempos del panel */
#define UI_LCD_RESET_PULSE_MS        20u       /* RST en bajo */
#define UI_LCD_RESET_WAIT_MS         150u      /* Espera tras soltar RST */

/* Timeout de SPI/DMA: tiempo teórico de la transferencia × factor + margen */
#define UI_SPI_TIMEOUT_FACTOR        2u
#define UI_SPI_TIMEOUT_MARGIN_US     2000u

/* ---- Teclado 4x4 ---- */
#define UI_KEYPAD_SCAN_PERIOD_MS     10u       /* Cada cuánto se barre la matriz */
#define UI_KEYPAD_DEBOUNCE_SCANS     3u        /* Barridos iguales para aceptar un cambio (30 ms) */
#define UI_KEYPAD_SETTLE_US          10u       /* Espera tras activar una fila antes de leer */
#define UI_KEYPAD_EVENT_QUEUE_LEN    8u        /* Eventos presionar/soltar sin leer */

/* Teclas de navegación (docs/ui_contract.md §6), por el carácter impreso en la tecla */
#define UI_KEY_UP_CHAR               'A'       /* LV_KEY_PREV: foco al anterior */
#define UI_KEY_ENTER_CHAR            'B'       /* LV_KEY_ENTER: acepta */
#define UI_KEY_DOWN_CHAR             'C'       /* LV_KEY_NEXT: foco al siguiente */
#define UI_KEY_BACK_CHAR             'D'       /* LV_KEY_ESC: vuelve */
#define UI_KEY_ERASE_CHAR            '*'       /* LV_KEY_BACKSPACE: borra un dígito (Perfil) */
#define UI_KEY_ROTATE_CHAR           '#'       /* No va a LVGL: gira la interfaz 90° a la izquierda */
/* Los dígitos '0'…'9' llegan a LVGL como su propio carácter (campo de ID en Perfil). */

/* ---- Pantallas ---- */
#define UI_SEND_MSG_MS               5000u     /* "Enviando datos..." en ScreenEnding */
#define UI_PROFILE_ID_CHARS          "0123456789"  /* Lo único que acepta el ID de usuario */

/* ---- Wi-Fi (hal_wifi en el core 0, pantalla Wifi en el core 1) ---- */
#define UI_WIFI_MAX_NETS             8u        /* Redes que se muestran (abiertas primero) */
#define UI_WIFI_POLL_MS              10u       /* Cada cuánto el core 0 llama a hal_wifi_poll() */
#define UI_WIFI_SCAN_TIMEOUT_MS      10000u    /* Un escaneo normal tarda ~3 s */
#define UI_WIFI_CONNECT_TIMEOUT_MS   15000u    /* Asociación + DHCP */
#define UI_WIFI_REQUEST_QUEUE_LEN    4u        /* Pedidos de la UI sin atender */
#define UI_WIFI_REPORT_QUEUE_LEN     4u        /* Reportes sin leer; si se llena se pierde el más viejo */
#define UI_WIFI_REPORT_CHECK_MS      100u      /* Cada cuánto la UI lee los reportes */

/* ---- Contrato con el firmware ---- */
#define UI_STATUS_PERIOD_MS          50u       /* El firmware publica a ~20 Hz */

#endif /* UI_CONFIG_H */
