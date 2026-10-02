// hal_i2c.c · Implementación de lectura/escritura de registros I2C con timeout (SDK).

#include "hal_i2c.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "board.h"
#include "config.h"

// Traduce el valor de retorno del SDK (bytes transferidos o PICO_ERROR_*) a status_t.
static status_t map_result(int result, size_t expected) {
    if (result == PICO_ERROR_TIMEOUT) {
        return STATUS_TIMEOUT;
    }
    if (result < 0 || (size_t)result != expected) {
        return STATUS_NACK; // sin ACK: no están todos los bytes
    }
    return STATUS_OK;
}

void hal_i2c_init(void) {
    i2c_init(BOARD_I2C_INST, I2C_BUS_FREQ_HZ);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);
}

status_t hal_i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    if (buf == NULL || len == 0u) {
        return STATUS_BAD_ARG;
    }
    // Fase de escritura del registro, manteniendo el bus (nostop=true) para el restart.
    int w = i2c_write_timeout_us(BOARD_I2C_INST, addr, &reg, 1, true, I2C_TIMEOUT_US);
    status_t st = map_result(w, 1u);
    if (st != STATUS_OK) {
        return st;
    }
    int r = i2c_read_timeout_us(BOARD_I2C_INST, addr, buf, len, false, I2C_TIMEOUT_US);
    return map_result(r, len);
}

status_t hal_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    const uint8_t payload[2] = {reg, value};
    int w = i2c_write_timeout_us(BOARD_I2C_INST, addr, payload, 2, false, I2C_TIMEOUT_US);
    return map_result(w, 2u);
}
