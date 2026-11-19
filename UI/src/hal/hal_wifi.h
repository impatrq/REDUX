/**
 * @file hal_wifi.h
 * @brief Wi-Fi del CYW43 (Pico 2 W): escaneo, conexión (abiertas y WPA2 guardadas),
 *        autoconexión al arrancar y reconexión si se cae.
 *
 * Todo corre en el core 1 (el mismo que LVGL): el chip se inicia ahí para que sus
 * interrupciones caigan en ese core. El core 0 (motor, en el firmware) no toca nada
 * del Wi-Fi. Como no hay cruce de núcleos, no hay colas: la UI pide con
 * hal_wifi_request_*() y lee el estado con los getters, todo en el core 1 y sin
 * bloquear. Ver docs/ui_contract.md §13.
 *
 * Redes guardadas: src/ui/wifi_redes.h (lista de SSID + clave WPA2; está en
 * .gitignore). Plantilla: src/ui/wifi_redes.example.h.
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
    HAL_WIFI_STATE_SCAN_DONE,    /**< El último escaneo dejó la lista lista */
    HAL_WIFI_STATE_SCAN_FAILED,
    HAL_WIFI_STATE_CONNECTING,
    HAL_WIFI_STATE_CONNECTED,    /**< Asociado y con IP (DHCP) */
    HAL_WIFI_STATE_CONNECT_FAILED
} hal_wifi_state_t;

typedef struct {
    char ssid[HAL_WIFI_SSID_MAX_LEN + 1u];
    int16_t rssi_dbm;
    bool open;                   /**< true si no pide contraseña */
    bool saved;                  /**< true si está en wifi_redes.h (se conecta con su clave) */
} hal_wifi_net_t;

/** Una red guardada. La tabla WIFI_REDES de wifi_redes.h es de este tipo. */
typedef struct {
    const char *ssid;
    const char *password;  /**< Clave WPA2/WPA3-mixto, o "" si la red es abierta */
} hal_wifi_saved_t;

/** Core 1: arranca el chip en modo estación. false si el CYW43 no inicia. */
bool hal_wifi_init(void);

/** Core 1, en el mismo lazo que lv_timer_handler(): atiende pedidos, cierra escaneos y
 *  conexiones (con timeout), autoconecta al arrancar y reconecta si se cae. No bloquea. */
void hal_wifi_poll(void);

/** Core 1: pide un escaneo. Se atiende cuando termina lo que esté en curso. */
void hal_wifi_request_scan(void);

/** Core 1: pide conectarse a @p ssid. Si está guardada usa su clave (WPA2); si no, abierta. */
void hal_wifi_request_connect(const char *ssid);

/* ---- Lectura de estado (core 1) ---- */

/** Estado actual del Wi-Fi. */
hal_wifi_state_t hal_wifi_get_state(void);

/** SSID conectado, o "" si no hay conexión. */
const char *hal_wifi_connected_ssid(void);

/** Sube con cada escaneo terminado: la UI rehace la lista cuando cambia. */
uint32_t hal_wifi_scan_generation(void);

/** Copia hasta @p max redes del último escaneo (ya ordenadas) y devuelve cuántas copió. */
uint32_t hal_wifi_get_nets(hal_wifi_net_t *out, uint32_t max);

#endif /* HAL_WIFI_H */
