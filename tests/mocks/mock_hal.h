// mock_hal.h · Interfaz de inspección de los dobles de hal_pwm y hal_gpio para las
// pruebas en PC. Los mocks registran cada escritura de duty (pin y valor, en orden)
// y el estado de enable del motor, para verificar los invariantes del driver.
#ifndef REDUX_MOCK_HAL_H
#define REDUX_MOCK_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "redux_types.h" // status_t

#define MOCK_PWM_LOG_MAX 4096u // escrituras de duty registradas antes de saturar

// Una escritura de duty tal como la hizo el driver (sin recortes del mock).
typedef struct {
    uint32_t pin;
    float duty;
} mock_pwm_write_t;

// Deja los mocks en estado inicial: log vacío, duties desconocidos, motor deshabilitado.
void mock_hal_reset(void);

// --- hal_pwm ---
uint32_t mock_pwm_write_count(void);         // cantidad de escrituras registradas
mock_pwm_write_t mock_pwm_write_at(uint32_t i); // i-ésima escritura (orden de llamada)
float mock_pwm_last_duty(uint32_t pin);      // último duty escrito en un pin (0 si ninguno)

// --- hal_gpio ---
bool mock_motor_enabled(void); // último estado pasado a hal_gpio_motor_enable

// --- hal_i2c (doble del AS5600) ---
// Programa lo que devuelven las lecturas de registros del encoder. Las lecturas de
// RAW ANGLE (0x0C) consumen una cuenta de la cola; si la cola queda vacía se repite
// la última. Las lecturas de STATUS (0x0B) devuelven los bits de imán programados.
void mock_i2c_reset(void);                             // cola vacía, status OK, imán OK
void mock_i2c_set_status(status_t st);                 // fuerza el status de toda lectura
void mock_i2c_set_magnet(bool md, bool ml, bool mh);   // bits MD/ML/MH del registro STATUS
void mock_i2c_push_raw(int32_t raw);                   // encola una cuenta cruda (0..4095)
void mock_i2c_push_raw_n(int32_t raw, uint32_t n);     // encola n veces la misma cuenta
void mock_i2c_push_ramp(int32_t start, int32_t step, uint32_t n); // rampa (para dispersión)

#endif // REDUX_MOCK_HAL_H
