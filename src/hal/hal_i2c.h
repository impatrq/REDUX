// hal_i2c.h · Leer y escribir registros por I2C con timeout obligatorio; devuelve
// status_t. Nunca bloquea sin límite de tiempo.
#ifndef REDUX_HAL_I2C_H
#define REDUX_HAL_I2C_H

#include <stddef.h>
#include <stdint.h>

#include "redux_types.h"

// Inicializa el bus I2C0 en GP4/GP5 a la frecuencia de config.h, con pull-ups internos.
void hal_i2c_init(void);

// Lee `len` bytes a partir del registro `reg` del dispositivo `addr` (7 bits).
// Devuelve STATUS_OK, STATUS_TIMEOUT si vence el timeout, STATUS_NACK si el
// dispositivo no responde, o STATUS_BAD_ARG si los argumentos no son válidos.
status_t hal_i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);

// Escribe un byte en el registro `reg` del dispositivo `addr` (7 bits).
// Mismos códigos de retorno que hal_i2c_read_regs.
status_t hal_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value);

#endif // REDUX_HAL_I2C_H
