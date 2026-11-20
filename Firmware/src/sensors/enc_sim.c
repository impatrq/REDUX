// enc_sim.c · Implementación del encoder simulado (ver enc_sim.h). Reemplaza a as5600.c en
// la build con REDUX_ENC=SIM: provee los mismos símbolos de as5600.h pero contra un ángulo
// interno en vez del bus I2C. No toca hardware; el resto del firmware (rom_calib, app) no
// cambia. Las funciones puras (wrap_diff, counts_to_deg) son idénticas a las de as5600.c.

#include "enc_sim.h"

#include "as5600.h" // AS5600_COUNTS y los prototipos que implementamos
#include "config.h" // AS5600_FILTER_SAMPLES, ENC_SIM_MAX_DEG, ENC_SIM_DEG_PER_S

#define ENC_SIM_HALF_COUNTS (AS5600_COUNTS / 2) // 2048: centro del envolvimiento
#define ENC_SIM_TICK_S      0.001f              // paso de integración = tick de control (1 ms)

static float s_angle_deg = 0.0f; // ángulo articular simulado (0 = flexión máxima)
static int32_t s_avg_n = 0;      // muestras acumuladas del promedio no bloqueante

// Cuenta cruda (0..AS5600_COUNTS-1) correspondiente al ángulo actual.
static int32_t enc_sim_raw_now(void) {
    float counts = (s_angle_deg / 360.0f) * (float)AS5600_COUNTS;
    int32_t raw = (int32_t)counts;
    raw %= AS5600_COUNTS;
    if (raw < 0) {
        raw += AS5600_COUNTS;
    }
    return raw;
}

void enc_sim_init(void) {
    s_angle_deg = 0.0f;
    s_avg_n = 0;
}

void enc_sim_set_angle_deg(float deg) {
    if (deg < 0.0f) {
        deg = 0.0f;
    } else if (deg > ENC_SIM_MAX_DEG) {
        deg = ENC_SIM_MAX_DEG;
    }
    s_angle_deg = deg;
}

void enc_sim_integrate(direction_t dir, float duty) {
    const float delta = duty * ENC_SIM_DEG_PER_S * ENC_SIM_TICK_S;
    if (dir == DIR_EXT) {
        s_angle_deg += delta; // extender sube el ángulo
    } else if (dir == DIR_FLEX) {
        s_angle_deg -= delta; // flexionar lo baja
    }
    if (s_angle_deg < 0.0f) {
        s_angle_deg = 0.0f;
    } else if (s_angle_deg > ENC_SIM_MAX_DEG) {
        s_angle_deg = ENC_SIM_MAX_DEG;
    }
}

float enc_sim_angle_deg(void) { return s_angle_deg; }

// --- Implementación de la API de as5600.h ------------------------------------
status_t as5600_read_raw(int32_t *raw_out) {
    *raw_out = enc_sim_raw_now();
    return STATUS_OK;
}

status_t as5600_read_magnet(as5600_magnet_t *m) {
    m->detected = true; // el imán simulado siempre está presente y bien ubicado
    m->too_weak = false;
    m->too_strong = false;
    return STATUS_OK;
}

void as5600_avg_begin(void) { s_avg_n = 0; }

status_t as5600_avg_sample(void) {
    if (s_avg_n < (int32_t)AS5600_FILTER_SAMPLES) {
        s_avg_n++;
    }
    return STATUS_OK;
}

bool as5600_avg_ready(void) { return s_avg_n >= (int32_t)AS5600_FILTER_SAMPLES; }

void as5600_avg_result(int32_t *raw_out, int32_t *spread_counts_out) {
    *raw_out = enc_sim_raw_now();
    *spread_counts_out = 0; // señal simulada perfectamente estable
}

int32_t as5600_wrap_diff(int32_t raw, int32_t ref) {
    int32_t d = (raw - ref + ENC_SIM_HALF_COUNTS) % AS5600_COUNTS;
    if (d < 0) {
        d += AS5600_COUNTS;
    }
    return d - ENC_SIM_HALF_COUNTS;
}

float as5600_counts_to_deg(int32_t counts) {
    return (float)counts * 360.0f / (float)AS5600_COUNTS;
}
