// hal_pwm.c · Implementación de la configuración de frecuencia y duty de PWM (SDK).

#include "hal_pwm.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "util.h"

// Guardamos el wrap de cada slice de PWM para traducir el duty (0.0..1.0) al nivel
// de comparación. NUM_PWM_SLICES lo define el SDK según el chip (12 en el RP2350).
static uint16_t g_wrap[NUM_PWM_SLICES];

void hal_pwm_init(uint32_t pin, uint32_t freq_hz) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(pin);

    // Elegimos divisor y wrap para acercarnos a freq_hz manteniendo wrap en 16 bits.
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t divider16 = (sys_hz / freq_hz / 4096u) + 1u; // divisor en pasos de 1/16
    if (divider16 < 16u) {
        divider16 = 16u; // divisor mínimo = 1.0
    }
    const uint32_t wrap = (sys_hz * 16u) / divider16 / freq_hz - 1u;

    pwm_set_clkdiv_int_frac(slice, (uint8_t)(divider16 / 16u), (uint8_t)(divider16 % 16u));
    pwm_set_wrap(slice, (uint16_t)wrap);
    g_wrap[slice] = (uint16_t)wrap;

    pwm_set_gpio_level(pin, 0); // arranca en duty 0
    pwm_set_enabled(slice, true);
}

void hal_pwm_set_duty(uint32_t pin, float duty) {
    const uint slice = pwm_gpio_to_slice_num(pin);
    const float d = clampf(duty, 0.0f, 1.0f);
    const uint16_t level = (uint16_t)(d * (float)g_wrap[slice] + 0.5f);
    pwm_set_gpio_level(pin, level);
}
