// fase3_check.c · Programa de banco de la Fase 3 (no es el firmware). Ejecutable aparte
// (target fase3_check) para verificar el AS5600 y la calibración del rango. El driver del
// motor queda DESHABILITADO siempre. Comandos por el monitor serie:
//   "cal"   -> corre la calibración (pulsador u "ok" para confirmar; "recal" reintenta)
//   "raw"   -> vuelve al modo crudo
//   "ok" / "cancel" / "recal" -> eventos de la interfaz durante la calibración
// Modo "raw": imprime RAW, grados y los bits MD/ML/MH cada 200 ms.
// Tras calibrar: imprime θ, rom_deg y si está DENTRO, en ZONA DE LÍMITE o FUERA, 200 ms.

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "as5600.h"
#include "bts7960.h"
#include "config.h"
#include "hal_i2c.h"
#include "hal_time.h"
#include "rom_calib.h"
#include "ui_basic.h"

#define FASE3_REPORT_US 200000u  // período de impresión por USB: 200 ms
#define FASE3_CMD_LINE_MAX 32u   // largo máximo de una línea de comando por USB

typedef enum { MODE_RAW, MODE_CAL } mode_t;

static mode_t g_mode = MODE_RAW;
static rom_calib_t g_rc;
static rom_phase_t g_last_phase = ROM_IDLE;

// Imprime una lectura cruda con los bits del imán (modo raw).
static void print_raw(void) {
    as5600_magnet_t m;
    int32_t raw = 0;
    const status_t s1 = as5600_read_magnet(&m);
    const status_t s2 = as5600_read_raw(&raw);
    if (s1 != STATUS_OK || s2 != STATUS_OK) {
        printf("[raw] I2C error (magnet=%d raw=%d)\n", (int)s1, (int)s2);
        return;
    }
    printf("[raw] RAW=%" PRId32 " deg=%.1f MD=%d ML=%d MH=%d\n", raw,
           (double)as5600_counts_to_deg(raw), (int)m.detected, (int)m.too_weak,
           (int)m.too_strong);
}

// Clasifica θ respecto de los límites de trabajo (usa ROM_MARGIN_DEG como banda).
static const char *zone_name(float theta, float lo, float hi) {
    if (theta < lo || theta > hi) {
        return "FUERA";
    }
    if (theta < lo + ROM_MARGIN_DEG || theta > hi - ROM_MARGIN_DEG) {
        return "ZONA DE LIMITE";
    }
    return "DENTRO";
}

// Imprime el ángulo de trabajo y la zona (modo calibrado).
static void print_calibrated(void) {
    int32_t raw = 0;
    if (as5600_read_raw(&raw) != STATUS_OK) {
        printf("[cal] I2C error\n");
        return;
    }
    const float theta = rom_calib_angle_deg(&g_rc, raw);
    const float lo = rom_calib_limit_lo(&g_rc);
    const float hi = rom_calib_limit_hi(&g_rc);
    printf("[cal] theta=%.1f rom=%.1f [%s]\n", (double)theta, (double)g_rc.rom_deg,
           zone_name(theta, lo, hi));
}

// Describe por USB el motivo de una calibración fallida.
static void announce_failure(void) {
    switch (g_rc.error) {
    case ROM_ERR_MAGNET:
        ui_message("Calibracion invalida: iman fuera de rango (revise distancia). 'recal'.");
        break;
    case ROM_ERR_UNSTABLE:
        ui_message("Calibracion invalida: lectura inestable (sostenga la posicion). 'recal'.");
        break;
    case ROM_ERR_RANGE:
        ui_message("Calibracion invalida: recorrido fuera de rango. 'recal'.");
        break;
    case ROM_ERR_BUS:
        ui_message("Calibracion invalida: fallo del bus I2C. 'recal'.");
        break;
    case ROM_ERR_NONE:
    default:
        ui_message("Calibracion invalida. 'recal'.");
        break;
    }
}

// Reacciona a cada cambio de fase de la calibración con un mensaje y el LED.
static void announce_phase(void) {
    switch (g_rc.phase) {
    case ROM_WAIT_FLEX:
        ui_message("Lleve el brazo a FLEXION maxima y confirme (pulsador u 'ok').");
        ui_set_waiting(true);
        break;
    case ROM_WAIT_EXT:
        ui_message("Flexion OK. Lleve el brazo a EXTENSION maxima y confirme.");
        ui_set_waiting(true);
        break;
    case ROM_SAMPLING_FLEX:
    case ROM_SAMPLING_EXT:
        ui_set_waiting(false);
        ui_message("Midiendo, sostenga la posicion...");
        break;
    case ROM_DONE:
        ui_set_waiting(false);
        printf("Calibracion OK: rom=%.1f deg, sign=%" PRId32 ". Mostrando angulo.\n",
               (double)g_rc.rom_deg, g_rc.sign);
        break;
    case ROM_FAILED:
        ui_set_waiting(false);
        announce_failure();
        break;
    case ROM_IDLE:
    default:
        break;
    }
}

static void enter_cal_mode(void) {
    g_mode = MODE_CAL;
    rom_calib_begin(&g_rc);
    g_last_phase = ROM_IDLE; // fuerza el anuncio de WAIT_FLEX en el próximo paso
}

static void enter_raw_mode(void) {
    g_mode = MODE_RAW;
    ui_set_waiting(false);
    printf("Modo raw: RAW, grados y bits del iman cada 200 ms.\n");
}

static void process_line(const char *line) {
    if (line[0] == '\0') {
        return;
    }
    if (strcmp(line, "cal") == 0) {
        enter_cal_mode();
    } else if (strcmp(line, "raw") == 0) {
        enter_raw_mode();
    } else {
        ui_basic_feed_line(line); // "ok" / "cancel" / "recal"
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
    } else if (*len < FASE3_CMD_LINE_MAX - 1u) {
        line[(*len)++] = (char)c;
    } else {
        *len = 0u; // línea demasiado larga: descartar
    }
}

int main(void) {
    stdio_init_all();
    hal_time_init();
    hal_i2c_init();
    bts7960_init();        // arranca deshabilitado y en duty 0
    bts7960_enable(false); // el motor NO se usa en la Fase 3: queda deshabilitado
    ui_init();

    printf("REDUX fase3_check: 'cal' calibra, 'raw' vuelve a crudo. Motor deshabilitado.\n");
    enter_raw_mode();

    char line[FASE3_CMD_LINE_MAX];
    uint32_t len = 0u;
    uint64_t last_report_us = hal_time_us();

    while (true) {
        poll_serial(line, &len);

        // Trabajo de cadencia fija (1 ms): interfaz y, en calibración, el promediado.
        if (hal_time_tick_take()) {
            ui_tick();
            if (g_mode == MODE_CAL) {
                rom_calib_tick(&g_rc);
            }
        }

        // Eventos del usuario: solo importan durante la calibración.
        const ui_event_t ev = ui_poll();
        if (g_mode == MODE_CAL) {
            if (ev != UI_EV_NONE) {
                rom_calib_event(&g_rc, ev);
            }
            if (g_rc.phase != g_last_phase) {
                announce_phase();
                g_last_phase = g_rc.phase;
            }
        }

        // Reporte periódico por USB.
        const uint64_t now_us = hal_time_us();
        if ((now_us - last_report_us) >= FASE3_REPORT_US) {
            last_report_us = now_us;
            if (g_mode == MODE_RAW) {
                print_raw();
            } else if (rom_calib_is_ready(&g_rc)) {
                print_calibrated();
            }
        }
    }
}
