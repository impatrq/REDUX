// hal_gpio.h · Pines de enable del driver y LED de estado.
#ifndef REDUX_HAL_GPIO_H
#define REDUX_HAL_GPIO_H

#include <stdbool.h>

// No-op en esta PCB: los enable del BTS7960 van fijos a +5 V del motor y no se
// manejan por GPIO. Se conserva para no romper las llamadas existentes; la condición
// segura de arranque de la v1 la dan RPWM y LPWM en 0 (bts7960_init).
void hal_gpio_init(void);

// No-op en esta PCB: R_EN/L_EN están fijos en alto por hardware. Se conserva la firma
// para bts7960_enable y los programas de banco. La habilitación/corte real del motor
// depende de RPWM/LPWM, no de estos pines.
void hal_gpio_motor_enable(bool on);

// Configura el LED de estado (GP2) como salida digital. Separado de hal_gpio_init
// para que el programa de prueba pueda tomar el mismo pin por PWM sin conflicto.
void hal_gpio_led_init(void);

// Enciende o apaga el LED de estado (activo en alto).
void hal_gpio_led_set(bool on);

#endif // REDUX_HAL_GPIO_H
