// hal_time.c · Implementación del tick de 1 ms y del tiempo en µs. Único lugar,
// junto al resto de hal/ y main.c, que toca el hardware (SDK).

#include "hal_time.h"

#include "pico/stdlib.h"
#include "pico/time.h"

#include "config.h"

// La ISR solo toca estas dos variables volatile: incrementa el contador y levanta
// la bandera. Nada de lógica pesada dentro de la interrupción.
static volatile uint32_t g_ticks = 0;
static volatile bool g_tick_flag = false;

static repeating_timer_t g_timer;
static bool g_started = false;

static bool tick_isr(repeating_timer_t *rt) {
    (void)rt;
    g_ticks++;
    g_tick_flag = true;
    return true; // seguir repitiendo
}

void hal_time_init(void) {
    if (g_started) {
        return;
    }
    // Período negativo: el inicio de un disparo al inicio del siguiente (período
    // fijo), independiente de cuánto tarde la ISR.
    add_repeating_timer_us(-(int64_t)CTRL_TICK_US, tick_isr, NULL, &g_timer);
    g_started = true;
}

uint32_t hal_time_ticks(void) {
    return g_ticks;
}

bool hal_time_tick_take(void) {
    if (!g_tick_flag) {
        return false;
    }
    g_tick_flag = false;
    return true;
}

uint64_t hal_time_us(void) {
    return time_us_64();
}
