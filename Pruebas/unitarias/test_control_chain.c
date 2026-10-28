// test_control_chain.c · Simulación encadenada intent -> limits -> power_target ->
// power_step con un modelo de brazo simple. Verifica dos escenarios de la Fase 5:
//   1. Una flexión que llega al límite corta en ese mismo tick (duty 0) y no se pasa.
//   2. Una co-contracción en marcha corta el motor al instante.

#include "unity.h"

#include "config.h" // DUTY_MIN, DUTY_MAX
#include "intent.h"
#include "limits.h"
#include "power.h"

// Rango de trabajo de ejemplo (0° = flexión máxima, el ángulo crece al extender).
#define LO_DEG 5.0f
#define HI_DEG 125.0f

// Modelo de planta muy simple: grados que avanza el brazo por tick a duty 1.0. Solo
// para cerrar el lazo en la simulación; no pretende ser dinámica real.
#define ANGLE_PER_DUTY_DEG 0.2f

void setUp(void) {}
void tearDown(void) {}

static direction_t dir_from_intent(intent_t in) {
    switch (in) {
    case INTENT_FLEX:
        return DIR_FLEX;
    case INTENT_EXT:
        return DIR_EXT;
    default: // INTENT_REST o INTENT_COCONTRACTION
        return DIR_STOP;
    }
}

// Un ciclo de control completo: decide intención, factor de límites, duty objetivo,
// avanza la rampa (con corte inmediato por co-contracción o límite) y mueve el brazo
// según el duty realmente aplicado. Devuelve el hard_stop usado en este tick.
static bool chain_tick(power_state_t *ps, float *angle_deg, bool bic_on, bool tri_on,
                       float intensity) {
    const intent_t in = intent_decide(bic_on, tri_on);
    const direction_t dir = dir_from_intent(in);
    const float factor = limits_factor(dir, *angle_deg, LO_DEG, HI_DEG);
    const float target = power_target(intensity, factor);
    const bool hard_stop =
        (in == INTENT_COCONTRACTION) || limits_must_stop(dir, *angle_deg, LO_DEG, HI_DEG);

    power_step(ps, dir, target, hard_stop);

    if (ps->dir == DIR_FLEX) {
        *angle_deg -= ps->duty * ANGLE_PER_DUTY_DEG;
    } else if (ps->dir == DIR_EXT) {
        *angle_deg += ps->duty * ANGLE_PER_DUTY_DEG;
    }
    return hard_stop;
}

// Escenario 1: flexión sostenida que llega al límite inferior.
void test_flexion_stops_at_limit_same_tick(void) {
    power_state_t ps;
    power_init(&ps);
    float angle = 40.0f;

    // Flexiona (solo bíceps, intensidad plena) hasta que el límite obliga a cortar.
    bool cut = false;
    int guard = 0;
    while (!cut) {
        cut = chain_tick(&ps, &angle, true, false, 1.0f);
        TEST_ASSERT_TRUE(++guard < 200000);
    }

    // En el tick del corte, el duty es 0 y el sentido queda en STOP.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);
    TEST_ASSERT_EQUAL_INT(DIR_STOP, ps.dir);

    // Llegó al límite y no se pasó más que el avance de un tick (no se desboca).
    TEST_ASSERT_TRUE(angle <= LO_DEG + 1e-3f);
    TEST_ASSERT_TRUE(angle >= LO_DEG - 0.5f);

    // Con el bíceps aún activo, el motor sigue cortado y el brazo no avanza más.
    const float angle_at_cut = angle;
    for (int i = 0; i < 500; ++i) {
        chain_tick(&ps, &angle, true, false, 1.0f);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, angle_at_cut, angle);
}

// Escenario 2: co-contracción con el motor en marcha -> corte inmediato.
void test_cocontraction_cuts_while_running(void) {
    power_state_t ps;
    power_init(&ps);
    float angle = 60.0f; // lejos de ambos límites

    // Arranca la flexión y deja que la rampa suba un rato.
    for (int i = 0; i < 400; ++i) {
        chain_tick(&ps, &angle, true, false, 1.0f);
    }
    TEST_ASSERT_TRUE(ps.duty > 0.0f); // el motor está en marcha
    TEST_ASSERT_EQUAL_INT(DIR_FLEX, ps.dir);

    // Ambos músculos activos: co-contracción -> corte al instante en este mismo tick.
    const bool hard = chain_tick(&ps, &angle, true, true, 1.0f);
    TEST_ASSERT_TRUE(hard);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);
    TEST_ASSERT_EQUAL_INT(DIR_STOP, ps.dir);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flexion_stops_at_limit_same_tick);
    RUN_TEST(test_cocontraction_cuts_while_running);
    return UNITY_END();
}
