// emg_check.c · Programa de banco del canal EMG (no es el firmware). Ejecutable aparte
// (target emg_check) para verificar el canal EMG y su calibración. Sirve con el EMG
// real o con un potenciómetro en GP26/GP27 simulando la envolvente. El driver del motor
// queda DESHABILITADO siempre. Hace el sobremuestreo de EMG_OVERSAMPLE lecturas por canal
// y por tick (lo que en el firmware hará app_tick) y pasa la muestra promediada a emg.
// Vuelca un CSV cada FASE4_REPORT_MS con envolvente, activo e intensidad de ambos canales.
// Comandos por el monitor serie:
//   "rest"  -> calibra el reposo de ambos canales (ambos músculos relajados)
//   "mvc b" -> calibra la contracción máxima del bíceps
//   "mvc t" -> calibra la contracción máxima del tríceps

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "board.h"
#include "bts7960.h"
#include "config.h"
#include "emg.h"
#include "emg_calib.h"
#include "hal_adc.h"
#include "hal_time.h"

// Qué toma de calibración está corriendo (una por vez).
typedef enum { CAL_NONE, CAL_REST, CAL_MVC_B, CAL_MVC_T } cal_mode_t;

static emg_channel_t g_ch_b; // bíceps  (ADC0)
static emg_channel_t g_ch_t; // tríceps (ADC1)
static emg_calib_t g_cal_b;
static emg_calib_t g_cal_t;
static cal_mode_t g_mode = CAL_NONE;

// Sobremuestreo: promedia EMG_OVERSAMPLE lecturas crudas del canal ADC indicado.
static uint16_t read_oversampled(uint8_t adc_channel) {
    uint32_t acc = 0u;
    for (uint32_t i = 0u; i < EMG_OVERSAMPLE; ++i) {
        acc += hal_adc_read(adc_channel);
    }
    return (uint16_t)(acc / EMG_OVERSAMPLE);
}

// Describe por USB el resultado de una toma de calibración ya resuelta.
static void announce_calib(const char *who, const emg_calib_t *c) {
    if (emg_calib_is_done(c)) {
        printf("[cal] %s OK: thr_on=%.1f env_max=%.1f\n", who, (double)c->thr_on,
               (double)c->env_at_max);
    } else if (emg_calib_failed(c)) {
        switch (c->error) {
        case EMG_CAL_ERR_REST_NOISE:
            printf("[cal] %s ERROR: ruido en reposo (electrodo flojo). Reintente 'rest'.\n",
                   who);
            break;
        case EMG_CAL_ERR_WEAK_MVC:
            printf("[cal] %s ERROR: contraccion debil. Reintente el MVC.\n", who);
            break;
        case EMG_CAL_ERR_NONE:
        default:
            printf("[cal] %s ERROR.\n", who);
            break;
        }
    }
}

static void start_rest(void) {
    emg_calib_rest_begin(&g_cal_b, &g_ch_b);
    emg_calib_rest_begin(&g_cal_t, &g_ch_t);
    g_mode = CAL_REST;
    printf("[cal] Reposo: relaje ambos musculos y sostenga %u ms...\n", CAL_REST_MS);
}

static void start_mvc(cal_mode_t which) {
    emg_channel_t *ch = (which == CAL_MVC_B) ? &g_ch_b : &g_ch_t;
    emg_calib_t *c = (which == CAL_MVC_B) ? &g_cal_b : &g_cal_t;
    const char *who = (which == CAL_MVC_B) ? "biceps" : "triceps";
    if (!ch->calibrated) {
        printf("[cal] %s: calibre primero el reposo con 'rest'.\n", who);
        return;
    }
    emg_calib_mvc_begin(c, ch);
    g_mode = which;
    printf("[cal] MVC %s: contraiga al maximo y sostenga %u ms...\n", who, CAL_MVC_MS);
}

