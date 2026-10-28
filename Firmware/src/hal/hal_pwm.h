// hal_pwm.h · Configurar la frecuencia de PWM y fijar el duty de 0.0 a 1.0.
// No conoce al BTS7960.
#ifndef REDUX_HAL_PWM_H
#define REDUX_HAL_PWM_H

#include <stdint.h>

// Configura el pin como salida PWM a la frecuencia pedida y lo deja con duty 0.
// Varios pines del mismo slice comparten frecuencia; llamar una vez por pin.
void hal_pwm_init(uint32_t pin, uint32_t freq_hz);

// Fija el ciclo de trabajo del pin, recortado al rango [0.0, 1.0].
void hal_pwm_set_duty(uint32_t pin, float duty);

#endif // REDUX_HAL_PWM_H
