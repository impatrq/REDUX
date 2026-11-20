/**
 * @file api_config.example.h
 * @brief Plantilla de la configuración de la API de sesiones (envío por Wi-Fi).
 *
 * Copiar a `api_config.h` (en la misma carpeta) y poner los valores reales.
 * `api_config.h` está en el .gitignore: NO se sube nunca (lleva la clave). Este
 * ejemplo sí se versiona, con datos ficticios.
 *
 *  - REDUX_API_HOST: IP de la computadora que corre la API, vista desde la red
 *    Wi-Fi a la que se conecta la Pico (p. ej. "192.168.1.100"). Es una IP, no un
 *    nombre: la Pico conecta directo sin DNS. Averiguarla con `ipconfig` (IPv4).
 *  - REDUX_API_PORT: puerto de la API (el servidor Flask escucha en 5000).
 *  - REDUX_API_KEY:  la misma clave que la API espera en el encabezado
 *    X-Redux-Key (REDUX_API_KEY del .env de la API).
 */
#ifndef API_CONFIG_H
#define API_CONFIG_H

#define REDUX_API_HOST "192.168.1.100"
#define REDUX_API_PORT 5000
#define REDUX_API_KEY  "cambiar-esta-clave"

#endif /* API_CONFIG_H */
