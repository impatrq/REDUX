// rep_stats.c · Implementación de la estadística por repetición. Lógica pura: solo usa
// las constantes de config.h (umbrales de descarte y zona muerta) y DUTY_CEIL para la
// asistencia. Ver el contrato y la convención de ángulo en rep_stats.h.

#include "rep_stats.h"

#include "config.h"

// Cierra la repetición en curso con los acumuladores actuales. Devuelve true y llena *out
// si supera los mínimos de rango y duración; false si se descarta.
static bool close_rep(const rep_stats_t *rs, uint32_t now_ms, rep_t *out) {
    const float range = rs->max_angle - rs->min_angle;
    const uint32_t dur = now_ms - rs->start_ms; // ms monótonos; la resta tolera el wrap
    if (range < REP_MIN_RANGE_DEG || dur < REP_MIN_DURATION_MS) {
        return false; // repetición demasiado corta en recorrido o en tiempo: se descarta
    }
    out->rango_grados = range;
    out->duracion_ms = dur;
    if (rs->active_n > 0u) {
        const float mean_duty = (float)(rs->duty_sum / (double)rs->active_n);
        out->asistencia_pct = mean_duty / DUTY_CEIL * 100.0f;
    } else {
        out->asistencia_pct = 0.0f; // el motor no se activó en esta repetición
    }
    return true;
}

// Arranca una repetición nueva en el ángulo `a` (pico de extensión = inicio de flexión).
static void open_rep(rep_stats_t *rs, float a, uint32_t now_ms) {
    rs->rep_open = true;
    rs->min_angle = a;
    rs->max_angle = a;
    rs->start_ms = now_ms;
    rs->duty_sum = 0.0;
    rs->active_n = 0u;
}

void rep_stats_init(rep_stats_t *rs) {
    rs->have_ref = false;
    rs->rep_open = false;
    rs->move_dir = 0;
    rs->extreme = 0.0f;
    rs->min_angle = 0.0f;
    rs->max_angle = 0.0f;
    rs->start_ms = 0u;
    rs->duty_sum = 0.0;
    rs->active_n = 0u;
}

bool rep_stats_sample(rep_stats_t *rs, float angle_deg, float duty_applied, bool motor_active,
                      uint32_t now_ms, rep_t *out) {
    // Primera muestra: fija la referencia y no abre repetición (segmento parcial inicial).
    if (!rs->have_ref) {
        rs->have_ref = true;
        rs->extreme = angle_deg;
        rs->move_dir = 0;
        return false;
    }

    bool emitted = false;
    if (rs->move_dir > 0) {
        // Extendiendo (subiendo): seguimos el máximo; una caída de al menos la zona muerta
        // es el inicio de una flexión = frontera entre repeticiones.
        if (angle_deg > rs->extreme) {
            rs->extreme = angle_deg;
        } else if (angle_deg <= rs->extreme - REP_DIR_DEADBAND_DEG) {
            if (rs->rep_open) {
                emitted = close_rep(rs, now_ms, out); // cierra la rep en curso
            }
            open_rep(rs, angle_deg, now_ms); // y abre la nueva en este pico
            rs->move_dir = -1;
            rs->extreme = angle_deg;
        }
    } else if (rs->move_dir < 0) {
        // Flexionando (bajando): seguimos el mínimo; una subida de al menos la zona muerta
        // es el inicio de una extensión (no abre repetición, solo cambia el pivote).
        if (angle_deg < rs->extreme) {
            rs->extreme = angle_deg;
        } else if (angle_deg >= rs->extreme + REP_DIR_DEADBAND_DEG) {
            rs->move_dir = 1;
            rs->extreme = angle_deg;
        }
    } else {
        // Sin decidir aún: clasificamos con la primera desviación mayor que la zona muerta.
        // Si arranca bajando no hay pico previo, así que la primera repetición recién se
        // abre en el próximo pico de extensión.
        if (angle_deg >= rs->extreme + REP_DIR_DEADBAND_DEG) {
            rs->move_dir = 1;
            rs->extreme = angle_deg;
        } else if (angle_deg <= rs->extreme - REP_DIR_DEADBAND_DEG) {
            rs->move_dir = -1;
            rs->extreme = angle_deg;
        } else if (angle_deg > rs->extreme) {
            rs->extreme = angle_deg; // mientras tanto, el pivote sigue al máximo
        }
    }

    // Acumula la muestra en la repetición en curso (ya reiniciada si se abrió una nueva).
    if (rs->rep_open) {
        if (angle_deg < rs->min_angle) {
            rs->min_angle = angle_deg;
        }
        if (angle_deg > rs->max_angle) {
            rs->max_angle = angle_deg;
        }
        if (motor_active) {
            rs->duty_sum += (double)duty_applied;
            rs->active_n++;
        }
    }
    return emitted;
}

bool rep_stats_finish(rep_stats_t *rs, uint32_t now_ms, rep_t *out) {
    if (!rs->rep_open) {
        return false;
    }
    const bool emitted = close_rep(rs, now_ms, out);
    rs->rep_open = false;
    return emitted;
}
