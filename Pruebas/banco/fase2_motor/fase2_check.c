// fase2_check.c · Programa de banco de la Fase 2 (no es el firmware). Ejecutable aparte
// (target fase2_check) que maneja el BTS7960 con el motor SIN carga, por comandos del
// monitor serie:
//   "f N"  -> flexiona al N % (RPWM)
//   "e N"  -> extiende al N % (LPWM)
//   "s"    -> frena
//   "en 1" / "en 0" -> habilita / deshabilita el driver
// Protecciones del banco: N se limita a FASE2_DUTY_MAX_PCT; al invertir el sentido
// primero frena y espera FASE2_REVERSE_BRAKE_MS; si no llega ningún comando en
// FASE2_DEADMAN_MS frena solo (hombre muerto). Imprime el estado tras cada comando.

#include <inttypes.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "bts7960.h"
#include "config.h"
#include "hal_time.h"

static direction_t g_dir = DIR_STOP; // sentido aplicado actualmente
static uint32_t g_pct = 0u;          // duty actual en %
static bool g_enabled = false;       // estado del driver

static const char *dir_name(direction_t dir) {
    switch (dir) {
    case DIR_FLEX:
        return "FLEX";
    case DIR_EXT:
        return "EXT";
    case DIR_STOP:
    default:
        return "STOP";
    }
}

static void print_state(const char *ev) {
    printf("[%s] driver=%s dir=%s duty=%" PRIu32 "%%\n", ev, g_enabled ? "ON" : "OFF",
           dir_name(g_dir), g_pct);
}

// Aplica un sentido con su %, frenando y esperando antes de invertir si hace falta.
static void command_move(direction_t dir, uint32_t pct) {
    if (pct > FASE2_DUTY_MAX_PCT) {
        pct = FASE2_DUTY_MAX_PCT; // tope del banco
    }

    // Cambio de sentido con el motor en movimiento: frenar y esperar primero.
    if (g_dir != DIR_STOP && dir != g_dir) {
        bts7960_stop();
        sleep_ms(FASE2_REVERSE_BRAKE_MS);
    }

    bts7960_apply(dir, (float)pct / 100.0f);
    g_dir = dir;
    g_pct = pct;
}

static void command_stop(void) {
    bts7960_stop();
    g_dir = DIR_STOP;
    g_pct = 0u;
}

static void command_enable(bool on) {
    bts7960_enable(on);
    g_enabled = on;
    if (!on) {
        // enable(false) ya forzó duty 0 en el driver; reflejarlo en el estado.
        g_dir = DIR_STOP;
        g_pct = 0u;
    }
}

// Interpreta una línea completa y ejecuta el comando. Imprime el estado resultante.
static void process_line(const char *line) {
    int n = 0;
    if (sscanf(line, "en %d", &n) == 1) {
        command_enable(n != 0);
        print_state("en");
    } else if (sscanf(line, "f %d", &n) == 1 && n >= 0) {
        command_move(DIR_FLEX, (uint32_t)n);
        print_state("f");
    } else if (sscanf(line, "e %d", &n) == 1 && n >= 0) {
        command_move(DIR_EXT, (uint32_t)n);
        print_state("e");
    } else if (line[0] == 's') {
        command_stop();
        print_state("s");
    } else if (line[0] != '\0') {
        printf("comando desconocido: \"%s\" (use f N | e N | s | en 1 | en 0)\n", line);
    }
}

int main(void) {
    stdio_init_all();
    hal_time_init();
    bts7960_init(); // arranca deshabilitado, duty 0

    printf("REDUX fase2_check: f N | e N | s | en 1 | en 0  (N max %u%%, hombre muerto %u ms)\n",
           FASE2_DUTY_MAX_PCT, FASE2_DEADMAN_MS);
    print_state("init");

    char line[FASE2_CMD_LINE_MAX];
    uint32_t len = 0u;
    uint64_t last_cmd_us = hal_time_us();

    while (true) {
        const int c = getchar_timeout_us(0); // no bloqueante
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\n' || c == '\r') {
                line[len] = '\0';
                if (len > 0u) {
                    process_line(line);
                    last_cmd_us = hal_time_us(); // reinicia el hombre muerto
                }
                len = 0u;
            } else if (len < FASE2_CMD_LINE_MAX - 1u) {
                line[len++] = (char)c;
            } else {
                // Línea demasiado larga: descartar hasta el próximo fin de línea.
                len = 0u;
                printf("linea demasiado larga, descartada\n");
            }
        }

        // Hombre muerto: sin comandos en FASE2_DEADMAN_MS y el motor en marcha -> frenar.
        const uint64_t now_us = hal_time_us();
        if (g_dir != DIR_STOP && (now_us - last_cmd_us) >= (uint64_t)FASE2_DEADMAN_MS * 1000u) {
            command_stop();
            print_state("hombre-muerto");
            last_cmd_us = now_us;
        }
    }
}
