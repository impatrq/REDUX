// hal_gpio.c · Implementación de los pines de enable y LED (SDK).

#include "hal_gpio.h"

#include "hardware/gpio.h"

#include "board.h"

void hal_gpio_init(void) {
    // No-op en esta PCB: R_EN/L_EN del BTS7960 van fijos a +5 V del lado del motor,
    // así que el firmware no maneja ningún pin de enable. La condición segura de
    // arranque la dan RPWM y LPWM en 0 (ver bts7960_init).
    (void)0;
}

void hal_gpio_motor_enable(bool on) {
    // No-op en esta PCB: los enable están fijos en alto por hardware y no llegan a
    // ningún GPIO. Se conserva la firma para no romper bts7960_enable ni los
    // programas de banco; la habilitación/corte real depende de RPWM/LPWM.
    (void)on;
}

void hal_gpio_led_init(void) {
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, 0);
}

void hal_gpio_led_set(bool on) {
    gpio_put(PIN_LED, on);
}
