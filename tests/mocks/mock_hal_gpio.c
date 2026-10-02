// mock_hal_gpio.c · Doble de hal_gpio para las pruebas en PC. Registra el estado de
// enable del motor y hace de hal_gpio_init un no-op (no hay hardware que inicializar).

#include "hal_gpio.h"

#include "mock_hal.h"

static bool g_motor_enabled;

// Coordinado por mock_hal_reset() (definido en mock_hal_pwm.c).
void mock_hal_gpio_reset(void) {
    g_motor_enabled = false;
}

void hal_gpio_init(void) {
    // Condición segura de arranque: el motor queda deshabilitado.
    g_motor_enabled = false;
}

void hal_gpio_motor_enable(bool on) {
    g_motor_enabled = on;
}

bool mock_motor_enabled(void) {
    return g_motor_enabled;
}
