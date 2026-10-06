// main.c · Punto de entrada: inicializa el SDK y los módulos, corre el lazo principal
// esperando el tick de 1 ms y despacha un ciclo de control (app_tick) por tick. No
// contiene lógica de control. El reporte de estado por USB se arma aquí y se envía de
// a chunks, sin bloquear el lazo. La telemetría completa en core 1 es de la Fase 7.

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "app.h"
#include "config.h"
#include "hal_adc.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_time.h"
#include "ui_basic.h"

#define REDUX_FW_VERSION "0.6.0"

// --- Reporte de estado por USB, no bloqueante --------------------------------
static char g_rep[STATUS_LINE_MAX];
static size_t g_rep_len = 0u;
static size_t g_rep_pos = 0u;

static const char *state_name(sys_state_t s) {
    switch (s) {
    case ST_BOOT:
        return "BOOT";
    case ST_CHECK_ENC:
        return "CHECK_ENC";
    case ST_CALIB_ROM:
        return "CALIB_ROM";
    case ST_CALIB_REST:
        return "CALIB_REST";
    case ST_CALIB_MVC:
        return "CALIB_MVC";
    case ST_ARMING:
        return "ARMING";
    case ST_RUN:
        return "RUN";
    case ST_FAULT:
        return "FAULT";
    default:
        return "?";
    }
}

// Arma una línea de estado si venció el período y no hay otra línea drenándose.
// Mide su propio tiempo de armado (trep) y lo incluye: así queda a la vista que el
// reporte no entra en la medición del ciclo de control (que vive en app_tick).
static void report_build(uint64_t now_us, uint64_t *last_us) {
    if (g_rep_pos < g_rep_len) {
        return; // todavía se está enviando la línea anterior
    }
    if ((now_us - *last_us) < (STATUS_REPORT_MS * 1000u)) {
        return;
    }
    *last_us = now_us;

    const uint64_t build0 = hal_time_us();
    app_status_t s;
    app_get_status(&s);
    const int n = snprintf(
        g_rep, sizeof(g_rep),
        "st=%s th=%.1f bic=%.0f/%.2f tri=%.0f/%.2f dir=%d duty=%.2f F=0x%02X sim=%d trep=%lluus\n",
        state_name(s.state), (double)s.angle_deg, (double)s.env_bic, (double)s.intensity_bic,
        (double)s.env_tri, (double)s.intensity_tri, (int)s.dir, (double)s.duty,
        (unsigned)s.faults, (int)s.emg_simulated,
        (unsigned long long)(hal_time_us() - build0));

    g_rep_len = (n > 0 && (size_t)n < sizeof(g_rep)) ? (size_t)n : 0u;
    g_rep_pos = 0u;
}

// Envía hasta STATUS_TX_CHUNK bytes de la línea pendiente. putchar_raw respeta
// PICO_STDIO_USB_STDOUT_TIMEOUT_US (configurado bajo en el CMakeLists), así que una
// terminal abierta que no lee no bloquea el lazo ni dispara el watchdog.
static void report_drain(void) {
    uint32_t sent = 0u;
    while (g_rep_pos < g_rep_len && sent < STATUS_TX_CHUNK) {
        putchar_raw(g_rep[g_rep_pos++]);
        sent++;
    }
}

#if BENCH_MODE
// Diagnóstico temporal (solo con BENCH_MODE): cuelga el lazo sin alimentar el watchdog
// para verificar que la Pico se reinicia. Avisa por USB antes de colgarse. Si el
// watchdog ya está habilitado (tras armar), reinicia en ~WDT_TIMEOUT_MS; si no, queda
// colgada hasta un reset manual. Quitar antes de la versión final.
static void diag_hang(void) {
    printf("[diag] HANG: cuelgo el lazo sin alimentar el watchdog. Si esta armado, "
           "la Pico se reinicia en ~%u ms.\n",
           WDT_TIMEOUT_MS);
    stdio_flush();
    while (true) {
        tight_loop_contents();
    }
}
#endif

// Lee una línea de serie (no bloqueante) y la despacha: primero a la fuente de EMG
// simulada ("b N"/"t N"/"r"); si no la consume, a la interfaz ("ok"/"cancel"/"recal").
static void poll_serial(char *line, uint32_t *len) {
    const int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) {
        return;
    }
    if (c == '\n' || c == '\r') {
        line[*len] = '\0';
#if BENCH_MODE
        if (strcmp(line, "hang") == 0) {
            diag_hang(); // no retorna
        }
#endif
        if (*len > 0u && !app_feed_sim(line)) {
            ui_basic_feed_line(line);
        }
        *len = 0u;
    } else if (*len < APP_CMD_LINE_MAX - 1u) {
        line[(*len)++] = (char)c;
    } else {
        *len = 0u; // línea demasiado larga: descartar
    }
}

int main(void) {
    stdio_init_all();

    hal_gpio_init();     // enables del driver (no-op en esta PCB): arranque seguro
    hal_gpio_led_init(); // LED de estado
    hal_time_init();     // tick de 1 ms
    hal_adc_init();      // EMG por ADC (con EMG_SOURCE_ADC)
    hal_i2c_init();      // bus del AS5600
    ui_init();           // pulsador + LED + serie
    app_init();          // supervisor y módulos (no habilita el watchdog aún)

    printf("REDUX firmware v%s\n", REDUX_FW_VERSION);

    char line[APP_CMD_LINE_MAX];
    uint32_t len = 0u;
    uint64_t last_report_us = hal_time_us();
    uint32_t last_served = hal_time_ticks();

    while (true) {
        poll_serial(line, &len);

        if (hal_time_tick_take()) {
            const uint32_t now = hal_time_ticks();
            const bool behind = (now - last_served) > 1u; // ticks perdidos: lazo atrasado
            last_served = now;
            app_tick(behind);
        }

        // Reporte de estado: armado y envío fuera de la medición del ciclo de control.
        report_build(hal_time_us(), &last_report_us);
        report_drain();
    }
}
