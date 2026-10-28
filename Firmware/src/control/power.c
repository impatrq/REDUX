// power.c · Implementación de intensidad → duty con rampas y frenado antes de invertir.

#include "power.h"

#include "config.h" // DUTY_MIN, DUTY_MAX, RAMP_*_PER_S, CTRL_TICK_US
#include "util.h"   // clampf, slewf

// Paso máximo de duty por tick para una pendiente dada (unidades de duty por segundo).
static float step_per_tick(float rate_per_s) {
    return rate_per_s * ((float)CTRL_TICK_US / 1000000.0f);
}

void power_init(power_state_t *ps) {
    ps->dir = DIR_STOP;
    ps->duty = 0.0f;
}

float power_target(float intensity, float factor) {
    if (factor <= 0.0f) {
        return 0.0f; // sentido bloqueado: 0, no DUTY_MIN
    }
    const float i = clampf(intensity, 0.0f, 1.0f);
    const float f = clampf(factor, 0.0f, 1.0f);
    const float duty = DUTY_MIN + i * (DUTY_MAX - DUTY_MIN) * f;
    return clampf(duty, 0.0f, DUTY_MAX);
}

void power_step(power_state_t *ps, direction_t dir_obj, float duty_obj, bool hard_stop) {
    if (hard_stop) {
        ps->dir = DIR_STOP;
        ps->duty = 0.0f;
        return;
    }

    // Si ya está detenido, puede adoptar el nuevo sentido sin frenar primero.
    if (ps->duty <= 0.0f && ps->dir != dir_obj) {
        ps->dir = dir_obj;
    }

    // Objetivo de este tick: el duty pedido solo si el sentido coincide y no es STOP;
    // en cualquier otro caso (parar o invertir) el objetivo es frenar a 0.
    float target;
    if (dir_obj == ps->dir && dir_obj != DIR_STOP) {
        target = clampf(duty_obj, 0.0f, DUTY_MAX);
    } else {
        target = 0.0f;
    }

    const float step = (target > ps->duty) ? step_per_tick(RAMP_UP_PER_S)
                                            : step_per_tick(RAMP_DOWN_PER_S);
    ps->duty = slewf(ps->duty, target, step);

    // Al llegar a 0 adopta el sentido pedido: así una inversión pasa por 0 antes de
    // arrancar en el nuevo sentido, y un pedido de STOP deja dir en STOP.
    if (ps->duty <= 0.0f) {
        ps->duty = 0.0f;
        ps->dir = dir_obj;
    }
}
