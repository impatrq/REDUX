// test_motor_output.c · Fase 6 (fix). Dos propiedades del camino de salida del motor:
//   1) bts7960_applied() devuelve el mismo sentido/duty que se escribió al PWM (lo que
//      el reporte de estado publica), incluido el recorte a DUTY_MAX. Esto fija que el
//      duty del reporte sea el realmente aplicado y no el objetivo de la rampa.
//   2) En el ángulo de trabajo, la flexión NO se bloquea aunque se esté más allá del
//      límite de extensión (la flexión se aleja de ese límite); solo se bloquea el
//      sentido que empuja hacia afuera. (El bug de banco no era este, pero lo fijamos.)

#include "unity.h"

#include "board.h" // PIN_MOTOR_RPWM, PIN_MOTOR_LPWM
#include "bts7960.h"
#include "config.h" // DUTY_MAX
#include "limits.h"
#include "mock_hal.h"

void setUp(void) {
    mock_hal_reset();
    bts7960_init();
}

void tearDown(void) {}

// Lo que devuelve bts7960_applied coincide con lo último escrito al PWM (flexión).
void test_applied_matches_written_flex(void) {
    bts7960_apply(DIR_FLEX, 0.5f);

    direction_t dir;
    float duty;
    bts7960_applied(&dir, &duty);

    TEST_ASSERT_EQUAL_INT(DIR_FLEX, dir);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, duty);
    // El reporte publica estos valores; deben ser los realmente escritos al PWM.
    TEST_ASSERT_EQUAL_FLOAT(0.5f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_LPWM));
}

void test_applied_matches_written_ext(void) {
    bts7960_apply(DIR_EXT, 0.3f);

    direction_t dir;
    float duty;
    bts7960_applied(&dir, &duty);

    TEST_ASSERT_EQUAL_INT(DIR_EXT, dir);
    TEST_ASSERT_EQUAL_FLOAT(0.3f, duty);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.3f, mock_pwm_last_duty(PIN_MOTOR_LPWM));
}

// stop (y DIR_STOP) deja todo en 0 y el getter informa DIR_STOP.
void test_applied_reports_stop(void) {
    bts7960_apply(DIR_FLEX, 0.5f);
    bts7960_stop();

    direction_t dir;
    float duty;
    bts7960_applied(&dir, &duty);

    TEST_ASSERT_EQUAL_INT(DIR_STOP, dir);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, duty);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_RPWM));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mock_pwm_last_duty(PIN_MOTOR_LPWM));
}

// El duty reportado refleja el recorte a DUTY_MAX que hace el driver.
void test_applied_reflects_duty_max_clamp(void) {
    bts7960_apply(DIR_FLEX, 1.5f);

    direction_t dir;
    float duty;
    bts7960_applied(&dir, &duty);

    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, duty);
    TEST_ASSERT_EQUAL_FLOAT(DUTY_MAX, mock_pwm_last_duty(PIN_MOTOR_RPWM));
}

// Más allá del límite de extensión, la flexión sigue permitida (se aleja del límite);
// la extensión, en cambio, queda bloqueada.
void test_flexion_not_blocked_past_extension_limit(void) {
    const float lo = 5.0f;
    const float hi = 110.0f;
    const float angle = 120.0f; // más allá del límite de extensión (como th=111.2 en banco)

    TEST_ASSERT_EQUAL_FLOAT(1.0f, limits_factor(DIR_FLEX, angle, lo, hi));
    TEST_ASSERT_FALSE(limits_must_stop(DIR_FLEX, angle, lo, hi));

    TEST_ASSERT_EQUAL_FLOAT(0.0f, limits_factor(DIR_EXT, angle, lo, hi));
    TEST_ASSERT_TRUE(limits_must_stop(DIR_EXT, angle, lo, hi));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_applied_matches_written_flex);
    RUN_TEST(test_applied_matches_written_ext);
    RUN_TEST(test_applied_reports_stop);
    RUN_TEST(test_applied_reflects_duty_max_clamp);
    RUN_TEST(test_flexion_not_blocked_past_extension_limit);
    return UNITY_END();
}
