// as5600.c · Implementación del driver AS5600: lectura de registros por I2C,
// decodificación del imán y lectura promediada no bloqueante.

#include "as5600.h"

#include <stddef.h>

#include "config.h" // AS5600_FILTER_SAMPLES
#include "hal_i2c.h"

// Constantes del dispositivo (no son ni pines ni parámetros ajustables): dirección
// de 7 bits y registros del AS5600, con los bits del registro STATUS.
#define AS5600_ADDR 0x36u
#define AS5600_REG_STATUS 0x0Bu
#define AS5600_REG_RAW_ANGLE 0x0Cu // RAW ANGLE high (0x0C) + low (0x0D)
#define AS5600_STATUS_MH 0x08u     // Magnet too strong (AGC mínimo)
#define AS5600_STATUS_ML 0x10u     // Magnet too weak (AGC máximo)
#define AS5600_STATUS_MD 0x20u     // Magnet detected

// Estado del acumulador de la lectura promediada (un solo encoder en la v1).
static int32_t s_ref;    // primera muestra del lote: referencia de wrap_diff
static int32_t s_sum;    // suma de las diferencias envueltas respecto de s_ref
static int32_t s_min;    // mínima diferencia del lote
static int32_t s_max;    // máxima diferencia del lote
static uint32_t s_count; // muestras acumuladas

// Normaliza cuentas al rango [0, AS5600_COUNTS-1].
static int32_t norm_counts(int32_t v) {
    v %= AS5600_COUNTS;
    if (v < 0) {
        v += AS5600_COUNTS;
    }
    return v;
}

status_t as5600_read_raw(int32_t *raw_out) {
    if (raw_out == NULL) {
        return STATUS_BAD_ARG;
    }
    uint8_t buf[2];
    const status_t st = hal_i2c_read_regs(AS5600_ADDR, AS5600_REG_RAW_ANGLE, buf, sizeof buf);
    if (st != STATUS_OK) {
        return st;
    }
    // RAW ANGLE es de 12 bits: nibble alto en 0x0C, byte bajo en 0x0D.
    const int32_t raw = ((int32_t)(buf[0] & 0x0Fu) << 8) | (int32_t)buf[1];
    *raw_out = raw;
    return STATUS_OK;
}

status_t as5600_read_magnet(as5600_magnet_t *m) {
    if (m == NULL) {
        return STATUS_BAD_ARG;
    }
    uint8_t s;
    const status_t st = hal_i2c_read_regs(AS5600_ADDR, AS5600_REG_STATUS, &s, 1u);
    if (st != STATUS_OK) {
        return st;
    }
    m->detected = (s & AS5600_STATUS_MD) != 0u;
    m->too_weak = (s & AS5600_STATUS_ML) != 0u;
    m->too_strong = (s & AS5600_STATUS_MH) != 0u;
    return STATUS_OK;
}

void as5600_avg_begin(void) {
    s_count = 0u;
    s_ref = 0;
    s_sum = 0;
    s_min = 0;
    s_max = 0;
}

status_t as5600_avg_sample(void) {
    int32_t raw;
    const status_t st = as5600_read_raw(&raw);
    if (st != STATUS_OK) {
        return st;
    }
    if (s_count == 0u) {
        s_ref = raw; // la primera muestra fija la referencia; su diferencia es 0
    }
    const int32_t d = as5600_wrap_diff(raw, s_ref);
    if (s_count == 0u || d < s_min) {
        s_min = d;
    }
    if (s_count == 0u || d > s_max) {
        s_max = d;
    }
    s_sum += d;
    s_count++;
    return STATUS_OK;
}

bool as5600_avg_ready(void) {
    return s_count >= AS5600_FILTER_SAMPLES;
}

void as5600_avg_result(int32_t *raw_out, int32_t *spread_counts_out) {
    if (raw_out == NULL || spread_counts_out == NULL) {
        return;
    }
    if (s_count == 0u) {
        *raw_out = 0;
        *spread_counts_out = 0;
        return;
    }
    const int32_t n = (int32_t)s_count;
    // Redondeo al entero más cercano, respetando el signo de la suma.
    const int32_t mean = (s_sum >= 0) ? (s_sum + n / 2) / n : (s_sum - n / 2) / n;
    *raw_out = norm_counts(s_ref + mean);
    *spread_counts_out = s_max - s_min;
}

int32_t as5600_wrap_diff(int32_t raw, int32_t ref) {
    int32_t t = (raw - ref + AS5600_COUNTS / 2) % AS5600_COUNTS;
    if (t < 0) {
        t += AS5600_COUNTS; // C deja el módulo negativo si el operando lo es
    }
    return t - AS5600_COUNTS / 2;
}

float as5600_counts_to_deg(int32_t counts) {
    return (float)counts * (360.0f / (float)AS5600_COUNTS);
}
