// emg.c · Implementación del canal EMG: filtro exponencial, umbral con histéresis,
// tiempo mínimo de activación, intensidad 0–1 y detección de saturación.

#include "emg.h"

#include "config.h"
#include "util.h"

// Constante de pi: config no la define y aquí hace falta para la frecuencia de corte.
#define EMG_TWO_PI 6.28318530717958647692f

// Convierte una duración en ms a una cantidad entera de ticks del lazo de control.
static uint32_t emg_ms_to_ticks(uint32_t ms) {
    return (ms * 1000u) / CTRL_TICK_US;
}

float emg_env_alpha(float cutoff_hz, uint32_t tick_us) {
    if (cutoff_hz <= 0.0f || tick_us == 0u) {
        return 1.0f; // sin filtrado válido: la envolvente sigue a la muestra
    }
    const float dt = (float)tick_us * 1e-6f;
    const float rc = 1.0f / (EMG_TWO_PI * cutoff_hz);
    return dt / (rc + dt);
}

void emg_init(emg_channel_t *ch) {
    ch->alpha = emg_env_alpha(EMG_ENV_CUTOFF_HZ, CTRL_TICK_US);
    ch->env = 0.0f;
    ch->active = false;
    ch->saturated = false;
    ch->calibrated = false;
    ch->above_ticks = 0u;
    ch->sat_ticks = 0u;
    ch->thr_on = 0.0f;
    ch->thr_off = 0.0f;
    ch->env_at_max = 0.0f;
}

// Actualiza el contador de saturación con la muestra cruda (antes de rectificar).
static void emg_track_saturation(emg_channel_t *ch, uint16_t raw) {
    if (raw > EMG_SAT_COUNTS) {
        if (ch->sat_ticks < emg_ms_to_ticks(EMG_SAT_MS)) {
            ch->sat_ticks++;
        }
        if (ch->sat_ticks >= emg_ms_to_ticks(EMG_SAT_MS)) {
            ch->saturated = true;
        }
    } else {
        ch->sat_ticks = 0u;
        ch->saturated = false;
    }
}

// Rectifica la muestra si la señal no viene ya rectificada del hardware.
static float emg_rectify(uint16_t raw) {
#if EMG_RAW_SIGNAL
    const float d = (float)raw - (float)EMG_OFFSET_COUNTS;
    return (d < 0.0f) ? -d : d;
#else
    return (float)raw;
#endif
}

// Resuelve la activación con histéresis y tiempo mínimo sobre el umbral.
static void emg_update_active(emg_channel_t *ch) {
    if (!ch->calibrated) {
        return; // sin umbrales válidos el canal nunca se activa
    }
    if (!ch->active) {
        if (ch->env >= ch->thr_on) {
            if (ch->above_ticks < emg_ms_to_ticks(EMG_ON_MIN_MS)) {
                ch->above_ticks++;
            }
            if (ch->above_ticks >= emg_ms_to_ticks(EMG_ON_MIN_MS)) {
                ch->active = true;
            }
        } else {
            ch->above_ticks = 0u;
        }
    } else if (ch->env < ch->thr_off) {
        ch->active = false;
        ch->above_ticks = 0u;
    }
}

float emg_update(emg_channel_t *ch, uint16_t raw) {
    emg_track_saturation(ch, raw);
    const float x = emg_rectify(raw);
    ch->env += ch->alpha * (x - ch->env);
    emg_update_active(ch);
    return ch->env;
}

bool emg_is_active(const emg_channel_t *ch) {
    return ch->active;
}

float emg_intensity(const emg_channel_t *ch) {
    if (!ch->calibrated || ch->env_at_max <= ch->thr_on) {
        return 0.0f;
    }
    return clampf(mapf(ch->env, ch->thr_on, ch->env_at_max, 0.0f, 1.0f), 0.0f, 1.0f);
}

bool emg_is_saturated(const emg_channel_t *ch) {
    return ch->saturated;
}

float emg_envelope(const emg_channel_t *ch) {
    return ch->env;
}

float emg_threshold_on(const emg_channel_t *ch) {
    return ch->calibrated ? ch->thr_on : 0.0f;
}

void emg_set_thresholds(emg_channel_t *ch, float thr_on, float thr_off) {
    ch->thr_on = thr_on;
    ch->thr_off = thr_off;
    ch->calibrated = true;
}

void emg_set_max(emg_channel_t *ch, float env_at_max) {
    ch->env_at_max = env_at_max;
}
