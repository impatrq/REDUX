// rom_calib.c · Máquina de estados de la calibración del rango del paciente.
// No bloquea: cada CONFIRM arranca un promedio que se completa de a una muestra por
// tick (as5600_avg_*), y al terminar valida imán, estabilidad y rango.

#include "rom_calib.h"

#include <stddef.h>

#include "as5600.h"
#include "config.h" // ROM_MIN_DEG, ROM_MAX_DEG, ROM_MARGIN_DEG, ROM_STABLE_MAX_DEG

// Marca el intento como fallido con un motivo. El usuario deberá RECALIBRAR.
static void fail(rom_calib_t *rc, rom_error_t err) {
    rc->phase = ROM_FAILED;
    rc->error = err;
}

// Verifica el imán y arranca el promedio del extremo indicado. Si el imán no está
// bien o el bus falla, marca el intento como fallido sin empezar a muestrear.
static void start_sampling(rom_calib_t *rc, rom_phase_t sampling_phase) {
    as5600_magnet_t m;
    const status_t st = as5600_read_magnet(&m);
    if (st != STATUS_OK) {
        fail(rc, ROM_ERR_BUS);
        return;
    }
    if (!m.detected || m.too_weak || m.too_strong) {
        fail(rc, ROM_ERR_MAGNET);
        return;
    }
    as5600_avg_begin();
    rc->phase = sampling_phase;
    rc->error = ROM_ERR_NONE;
}

// Con raw_flex y raw_ext ya tomados, calcula sign y rom_deg y valida el rango.
static void finalize(rom_calib_t *rc) {
    const int32_t diff = as5600_wrap_diff(rc->raw_ext, rc->raw_flex);
    rc->sign = (diff >= 0) ? 1 : -1;
    const int32_t span = (diff >= 0) ? diff : -diff;
    rc->rom_deg = as5600_counts_to_deg(span);
    if (rc->rom_deg < ROM_MIN_DEG || rc->rom_deg > ROM_MAX_DEG) {
        fail(rc, ROM_ERR_RANGE);
        return;
    }
    rc->phase = ROM_DONE;
    rc->error = ROM_ERR_NONE;
}

// Toma el resultado de un lote promediado y, si es estable, lo guarda y avanza.
static void collect_sample(rom_calib_t *rc) {
    const status_t st = as5600_avg_sample();
    if (st != STATUS_OK) {
        fail(rc, ROM_ERR_BUS);
        return;
    }
    if (!as5600_avg_ready()) {
        return; // faltan muestras: seguimos en los próximos ticks
    }
    int32_t raw;
    int32_t spread;
    as5600_avg_result(&raw, &spread);
    if (as5600_counts_to_deg(spread) > ROM_STABLE_MAX_DEG) {
        fail(rc, ROM_ERR_UNSTABLE);
        return;
    }
    if (rc->phase == ROM_SAMPLING_FLEX) {
        rc->raw_flex = raw;
        rc->phase = ROM_WAIT_EXT;
        rc->error = ROM_ERR_NONE;
    } else {
        rc->raw_ext = raw;
        finalize(rc);
    }
}

void rom_calib_begin(rom_calib_t *rc) {
    if (rc == NULL) {
        return;
    }
    rc->phase = ROM_WAIT_FLEX;
    rc->error = ROM_ERR_NONE;
    rc->raw_flex = 0;
    rc->raw_ext = 0;
    rc->sign = 1;
    rc->rom_deg = 0.0f;
}

void rom_calib_event(rom_calib_t *rc, ui_event_t ev) {
    if (rc == NULL) {
        return;
    }
    switch (ev) {
    case UI_EV_CANCEL:
        rc->phase = ROM_IDLE;
        rc->error = ROM_ERR_NONE;
        break;
    case UI_EV_RECALIBRATE:
        rom_calib_begin(rc);
        break;
    case UI_EV_CONFIRM:
        if (rc->phase == ROM_WAIT_FLEX) {
            start_sampling(rc, ROM_SAMPLING_FLEX);
        } else if (rc->phase == ROM_WAIT_EXT) {
            start_sampling(rc, ROM_SAMPLING_EXT);
        }
        break;
    case UI_EV_NONE:
    default:
        break;
    }
}

void rom_calib_tick(rom_calib_t *rc) {
    if (rc == NULL) {
        return;
    }
    if (rc->phase == ROM_SAMPLING_FLEX || rc->phase == ROM_SAMPLING_EXT) {
        collect_sample(rc);
    }
}

bool rom_calib_is_ready(const rom_calib_t *rc) {
    return rc != NULL && rc->phase == ROM_DONE;
}

float rom_calib_angle_deg(const rom_calib_t *rc, int32_t raw) {
    const int32_t diff = as5600_wrap_diff(raw, rc->raw_flex);
    return (float)rc->sign * as5600_counts_to_deg(diff);
}

float rom_calib_limit_lo(const rom_calib_t *rc) {
    (void)rc;
    return ROM_MARGIN_DEG;
}

float rom_calib_limit_hi(const rom_calib_t *rc) {
    return rc->rom_deg - ROM_MARGIN_DEG;
}
