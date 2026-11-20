/**
 * @file hal_api.h
 * @brief Cliente HTTP mínimo de la API de REDUX (POST /api/sesion) por Wi-Fi.
 *
 * Todo corre en el core 1 (el mismo que LVGL y hal_wifi): usa lwIP crudo (TCP raw,
 * pico_cyw43_arch_lwip_threadsafe_background) con una máquina de estados por polling,
 * así que NUNCA bloquea el lazo (el latido del panel sigue latiendo durante el envío
 * y sus reintentos). El core 0 (motor) no toca nada de esto.
 *
 * Config local: src/ui/api_config.h (IP de la computadora, puerto y clave X-Redux-Key);
 * está en .gitignore. Plantilla: src/ui/api_config.example.h.
 *
 * Reintentos (Paso C, punto 3): ante una falla recuperable (sin Wi-Fi, sin respuesta,
 * error 5xx) reintenta cada UI_API_RETRY_INTERVAL_MS, hasta UI_API_RETRY_MAX veces, con
 * el MISMO cuerpo (mismo id_envio: la API deduplica). 200/201 = enviado; 4xx = error
 * definitivo (se muestra el motivo).
 */
#ifndef HAL_API_H
#define HAL_API_H

#include <stdbool.h>
#include <stdint.h>

/** Estado del envío, para la pantalla de fin (estado REAL, no por tiempo). */
typedef enum {
    HAL_API_IDLE = 0,  /**< Sin envío en curso ni resultado reciente */
    HAL_API_SENDING,   /**< Conectando / enviando / esperando respuesta */
    HAL_API_RETRYING,  /**< Falla recuperable: esperando para reintentar */
    HAL_API_SENT,      /**< Último envío aceptado (200/201) */
    HAL_API_ERROR      /**< Error definitivo (4xx) o se agotaron los reintentos */
} hal_api_state_t;

/** Core 1: prepara el cliente. Devuelve false si falta api_config.h (no se enviará). */
bool hal_api_init(void);

/** Core 1, en el mismo lazo que hal_wifi_poll: atiende la máquina de estados. No bloquea. */
void hal_api_poll(void);

/**
 * Core 1: encola un POST /api/sesion con @p body (de @p len bytes). El buffer NO se copia:
 * debe seguir válido y sin cambiar hasta que el envío termine (hal_api_busy() vuelve a false).
 * @p paciente es solo para el log por USB. Si ya hay un envío en curso, ignora el pedido.
 */
void hal_api_send_session(const char *body, uint32_t len, int32_t paciente);

/** Estado actual del envío. */
hal_api_state_t hal_api_get_state(void);

/** Motivo del último error (válido con HAL_API_ERROR); "" si no hay. */
const char *hal_api_error_motivo(void);

/** true mientras haya un envío en curso (incluida la espera entre reintentos). */
bool hal_api_busy(void);

#endif /* HAL_API_H */
