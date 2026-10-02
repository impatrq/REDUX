// bts7960.c · Implementación del driver del puente H BTS7960 (apagar opuesto, luego activar).

#include "bts7960.h"

#include "board.h"  // PIN_MOTOR_RPWM, PIN_MOTOR_LPWM
#include "config.h" // PWM_FREQ_HZ, DUTY_MAX
#include "hal_gpio.h"
#include "hal_pwm.h"
#include "util.h" // clampf

void bts7960_init(void) {
    // Condición segura de arranque: enables en bajo (deshabilitado) y duty 0.
    hal_gpio_init();
    hal_pwm_init(PIN_MOTOR_RPWM, PWM_FREQ_HZ);
    hal_pwm_init(PIN_MOTOR_LPWM, PWM_FREQ_HZ);
    bts7960_stop();
}

void bts7960_enable(bool on) {
    // Al deshabilitar, frenar primero: así los enable nunca caen con un duty activo.
    if (!on) {
        bts7960_stop();
    }
    hal_gpio_motor_enable(on);
}

void bts7960_stop(void) {
    hal_pwm_set_duty(PIN_MOTOR_RPWM, 0.0f);
    hal_pwm_set_duty(PIN_MOTOR_LPWM, 0.0f);
}

void bts7960_apply(direction_t dir, float duty) {
    const float d = clampf(duty, 0.0f, DUTY_MAX);

    switch (dir) {
    case DIR_FLEX: // flexión: RPWM activo, LPWM apagado
        hal_pwm_set_duty(PIN_MOTOR_LPWM, 0.0f);
        hal_pwm_set_duty(PIN_MOTOR_RPWM, d);
        break;
    case DIR_EXT: // extensión: LPWM activo, RPWM apagado
        hal_pwm_set_duty(PIN_MOTOR_RPWM, 0.0f);
        hal_pwm_set_duty(PIN_MOTOR_LPWM, d);
        break;
    case DIR_STOP:
    default:
        bts7960_stop();
        break;
    }
}
