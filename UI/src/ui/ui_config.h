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
#define UI_PROFILE_ID_CHARS          "0123456789"  /* Lo único que acepta el ID de usuario */

/* ---- Wi-Fi (hal_wifi y pantalla Wifi, los dos en el core 1) ---- */
#define UI_WIFI_MAX_NETS             8u        /* Redes que se muestran (conectables primero) */
#define UI_WIFI_SCAN_TIMEOUT_MS      10000u    /* Un escaneo normal tarda ~3 s */
#define UI_WIFI_CONNECT_TIMEOUT_MS   30000u    /* Asociación + DHCP (algunas redes tardan >15 s) */
#define UI_WIFI_LINK_LOST_MS         5000u     /* El enlace debe seguir caído este tiempo para darlo por perdido */
#define UI_WIFI_RECONNECT_MS         15000u    /* Reintento de autoconexión si no hay conexión */
#define UI_WIFI_REPORT_CHECK_MS      100u      /* Cada cuánto la UI relee el estado del Wi-Fi */

/* ---- Contrato con el firmware ---- */
#define UI_STATUS_PERIOD_MS          50u       /* El firmware publica a ~20 Hz */

/* ---- API de sesiones (hal_api, core 1): POST /api/sesion por Wi-Fi ---- */
#define UI_API_PATH_SESION           "/api/sesion"
#define UI_API_PATH_PACIENTE         "/api/paciente/"  /* + <id> (Paso C, punto 6) */
#define UI_API_POLL_PERIOD_MS        20u       /* Cada cuánto corre hal_api_poll (máquina de estados) */
#define UI_API_RETRY_MAX             5u        /* Reintentos ante falla recuperable (sin Wi-Fi, 5xx, timeout) */
#define UI_API_RETRY_INTERVAL_MS     5000u     /* Espera entre reintentos, con el MISMO id_envio */
#define UI_API_CONNECT_TIMEOUT_MS    8000u     /* Tope para asociar la conexión TCP */
#define UI_API_RESPONSE_TIMEOUT_MS   8000u     /* Tope para enviar y recibir la respuesta */
#define UI_API_RESP_MAX              256u      /* Bytes de la respuesta HTTP que se guardan (línea de estado + motivo) */
#define UI_API_HEADER_MAX            256u      /* Tope de los encabezados HTTP de la petición */
#define UI_API_MOTIVO_MAX            64u       /* Tope del motivo de error que se muestra en pantalla */

#endif /* UI_CONFIG_H */
