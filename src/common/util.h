// util.h · Funciones static inline sin estado: clampf, mapf (map lineal), slewf.
// Puede incluirlo cualquiera; no incluye a nadie.
#ifndef REDUX_UTIL_H
#define REDUX_UTIL_H

// Recorta x al intervalo [lo, hi]. Asume lo <= hi.
static inline float clampf(float x, float lo, float hi) {
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

// Mapea x desde [in_lo, in_hi] a [out_lo, out_hi] de forma lineal.
// Si el rango de entrada es nulo devuelve out_lo (evita división por cero).
static inline float mapf(float x, float in_lo, float in_hi, float out_lo, float out_hi) {
    const float span = in_hi - in_lo;
    if (span == 0.0f) {
        return out_lo;
    }
    return out_lo + (x - in_lo) * (out_hi - out_lo) / span;
}

// Acerca value a target como máximo max_step por llamada (límite de pendiente).
static inline float slewf(float value, float target, float max_step) {
    const float delta = target - value;
    if (delta > max_step) {
        return value + max_step;
    }
    if (delta < -max_step) {
        return value - max_step;
    }
    return target;
}

#endif // REDUX_UTIL_H
