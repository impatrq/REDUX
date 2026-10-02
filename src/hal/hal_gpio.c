// hal_gpio.c · Implementación de los pines de enable y LED (SDK).

#include "hal_gpio.h"

#include "hardware/gpio.h"

#include "board.h"

void hal_gpio_init(void) {
    // Enable del BTS7960 en bajo = driver apagado. Además del pull-down externo de
    // 10 kΩ, lo forzamos por software en el arranque.
    gpio_init(PIN_MOTOR_R_EN);
    gpio_set_dir(PIN_MOTOR_R_EN, GPIO_OUT);
    gpio_put(PIN_MOTOR_R_EN, 0);

    gpio_init(PIN_MOTOR_L_EN);
    gpio_set_dir(PIN_MOTOR_L_EN, GPIO_OUT);
    gpio_put(PIN_MOTOR_L_EN, 0);
}

void hal_gpio_led_init(void) {
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, 0);
}

void hal_gpio_led_set(bool on) {
    gpio_put(PIN_LED, on);
}
