/**
 * @file wifi_redes.example.h
 * @brief Plantilla de redes Wi-Fi guardadas (WPA2/WPA3 mixto).
 *
 * Copiar a `wifi_redes.h` (en la misma carpeta) y poner las redes reales. `wifi_redes.h`
 * está en el .gitignore: NO se sube nunca. Este ejemplo sí se versiona, con datos ficticios.
 *
 * El orden es la prioridad: al arrancar, la UI escanea y se conecta sola a la PRIMERA red
 * de esta lista que aparezca. La clave es la WPA2/WPA3-PSK de la red; si la red es abierta,
 * dejar la clave en "" (se conecta sin contraseña, también sola al arrancar y al reconectar).
 */
#ifndef WIFI_REDES_H
#define WIFI_REDES_H

#include "hal/hal_wifi.h"  /* hal_wifi_saved_t */

static const hal_wifi_saved_t WIFI_REDES[] = {
    { "MiRedDeCasa",  "clave-de-ejemplo" },
    { "RedDelTaller", "otra-clave-aqui" },
    { "RedAbierta",   "" },               /* Red abierta: sin clave */
};

#endif /* WIFI_REDES_H */
