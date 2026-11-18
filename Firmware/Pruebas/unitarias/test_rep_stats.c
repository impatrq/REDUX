// test_rep_stats.c · Pruebas de la estadística por repetición (lógica pura). Cubre
// ciclos completos (conteo, rango, asistencia y duración), ruido por debajo de la zona
// muerta, un cambio de dirección menor que la zona muerta (no debe partir la rep), el
// caso sin motor (asistencia 0) y el descarte por rango o duración insuficientes.

#include "unity.h"

#include "config.h"

#include "rep_stats.h"

void setUp(void) {}
void tearDown(void) {}

// Alimenta un tramo lineal [desde -> hasta] a `step` grados por muestra y `dt` ms por
// muestra, con un dientecito de ±`noise` grados alternado para simular ruido. Guarda las
// repeticiones emitidas en out[]/n. No incluye la muestra inicial `desde`.
static void ramp(rep_stats_t *rs, uint32_t *ms, uint32_t dt, float desde, float hasta,
                 float step, float noise, float duty, bool motor, rep_t out[], int *n) {
    const float dir = (hasta >= desde) ? step : -step;
    const float span = (hasta >= desde) ? (hasta - desde) : (desde - hasta);
    const int steps = (int)(span / step + 0.5f);
    float base = desde;
    for (int i = 0; i < steps; ++i) {
        base += dir;
        const float a = base + (((i & 1) != 0) ? noise : -noise);
        *ms += dt;
        rep_t r;
        if (rep_stats_sample(rs, a, duty, motor, *ms, &r)) {
            out[(*n)++] = r;
        }
    }
}

// Alimenta `cycles` triángulos (tope -> fondo -> tope) y cierra la rep en curso. Devuelve
// cuántas reps se emitieron. Reposo inicial en `tope`.
static int run_cycles(int cycles, float tope, float fondo, float step, uint32_t dt, float noise,
                      float duty, bool motor, rep_t out[]) {
    rep_stats_t rs;
    rep_stats_init(&rs);
    int n = 0;
    uint32_t ms = 0u;
    rep_t tmp;
    rep_stats_sample(&rs, tope, duty, motor, ms, &tmp); // referencia inicial en el tope
    for (int c = 0; c < cycles; ++c) {
        ramp(&rs, &ms, dt, tope, fondo, step, noise, duty, motor, out, &n);
        ramp(&rs, &ms, dt, fondo, tope, step, noise, duty, motor, out, &n);
    }
    if (rep_stats_finish(&rs, ms, &tmp)) {
        out[n++] = tmp;
    }
    return n;
}

// --- Ciclos completos: conteo, rango, asistencia y duración -------------------
void test_full_cycles_count_values(void) {
    rep_t reps[32];
    // 4 ciclos de 90° (100->10->100), 1°/muestra, 5 ms/muestra, duty 0.25 con motor.
    const int n = run_cycles(4, 100.0f, 10.0f, 1.0f, 5u, 0.0f, 0.25f, true, reps);

    // K ciclos -> K-1 reps (la primera flexión recién arranca tras el primer pico).
    TEST_ASSERT_EQUAL_INT(3, n);
    for (int i = 0; i < n; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, reps[i].rango_grados);
        // asistencia = media(duty)/DUTY_CEIL*100 = 0.25/0.5*100 = 50 %.
        TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, reps[i].asistencia_pct);
        // duración ≈ un ciclo completo = 180 muestras * 5 ms = 900 ms.
        TEST_ASSERT_UINT32_WITHIN(20u, 900u, reps[i].duracion_ms);
    }
}

// --- Sin motor: asistencia 0 --------------------------------------------------
void test_no_motor_assist_zero(void) {
    rep_t reps[32];
    const int n = run_cycles(4, 100.0f, 10.0f, 1.0f, 5u, 0.0f, 0.5f, false, reps);
    TEST_ASSERT_EQUAL_INT(3, n);
    for (int i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, reps[i].asistencia_pct);
        TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, reps[i].rango_grados);
    }
}

// --- Ruido por debajo de la zona muerta: no genera reps de más ----------------
void test_noise_below_deadband_no_extra_reps(void) {
    rep_t reps[32];
    // Ruido de ±1° (< REP_DIR_DEADBAND_DEG = 3°) sobre 3 ciclos: siguen siendo 2 reps.
    const int n = run_cycles(3, 100.0f, 10.0f, 1.0f, 5u, 1.0f, 0.25f, true, reps);
    TEST_ASSERT_EQUAL_INT(2, n);
    for (int i = 0; i < n; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(4.0f, 90.0f, reps[i].rango_grados);
    }
}

// --- Cambio de dirección menor que la zona muerta: no parte la rep ------------
void test_small_reversal_does_not_split(void) {
    rep_stats_t rs;
    rep_stats_init(&rs);
    rep_t reps[32];
    int n = 0;
    uint32_t ms = 0u;
    rep_t tmp;
    rep_stats_sample(&rs, 100.0f, 0.25f, true, ms, &tmp); // ref en el tope

    // Ciclo 1 (sin reps aún). Bajada con un dientecito de 2° (< 3°) en el medio.
    ramp(&rs, &ms, 5u, 100.0f, 50.0f, 1.0f, 0.0f, 0.25f, true, reps, &n);
    ramp(&rs, &ms, 5u, 50.0f, 52.0f, 1.0f, 0.0f, 0.25f, true, reps, &n); // reversa de 2°
    ramp(&rs, &ms, 5u, 52.0f, 10.0f, 1.0f, 0.0f, 0.25f, true, reps, &n);
    ramp(&rs, &ms, 5u, 10.0f, 100.0f, 1.0f, 0.0f, 0.25f, true, reps, &n);
    // Ciclo 2: abre y (al cerrar con finish) deja exactamente una rep.
    ramp(&rs, &ms, 5u, 100.0f, 10.0f, 1.0f, 0.0f, 0.25f, true, reps, &n);
    ramp(&rs, &ms, 5u, 10.0f, 100.0f, 1.0f, 0.0f, 0.25f, true, reps, &n);
    if (rep_stats_finish(&rs, ms, &tmp)) {
        reps[n++] = tmp;
    }

    TEST_ASSERT_EQUAL_INT(1, n); // el dientecito de 2° no generó una rep extra
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, reps[0].rango_grados);
}

// --- Descarte por rango o duración insuficientes ------------------------------
void test_discard_small_range(void) {
    rep_t reps[32];
    // Amplitud de 4° (< REP_MIN_RANGE_DEG = 5°): ninguna rep válida.
    const int n = run_cycles(3, 100.0f, 96.0f, 0.5f, 5u, 0.0f, 0.25f, true, reps);
    TEST_ASSERT_EQUAL_INT(0, n);
}

void test_discard_short_duration(void) {
    rep_t reps[32];
    // 90° de amplitud pero 1 ms/muestra: cada rep dura ~180 ms (< REP_MIN_DURATION_MS).
    const int n = run_cycles(3, 100.0f, 10.0f, 1.0f, 1u, 0.0f, 0.25f, true, reps);
    TEST_ASSERT_EQUAL_INT(0, n);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_cycles_count_values);
    RUN_TEST(test_no_motor_assist_zero);
    RUN_TEST(test_noise_below_deadband_no_extra_reps);
    RUN_TEST(test_small_reversal_does_not_split);
    RUN_TEST(test_discard_small_range);
    RUN_TEST(test_discard_short_duration);
    return UNITY_END();
}
