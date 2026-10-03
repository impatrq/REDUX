// limits.h · Bloquear el sentido que supera el límite y atenuar en la zona de freno.
// Depende de config y util. No toca hardware.
#ifndef REDUX_LIMITS_H
#define REDUX_LIMITS_H

#include <stdbool.h>

#include "redux_types.h"

// Factor de atenuación [0.0, 1.0] para el sentido `dir` en el ángulo actual.
// Convención (Fase 3): 0° = flexión máxima; el ángulo crece al extender.
//   FLEX: distancia al límite = angle_deg - lo_deg.
//   EXT : distancia al límite = hi_deg  - angle_deg.
// distancia <= 0            -> 0.0  (bloquea ese sentido; cubre fuera de rango).
// distancia < BRAKE_ZONE_DEG-> distancia / BRAKE_ZONE_DEG  (atenuación lineal).
// distancia >= BRAKE_ZONE_DEG-> 1.0.
// DIR_STOP                  -> 0.0.
// Función pura, sin estado.
float limits_factor(direction_t dir, float angle_deg, float lo_deg, float hi_deg);

// true si el sentido que se está aplicando (`dir`) ya alcanzó o pasó su límite,
// de modo que el supervisor debe cortar al instante aunque el duty esté en rampa
// (regla de seguridad 3). FLEX corta en angle_deg <= lo_deg; EXT en angle_deg >= hi_deg;
// DIR_STOP nunca corta. Fuera de rango solo da true para el sentido que se aleja.
// Función pura, sin estado.
bool limits_must_stop(direction_t dir, float angle_deg, float lo_deg, float hi_deg);

#endif // REDUX_LIMITS_H
