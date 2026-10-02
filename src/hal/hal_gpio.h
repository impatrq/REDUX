// hal_gpio.h · Pines de enable del driver y LED de estado.
#ifndef REDUX_HAL_GPIO_H
#define REDUX_HAL_GPIO_H

#include <stdbool.h>

// Configura R_EN y L_EN del BTS7960 como salida y los deja en bajo (driver apagado).
// Es la condición segura de arranque de toda la v1.
void hal_gpio_init(void);

// Configura el LED de estado (GP2) como salida digital. Separado de hal_gpio_init
// para que el programa de prueba pueda tomar el mismo pin por PWM sin conflicto.
void hal_gpio_led_init(void);

// Enciende o apaga el LED de estado (activo en alto).
void hal_gpio_led_set(bool on);

#endif // REDUX_HAL_GPIO_H
