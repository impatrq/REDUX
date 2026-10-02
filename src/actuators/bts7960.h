// bts7960.h · Driver del puente H: aplica dirección y duty garantizando que nunca
// haya PWM en ambos lados. Depende de hal_pwm y hal_gpio. No lee sensores.
#ifndef REDUX_BTS7960_H
#define REDUX_BTS7960_H

#include <stdbool.h>

#include "redux_types.h" // direction_t

// Configura RPWM (GP8) y LPWM (GP9) a PWM_FREQ_HZ con duty 0, y deja R_EN/L_EN en
// bajo. Arranca deshabilitado: ninguna corriente llega al motor hasta bts7960_enable.
void bts7960_init(void);

// Habilita o deshabilita ambos half-bridges. Al deshabilitar también fuerza duty 0.
void bts7960_enable(bool on);

// Deja ambos PWM (RPWM y LPWM) en 0 en la misma llamada. Freno pasivo.
void bts7960_stop(void);

// Aplica un sentido con su duty. Invariantes que garantiza:
//   - recorta el duty al rango [0, DUTY_MAX];
//   - pone a 0 el lado opuesto ANTES de escribir el lado activo, así nunca hay
//     PWM > 0 en RPWM y LPWM a la vez;
//   - DIR_STOP equivale a bts7960_stop().
// dir: DIR_FLEX -> RPWM; DIR_EXT -> LPWM; DIR_STOP -> ambos en 0.
void bts7960_apply(direction_t dir, float duty);

#endif // REDUX_BTS7960_H
