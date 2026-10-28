// angle_watch.c · Implementación del detector de saltos de ángulo. Lógica pura.

#include "angle_watch.h"

#include "config.h"

// Valor absoluto en float sin depender de math.h (evita enlazar libm en las pruebas).
static float absf(float x) {
    return (x < 0.0f) ? -x : x;
}

void angle_watch_reset(angle_watch_t *w) {
    w->have_prev = false;
    w->prev_deg = 0.0f;
}

bool angle_watch_jump(angle_watch_t *w, float angle_deg) {
    bool jump = false;
    if (w->have_prev) {
        jump = absf(angle_deg - w->prev_deg) > ANGLE_JUMP_MAX_DEG;
    }
    w->prev_deg = angle_deg;
    w->have_prev = true;
    return jump;
}
