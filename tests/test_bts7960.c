// test_bts7960.c · Verifica el invariante del puente H: secuencias aleatorias de
// bts7960_apply nunca dejan dos duties positivos a la vez (mock de hal_pwm). Fase 2.
// Además: init arranca deshabilitado y en 0, stop deja ambos en 0, el duty se recorta
// a DUTY_MAX, y apply apaga el lado opuesto antes de escribir el activo.

#include <stdlib.h>

#include "unity.h"

#include "board.h"  // PIN_MOTOR_RPWM, PIN_MOTOR_LPWM
#include "bts7960.h"
#include "config.h" // DUTY_MAX
#include "mock_hal.h"

void setUp(void) {
    mock_hal_reset();
}

void tearDown(void) {}

// El invariante central: nunca hay duty > 0 en RPWM y LPWM al mismo tiempo.
static void assert_not_both_positive(void) {
    const float r = mock_pwm_last_duty(PIN_MOTOR_RPWM);
    const float l = mock_pwm_last_duty(PIN_MOTOR_LPWM);
    TEST_ASSERT_FALSE_MESSAGE(r > 0.0f && l > 0.0f, "RPWM y LPWM positivos a la vez");
}

// init arranca deshabilitado y con ambos PWM en 0.
void test_init_starts_disabled_and_zero(void) {
    bts7960_init();
    TEST_ASSERT_FALSE_MESSAGE(mock_motor_enabled(), "init debe arrancar deshabilitado");
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_LPWM));
}

// stop deja ambos lados en 0, incluso tras haber aplicado un sentido.
void test_stop_leaves_both_zero(void) {
    bts7960_init();
    bts7960_apply(DIR_FLEX, 0.5f);
    bts7960_stop();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_LPWM));
    assert_not_both_positive();
}

// enable(false) también fuerza duty 0 (los enable nunca caen con un duty activo).
void test_disable_forces_zero(void) {
    bts7960_init();
    bts7960_enable(true);
    bts7960_apply(DIR_EXT, 0.4f);
    bts7960_enable(false);
    TEST_ASSERT_FALSE(mock_motor_enabled());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_LPWM));
}

// El duty se recorta a DUTY_MAX (y nunca por debajo de 0).
void test_duty_is_clamped_to_max(void) {
    bts7960_init();

    bts7960_apply(DIR_FLEX, 2.0f); // muy por encima del techo
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_LPWM));

    bts7960_apply(DIR_EXT, 1.5f);
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, mock_pwm_last_duty(PIN_MOTOR_LPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));

    bts7960_apply(DIR_FLEX, -0.3f); // negativo -> 0
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
}

// apply escribe 0 en el lado opuesto ANTES de escribir el lado activo.
void test_apply_zeros_opposite_before_active(void) {
    bts7960_init();
    const uint32_t base = mock_pwm_write_count();

    bts7960_apply(DIR_FLEX, 0.5f); // activo = RPWM, opuesto = LPWM
    TEST_ASSERT_EQUAL_UINT32(base + 2u, mock_pwm_write_count());

    const mock_pwm_write_t first = mock_pwm_write_at(base);
    const mock_pwm_write_t second = mock_pwm_write_at(base + 1u);

    TEST_ASSERT_EQUAL_UINT32(PIN_MOTOR_LPWM, first.pin); // opuesto primero
    TEST_ASSERT_EQUAL_FLOAT(0.0f, first.duty);
    TEST_ASSERT_EQUAL_UINT32(PIN_MOTOR_RPWM, second.pin); // activo después
    TEST_ASSERT_EQUAL_FLOAT(0.5f, second.duty);
}

// DIR_STOP equivale a frenar.
void test_apply_stop_direction_brakes(void) {
    bts7960_init();
    bts7960_apply(DIR_FLEX, 0.6f);
    bts7960_apply(DIR_STOP, 0.6f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_LPWM));
}

// Secuencia larga de comandos aleatorios: el invariante se mantiene en todo momento.
void test_random_sequence_never_both_positive(void) {
    bts7960_init();
    srand(12345u); // semilla fija: la prueba es determinista y reproducible

    const direction_t dirs[] = {DIR_FLEX, DIR_EXT, DIR_STOP};
    for (int i = 0; i < 20000; ++i) {
        const direction_t dir = dirs[rand() % 3];
        const float duty = (float)(rand() % 150) / 100.0f; // 0.00 .. 1.49 (cruza DUTY_MAX)
        bts7960_apply(dir, duty);
        assert_not_both_positive();

        // Y el lado activo nunca supera DUTY_MAX.
        TEST_ASSERT_TRUE(mock_pwm_last_duty(PIN_MOTOR_RPWM) <= DUTY_MAX);
        TEST_ASSERT_TRUE(mock_pwm_last_duty(PIN_MOTOR_LPWM) <= DUTY_MAX);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_starts_disabled_and_zero);
    RUN_TEST(test_stop_leaves_both_zero);
    RUN_TEST(test_disable_forces_zero);
    RUN_TEST(test_duty_is_clamped_to_max);
    RUN_TEST(test_apply_zeros_opposite_before_active);
    RUN_TEST(test_apply_stop_direction_brakes);
    RUN_TEST(test_random_sequence_never_both_positive);
    return UNITY_END();
}
