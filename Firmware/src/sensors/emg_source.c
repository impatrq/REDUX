// emg_source.c · Implementación de la fuente de EMG por canal (ver emg_source.h).

#include "emg_source.h"

#include <stdlib.h> // atoi

static int clamp_pct(int pct) {
    if (pct < 0) {
        return 0;
    }
    if (pct > 100) {
        return 100;
    }
    return pct;
}

void emg_source_init(emg_source_t *s, emg_src_t bic_src, emg_src_t tri_src) {
    s->bic_src = bic_src;
    s->tri_src = tri_src;
    s->bic_pct = 0;
    s->tri_pct = 0;
}

bool emg_source_bic_is_sim(const emg_source_t *s) {
    return s->bic_src == EMG_SRC_SERIAL;
}

bool emg_source_tri_is_sim(const emg_source_t *s) {
    return s->tri_src == EMG_SRC_SERIAL;
}

float emg_source_bic_level(const emg_source_t *s) {
    return emg_source_bic_is_sim(s) ? (float)s->bic_pct / 100.0f : 0.0f;
}

float emg_source_tri_level(const emg_source_t *s) {
    return emg_source_tri_is_sim(s) ? (float)s->tri_pct / 100.0f : 0.0f;
}

emg_cmd_result_t emg_source_feed(emg_source_t *s, const char *line) {
    // "r": reposo de los canales simulados (no toca un canal ADC).
    if (line[0] == 'r' && line[1] == '\0') {
        if (emg_source_bic_is_sim(s)) {
            s->bic_pct = 0;
        }
        if (emg_source_tri_is_sim(s)) {
            s->tri_pct = 0;
        }
        return EMG_CMD_OK;
    }
    // "b N" / "t N": intensidad simulada de un canal, solo si ese canal es SERIAL.
    if ((line[0] == 'b' || line[0] == 't') && line[1] == ' ') {
        const int pct = clamp_pct(atoi(&line[2]));
        if (line[0] == 'b') {
            if (!emg_source_bic_is_sim(s)) {
                return EMG_CMD_BIC_IS_ADC;
            }
            s->bic_pct = pct;
        } else {
            if (!emg_source_tri_is_sim(s)) {
                return EMG_CMD_TRI_IS_ADC;
            }
            s->tri_pct = pct;
        }
        return EMG_CMD_OK;
    }
    return EMG_CMD_NONE;
}
