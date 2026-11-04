/**
 * @file hal_wifi.h
 * @brief Wi-Fi del CYW43 (Pico 2 W): escaneo de redes y conexión a redes abiertas.
 *
 * El chip lo maneja el core 0: hal_wifi_init() y hal_wifi_poll() se llaman solo desde ahí.
 * La UI (core 1) pide cosas y recibe reportes por colas, sin tocar el driver:
 * hal_wifi_request_*() y hal_wifi_get_report() no bloquean. Ver docs/ui_contract.md §13.
 */
#ifndef HAL_WIFI_H
#define HAL_WIFI_H

#include <stdbool.h>
#include <stdint.h>

#include "ui/ui_config.h"

#define HAL_WIFI_SSID_MAX_LEN  32u  /* Máximo de 802.11 */

typedef enum {
    HAL_WIFI_STATE_OFF = 0,      /**< El chip no arrancó: no hay Wi-Fi */
    HAL_WIFI_STATE_IDLE,         /**< Sin escaneo ni conexión en curso */
    HAL_WIFI_STATE_SCANNING,
    HAL_WIFI_STATE_SCAN_DONE,    /**< nets[] tiene el resultado del último escaneo */
    HAL_WIFI_STATE_SCAN_FAILED,
    HAL_WIFI_STATE_CONNECTING,
    HAL_WIFI_STATE_CONNECTED,    /**< Asociado y con IP (DHCP) */
    HAL_WIFI_STATE_CONNECT_FAILED
} hal_wifi_state_t;

typedef struct {
    char ssid[HAL_WIFI_SSID_MAX_LEN + 1u];
    int16_t rssi_dbm;
    bool open;                   /**< true si no pide contraseña */
} hal_wifi_net_t;

/** Reporte core 0 -> core 1. Lleva siempre la última lista, abiertas primero y por señal. */
typedef struct {
    hal_wifi_state_t state;
    uint32_t net_count;
    hal_wifi_net_t nets[UI_WIFI_MAX_NETS];
    char connected_ssid[HAL_WIFI_SSID_MAX_LEN + 1u];  /**< "" si no hay conexión */
} hal_wifi_report_t;

/** Core 0, antes de arrancar el core 1: crea las colas y arranca el chip en modo estación. */
bool hal_wifi_init(void);

/** Core 0, cada UI_WIFI_POLL_MS: atiende pedidos, termina escaneos y conexiones (con timeout). */
void hal_wifi_poll(void);

/** Core 1: pide un escaneo. Se atiende cuando termina lo que esté en curso. */
bool hal_wifi_request_scan(void);

/** Core 1: pide conectarse a una red abierta. */
bool hal_wifi_request_connect(const char *ssid);

/** Core 1: saca el reporte más viejo sin leer. false si no hay. */
bool hal_wifi_get_report(hal_wifi_report_t *report);

#endif /* HAL_WIFI_H */
