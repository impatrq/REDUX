// emg_calib.c · Implementación de la calibración de reposo y de contracción máxima.

#include "emg_calib.h"

#include <math.h>

#include "config.h"

// Convierte una duración en ms a una cantidad entera de muestras (una por tick).
static uint32_t emg_calib_samples(uint32_t ms) {
    return (ms * 1000u) / CTRL_TICK_US;
}

void emg_calib_rest_begin(emg_calib_t *c, emg_channel_t *ch) {
    c->ch = ch;
    c->phase = EMG_CAL_REST_RUN;
    c->error = EMG_CAL_ERR_NONE;
    c->n = 0u;
    c->n_target = emg_calib_samples(CAL_REST_MS);
    c->sum = 0.0;
    c->sum_sq = 0.0;
    c->peak = 0.0f;
    c->rest_mean = 0.0f;
    c->rest_std = 0.0f;
    c->thr_on = 0.0f;
    c->env_at_max = 0.0f;
}

void emg_calib_mvc_begin(emg_calib_t *c, emg_channel_t *ch) {
    c->ch = ch;
    c->phase = EMG_CAL_MVC_RUN;
    c->error = EMG_CAL_ERR_NONE;
    c->n = 0u;
    c->n_target = emg_calib_samples(CAL_MVC_MS);
    c->peak = 0.0f;
}

// Cierra la toma de reposo: media, desvío, umbrales y validación de ruido.
static void emg_calib_finish_rest(emg_calib_t *c) {
    const double n = (double)c->n;
    const double mean = c->sum / n;
    double var = c->sum_sq / n - mean * mean;
    if (var < 0.0) {
        var = 0.0; // puede dar levemente negativo por redondeo
    }
    const float std = (float)sqrt(var);

    c->rest_mean = (float)mean;
    c->rest_std = std;

    if (std > EMG_REST_NOISE_MAX) {
        c->phase = EMG_CAL_FAILED;
        c->error = EMG_CAL_ERR_REST_NOISE;
        return;
    }

    float margin = EMG_K_NOISE * std;
    if (margin < EMG_MARGIN_MIN) {
        margin = EMG_MARGIN_MIN;
    }
    c->thr_on = (float)mean + margin;
    const float thr_off = (float)mean + margin * EMG_OFF_FRAC;
    emg_set_thresholds(c->ch, c->thr_on, thr_off);
    c->phase = EMG_CAL_DONE;
}

// Cierra la toma de MVC: máximo relativo al reposo y validación de contracción débil.
static void emg_calib_finish_mvc(emg_calib_t *c) {
    const float above_peak = c->peak - c->rest_mean;
    const float above_thr = c->thr_on - c->rest_mean;

    // Débil si el pico sobre el reposo no supera el umbral sobre el reposo por el factor.
    if (above_peak < EMG_MVC_MIN_RATIO * above_thr) {
        c->phase = EMG_CAL_FAILED;
        c->error = EMG_CAL_ERR_WEAK_MVC;
        return;
    }

    const float env_at_max = c->rest_mean + EMG_MVC_USE * above_peak;
    if (env_at_max <= c->thr_on) {
        c->phase = EMG_CAL_FAILED;
        c->error = EMG_CAL_ERR_WEAK_MVC;
        return;
    }

    c->env_at_max = env_at_max;
    emg_set_max(c->ch, env_at_max);
    c->phase = EMG_CAL_DONE;
}

void emg_calib_add(emg_calib_t *c, float env) {
    if (c->phase == EMG_CAL_REST_RUN) {
        c->sum += (double)env;
        c->sum_sq += (double)env * (double)env;
        c->n++;
        if (c->n >= c->n_target) {
            emg_calib_finish_rest(c);
        }
    } else if (c->phase == EMG_CAL_MVC_RUN) {
        if (env > c->peak) {
            c->peak = env;
        }
        c->n++;
        if (c->n >= c->n_target) {
            emg_calib_finish_mvc(c);
        }
    }
}

bool emg_calib_is_done(const emg_calib_t *c) {
    return c->phase == EMG_CAL_DONE;
}

bool emg_calib_failed(const emg_calib_t *c) {
    return c->phase == EMG_CAL_FAILED;
}
