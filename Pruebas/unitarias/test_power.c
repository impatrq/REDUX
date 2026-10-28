// test_power.c · Pruebas de power: power_target (factor 0, intensidad 0 y 1, techo
// DUTY_MAX) y power_step (rampa de subida y bajada, inversión de sentido que pasa por 0,
// hard_stop inmediato, duty nunca mayor a DUTY_MAX). Fase 5.

#include "unity.h"

#include "config.h" // DUTY_MIN, DUTY_MAX, RAMP_*_PER_S, CTRL_TICK_US
#include "power.h"

// Paso de duty por tick esperado para cada pendiente (debe coincidir con power.c).
#define TICK_S ((float)CTRL_TICK_US / 1000000.0f)
#define UP_STEP (RAMP_UP_PER_S * TICK_S)
#define DOWN_STEP (RAMP_DOWN_PER_S * TICK_S)

void setUp(void) {}
void tearDown(void) {}

// --- power_target ------------------------------------------------------------

// factor <= 0 -> 0.0 (NO DUTY_MIN: el sentido está bloqueado).
void test_target_factor_zero_is_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, power_target(1.0f, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, power_target(1.0f, -0.5f)); // negativo también
}

// intensidad 0 con factor pleno -> DUTY_MIN (piso de arranque).
void test_target_intensity_zero_is_duty_min(void) {
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MIN, power_target(0.0f, 1.0f));
}

// intensidad 1 con factor pleno -> DUTY_MAX.
void test_target_intensity_full_is_duty_max(void) {
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, power_target(1.0f, 1.0f));
}

// Entradas desbordadas: el duty nunca supera DUTY_MAX.
void test_target_never_exceeds_duty_max(void) {
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, power_target(5.0f, 5.0f));
    TEST_ASSERT_TRUE(power_target(2.0f, 2.0f) <= DUTY_MAX);
}

// --- power_step --------------------------------------------------------------

// Rampa de subida: sube como máximo UP_STEP por tick y converge al objetivo.
void test_step_ramp_up(void) {
    power_state_t ps;
    power_init(&ps);

    power_step(&ps, DIR_FLEX, DUTY_MAX, false);
    TEST_ASSERT_EQUAL_INT(DIR_FLEX, ps.dir);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, UP_STEP, ps.duty);

    float prev = ps.duty;
    for (int i = 0; i < 5000; ++i) {
        power_step(&ps, DIR_FLEX, DUTY_MAX, false);
        TEST_ASSERT_TRUE(ps.duty - prev <= UP_STEP + 1e-6f); // nunca sube más que el paso
        TEST_ASSERT_TRUE(ps.duty <= DUTY_MAX);               // nunca supera el techo
        prev = ps.duty;
    }
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, ps.duty); // converge a DUTY_MAX
}

// Rampa de bajada: baja como máximo DOWN_STEP por tick hasta 0.
void test_step_ramp_down(void) {
    power_state_t ps = {DIR_FLEX, 0.5f};

    power_step(&ps, DIR_STOP, 0.0f, false);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f - DOWN_STEP, ps.duty);

    float prev = ps.duty;
    for (int i = 0; i < 5000; ++i) {
        power_step(&ps, DIR_STOP, 0.0f, false);
        TEST_ASSERT_TRUE(prev - ps.duty <= DOWN_STEP + 1e-6f);
        prev = ps.duty;
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);
    TEST_ASSERT_EQUAL_INT(DIR_STOP, ps.dir);
}

// Inversión FLEX -> EXT: baja a 0 en FLEX y recién ahí adopta EXT. Nunca salto directo.
void test_step_reversal_passes_through_zero(void) {
    power_state_t ps = {DIR_FLEX, 0.5f};

    // Mientras desciende, el sentido sigue siendo FLEX (nunca EXT con duty > 0).
    int guard = 0;
    while (ps.dir == DIR_FLEX) {
        power_step(&ps, DIR_EXT, 0.5f, false);
        TEST_ASSERT_TRUE(++guard < 100000);
    }
    // En el tick en que el sentido cambia a EXT, el duty pasó por 0.
    TEST_ASSERT_EQUAL_INT(DIR_EXT, ps.dir);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);

    // A partir de ahí arranca la rampa de subida en EXT.
    power_step(&ps, DIR_EXT, 0.5f, false);
    TEST_ASSERT_EQUAL_INT(DIR_EXT, ps.dir);
    TEST_ASSERT_TRUE(ps.duty > 0.0f);
}

// hard_stop: corta al instante, sin rampa.
void test_step_hard_stop_is_immediate(void) {
    power_state_t ps = {DIR_FLEX, 0.5f};
    power_step(&ps, DIR_FLEX, 0.5f, true);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);
    TEST_ASSERT_EQUAL_INT(DIR_STOP, ps.dir);
}

// Con un objetivo desbordado, el duty aplicado nunca supera DUTY_MAX en ningún tick.
void test_step_never_exceeds_duty_max(void) {
    power_state_t ps;
    power_init(&ps);
    for (int i = 0; i < 5000; ++i) {
        power_step(&ps, DIR_FLEX, 2.0f, false); // objetivo imposible
        TEST_ASSERT_TRUE(ps.duty <= DUTY_MAX);
    }
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, ps.duty);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_target_factor_zero_is_zero);
    RUN_TEST(test_target_intensity_zero_is_duty_min);
    RUN_TEST(test_target_intensity_full_is_duty_max);
    RUN_TEST(test_target_never_exceeds_duty_max);
    RUN_TEST(test_step_ramp_up);
    RUN_TEST(test_step_ramp_down);
    RUN_TEST(test_step_reversal_passes_through_zero);
    RUN_TEST(test_step_hard_stop_is_immediate);
    RUN_TEST(test_step_never_exceeds_duty_max);
    return UNITY_END();
}
