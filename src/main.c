// main.c · Punto de entrada: inicializa el SDK y los módulos, arranca core 1 y
// corre el lazo principal esperando el tick de 1 ms. No contiene lógica de control.

#include <stdio.h>

#include "pico/stdlib.h"

#include "config.h"
#include "hal_gpio.h"
#include "hal_time.h"

#define REDUX_FW_VERSION "0.1.0"

int main(void) {
    stdio_init_all();

    hal_gpio_init();     // R_EN y L_EN en bajo: el motor no se activa en esta fase
    hal_gpio_led_init(); // LED de estado por GPIO digital
    hal_time_init();     // tick de 1 ms

    printf("REDUX firmware v%s\n", REDUX_FW_VERSION);

    // Parpadeo del LED gobernado por el tick de 1 ms (sin sleep bloqueante), tal como
    // el lazo de control esperará el tick en las fases siguientes.
    uint32_t last_toggle = 0;
    bool led_on = false;
    while (true) {
        if (hal_time_tick_take()) {
            const uint32_t now = hal_time_ticks();
            if (now - last_toggle >= LED_BLINK_TICKS) {
                last_toggle = now;
                led_on = !led_on;
                hal_gpio_led_set(led_on);
            }
        }
    }
}
