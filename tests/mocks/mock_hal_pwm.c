// mock_hal_pwm.c · Doble de hal_pwm para las pruebas en PC. A diferencia del HAL real,
// NO recorta el duty: registra exactamente lo que el driver escribe, para poder
// observar que el recorte a DUTY_MAX lo hace el driver y verificar el orden de escritura.

#include "hal_pwm.h"

#include "mock_hal.h"

#define MOCK_PWM_PINS 64u // suficiente para los GPIO de la placa

// Hook interno implementado en mock_hal_gpio.c (un único mock_hal_reset los coordina).
void mock_hal_gpio_reset(void);

static mock_pwm_write_t g_log[MOCK_PWM_LOG_MAX];
static uint32_t g_count;
static float g_last_duty[MOCK_PWM_PINS];

void mock_hal_reset(void) {
    g_count = 0u;
    for (uint32_t i = 0u; i < MOCK_PWM_PINS; ++i) {
        g_last_duty[i] = 0.0f;
    }
    // El estado del mock de gpio se resetea en mock_hal_gpio.c vía su propio hook.
    mock_hal_gpio_reset();
}

void hal_pwm_init(uint32_t pin, uint32_t freq_hz) {
    (void)freq_hz;
    if (pin < MOCK_PWM_PINS) {
        g_last_duty[pin] = 0.0f;
    }
}

void hal_pwm_set_duty(uint32_t pin, float duty) {
    if (g_count < MOCK_PWM_LOG_MAX) {
        g_log[g_count].pin = pin;
        g_log[g_count].duty = duty;
        g_count++;
    }
    if (pin < MOCK_PWM_PINS) {
        g_last_duty[pin] = duty;
    }
}

uint32_t mock_pwm_write_count(void) {
    return g_count;
}

mock_pwm_write_t mock_pwm_write_at(uint32_t i) {
    if (i < g_count) {
        return g_log[i];
    }
    const mock_pwm_write_t none = {0u, 0.0f};
    return none;
}

float mock_pwm_last_duty(uint32_t pin) {
    if (pin < MOCK_PWM_PINS) {
        return g_last_duty[pin];
    }
    return 0.0f;
}
