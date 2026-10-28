// mock_hal_i2c.c · Doble de hal_i2c para las pruebas en PC. Simula el AS5600: una cola
// de cuentas crudas que alimentan las lecturas de RAW ANGLE (0x0C) y un juego de bits
// MD/ML/MH para las lecturas de STATUS (0x0B). Permite forzar un status de bus para
// probar las fallas. No habla con ningún hardware.

#include "hal_i2c.h"

#include <stddef.h>

#include "mock_hal.h"

// Registros y bits del AS5600 que el doble reconoce (espejo de as5600.c).
#define MOCK_REG_STATUS 0x0Bu
#define MOCK_REG_RAW_ANGLE 0x0Cu
#define MOCK_STATUS_MH 0x08u
#define MOCK_STATUS_ML 0x10u
#define MOCK_STATUS_MD 0x20u

#define MOCK_I2C_QUEUE_LEN 512u // cuentas crudas encoladas

static int32_t g_queue[MOCK_I2C_QUEUE_LEN];
static uint32_t g_head;
static uint32_t g_tail;
static int32_t g_last;       // última cuenta entregada (se repite si la cola se vacía)
static uint8_t g_status_reg; // bits MD/ML/MH que devuelve 0x0B
static status_t g_forced;    // status forzado para toda lectura

static int32_t pop_raw(void) {
    if (g_head != g_tail) {
        g_last = g_queue[g_head];
        g_head = (g_head + 1u) % MOCK_I2C_QUEUE_LEN;
    }
    return g_last;
}

void mock_i2c_reset(void) {
    g_head = 0u;
    g_tail = 0u;
    g_last = 0;
    g_status_reg = MOCK_STATUS_MD; // por defecto: imán detectado y en rango
    g_forced = STATUS_OK;
}

void mock_i2c_set_status(status_t st) {
    g_forced = st;
}

void mock_i2c_set_magnet(bool md, bool ml, bool mh) {
    g_status_reg = (uint8_t)((md ? MOCK_STATUS_MD : 0u) | (ml ? MOCK_STATUS_ML : 0u) |
                             (mh ? MOCK_STATUS_MH : 0u));
}

void mock_i2c_push_raw(int32_t raw) {
    const uint32_t next = (g_tail + 1u) % MOCK_I2C_QUEUE_LEN;
    if (next != g_head) {
        g_queue[g_tail] = raw;
        g_tail = next;
    }
}

void mock_i2c_push_raw_n(int32_t raw, uint32_t n) {
    for (uint32_t i = 0u; i < n; ++i) {
        mock_i2c_push_raw(raw);
    }
}

void mock_i2c_push_ramp(int32_t start, int32_t step, uint32_t n) {
    for (uint32_t i = 0u; i < n; ++i) {
        mock_i2c_push_raw(start + step * (int32_t)i);
    }
}

status_t hal_i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    (void)addr;
    if (buf == NULL || len == 0u) {
        return STATUS_BAD_ARG;
    }
    if (g_forced != STATUS_OK) {
        return g_forced;
    }
    if (reg == MOCK_REG_STATUS) {
        buf[0] = g_status_reg;
        return STATUS_OK;
    }
    if (reg == MOCK_REG_RAW_ANGLE && len >= 2u) {
        const int32_t raw = pop_raw();
        buf[0] = (uint8_t)((raw >> 8) & 0x0F); // nibble alto
        buf[1] = (uint8_t)(raw & 0xFF);        // byte bajo
        return STATUS_OK;
    }
    return STATUS_BAD_ARG;
}

status_t hal_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    (void)addr;
    (void)reg;
    (void)value;
    return g_forced;
}
