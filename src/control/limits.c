// limits.c · Implementación de límites articulares y zona de freno (lógica pura).

#include "limits.h"

#include "config.h" // BRAKE_ZONE_DEG
#include "util.h"   // clampf

// Distancia (en grados) desde el ángulo actual hasta el límite en el sentido `dir`.
// Positiva dentro del rango; <= 0 al alcanzar o pasar el límite de ese sentido.
// DIR_STOP no tiene sentido de giro, así que no define distancia (ver cada llamador).
static float distance_to_limit(direction_t dir, float angle_deg, float lo_deg, float hi_deg) {
    if (dir == DIR_FLEX) {
        return angle_deg - lo_deg; // FLEX baja el ángulo hacia lo_deg
    }
    return hi_deg - angle_deg; // DIR_EXT: sube el ángulo hacia hi_deg
}

float limits_factor(direction_t dir, float angle_deg, float lo_deg, float hi_deg) {
    if (dir == DIR_STOP) {
        return 0.0f;
    }
    const float distance_deg = distance_to_limit(dir, angle_deg, lo_deg, hi_deg);
    if (distance_deg <= 0.0f) {
        return 0.0f; // en el límite o fuera de rango en este sentido
    }
    return clampf(distance_deg / BRAKE_ZONE_DEG, 0.0f, 1.0f);
}

bool limits_must_stop(direction_t dir, float angle_deg, float lo_deg, float hi_deg) {
    if (dir == DIR_STOP) {
        return false;
    }
    return distance_to_limit(dir, angle_deg, lo_deg, hi_deg) <= 0.0f;
}