// Alimenta la toma activa con la envolvente de este tick y, al resolverse, informa.
static void feed_calibration(float env_b, float env_t) {
    switch (g_mode) {
    case CAL_REST:
        emg_calib_add(&g_cal_b, env_b);
        emg_calib_add(&g_cal_t, env_t);
        if (g_cal_b.phase != EMG_CAL_REST_RUN && g_cal_t.phase != EMG_CAL_REST_RUN) {
            announce_calib("biceps reposo", &g_cal_b);
            announce_calib("triceps reposo", &g_cal_t);
            g_mode = CAL_NONE;
        }
        break;
    case CAL_MVC_B:
        emg_calib_add(&g_cal_b, env_b);
        if (g_cal_b.phase != EMG_CAL_MVC_RUN) {
            announce_calib("biceps", &g_cal_b);
            g_mode = CAL_NONE;
        }
        break;
    case CAL_MVC_T:
        emg_calib_add(&g_cal_t, env_t);
        if (g_cal_t.phase != EMG_CAL_MVC_RUN) {
            announce_calib("triceps", &g_cal_t);
            g_mode = CAL_NONE;
        }
        break;
    case CAL_NONE:
    default:
        break;
    }
}

static void process_line(const char *line) {
    if (strcmp(line, "rest") == 0) {
        start_rest();
    } else if (strcmp(line, "mvc b") == 0) {
        start_mvc(CAL_MVC_B);
    } else if (strcmp(line, "mvc t") == 0) {
        start_mvc(CAL_MVC_T);
    } else if (line[0] != '\0') {
        printf("[cmd] use: rest | mvc b | mvc t\n");
    }
}

// Lee un carácter no bloqueante y arma la línea; al completarla, la despacha.
static void poll_serial(char *line, uint32_t *len) {
    const int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) {
        return;
    }
    if (c == '\n' || c == '\r') {
        line[*len] = '\0';
        process_line(line);
        *len = 0u;
    } else if (*len < FASE4_CMD_LINE_MAX - 1u) {
        line[(*len)++] = (char)c;
    } else {
        *len = 0u; // línea demasiado larga: descartar
    }
}

static void print_csv_row(uint64_t t_ms) {
    printf("%" PRIu64 ",%.1f,%d,%.3f,%d,%.1f,%d,%.3f,%d\n", t_ms,
           (double)emg_envelope(&g_ch_b), (int)emg_is_active(&g_ch_b),
           (double)emg_intensity(&g_ch_b), (int)emg_is_saturated(&g_ch_b),
           (double)emg_envelope(&g_ch_t), (int)emg_is_active(&g_ch_t),
           (double)emg_intensity(&g_ch_t), (int)emg_is_saturated(&g_ch_t));
}

int main(void) {
    stdio_init_all();
    hal_time_init();
    hal_adc_init();
    bts7960_init();        // arranca deshabilitado y en duty 0
    bts7960_enable(false); // el motor NO se usa en la Fase 4: queda deshabilitado

    emg_init(&g_ch_b);
    emg_init(&g_ch_t);

    printf("REDUX emg_check: comandos 'rest', 'mvc b', 'mvc t'. Motor deshabilitado.\n");
    printf("t_ms,env_b,act_b,int_b,sat_b,env_t,act_t,int_t,sat_t\n");

    char line[FASE4_CMD_LINE_MAX];
    uint32_t len = 0u;
    uint64_t last_report_us = hal_time_us();

    while (true) {
        poll_serial(line, &len);

        // Trabajo de cadencia fija (1 ms): sobremuestreo, envolvente y calibración.
        if (hal_time_tick_take()) {
            const uint16_t raw_b = read_oversampled(ADC_CH_BICEPS);
            const uint16_t raw_t = read_oversampled(ADC_CH_TRICEPS);
            const float env_b = emg_update(&g_ch_b, raw_b);
            const float env_t = emg_update(&g_ch_t, raw_t);
            feed_calibration(env_b, env_t);
        }

        // Volcado CSV periódico por USB.
        const uint64_t now_us = hal_time_us();
        if ((now_us - last_report_us) >= (FASE4_REPORT_MS * 1000u)) {
            last_report_us = now_us;
            print_csv_row(now_us / 1000u);
        }
    }
}
