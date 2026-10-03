// power.h · Duty objetivo desde la intensidad; rampas de subida y bajada; frenar antes
// de invertir. Depende de config y util. No toca hardware.
#ifndef REDUX_POWER_H
#define REDUX_POWER_H

#include <stdbool.h>

#include "redux_types.h"

// Estado de la rampa de potencia. Lo posee el llamador (sin estado global). Un struct
// en cero equivale a motor detenido: DIR_STOP (==0) y duty 0.0.
typedef struct {
    direction_t dir; // sentido aplicado actualmente
    float duty;      // duty aplicado actualmente [0.0, DUTY_MAX]
} power_state_t;

// Deja el estado en reposo: DIR_STOP y duty 0.0.
void power_init(power_state_t *ps);

// Duty objetivo a partir de la intensidad EMG [0,1] y el factor de límites [0,1].
//   factor <= 0 -> 0.0  (NO DUTY_MIN: el sentido está bloqueado).
//   si no       -> DUTY_MIN + intensity*(DUTY_MAX-DUTY_MIN)*factor, nunca mayor a DUTY_MAX.
// intensity y factor se acotan a [0,1]. Función pura.
float power_target(float intensity, float factor);

// Avanza un tick (CTRL_TICK_US) el estado hacia el sentido `dir_obj` y el duty `duty_obj`:
//   hard_stop -> dir STOP y duty 0 al instante.
//   subida limitada por RAMP_UP_PER_S, bajada por RAMP_DOWN_PER_S.
//   si el sentido pedido difiere del actual, primero baja a 0 en el sentido actual y recién
//   al llegar a 0 adopta el nuevo: nunca pasa de FLEX a EXT directo.
//   dir_obj STOP -> baja con rampa hasta 0.
void power_step(power_state_t *ps, direction_t dir_obj, float duty_obj, bool hard_stop);

#endif // REDUX_POWER_H
