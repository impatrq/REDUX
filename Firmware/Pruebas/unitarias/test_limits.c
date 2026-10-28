// test_limits.c · Pruebas de limits_factor y limits_must_stop: lejos del límite, en la
// zona de freno, en el límite, fuera de rango en ambos sentidos (solo deja volver) y
// DIR_STOP. Fase 5.

#include "unity.h"

#include "config.h" // BRAKE_ZONE_DEG
#include "limits.h"

// Rango de trabajo de ejemplo (0° = flexión máxima, el ángulo crece al extender):
// lo = ROM_MARGIN_DEG, hi = rom_deg - ROM_MARGIN_DEG. Acá valores concretos.
#define LO_DEG 5.0f
#define HI_DEG 125.0f

void setUp(void) {}
void tearDown(void) {}

// --- limits_factor -----------------------------------------------------------

// Lejos de ambos límites: factor 1.0 en los dos sentidos.
void test_factor_far_is_one(void) {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, limits_factor(DIR_FLEX, 60.0f, LO_DEG, HI_DEG));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, limits_factor(DIR_EXT, 60.0f, LO_DEG, HI_DEG));
}

// Dentro de la zona de freno: factor = distancia / BRAKE_ZONE_DEG.
void test_factor_in_brake_zone(void) {
    // FLEX a mitad de la zona de freno sobre lo: distancia = 7.5 -> 0.5.
    const float angle_flex = LO_DEG + BRAKE_ZONE_DEG / 2.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.5f, limits_factor(DIR_FLEX, angle_flex, LO_DEG, HI_DEG));
    // EXT a mitad de la zona de freno bajo hi: distancia = 7.5 -> 0.5.
    const float angle_ext = HI_DEG - BRAKE_ZONE_DEG / 2.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.5f, limits_factor(DIR_EXT, angle_ext, LO_DEG, HI_DEG));
    // Justo al borde de la zona de freno: factor 1.0.
    TEST_ASSERT_EQUAL_FLOAT(1.0f, limits_factor(DIR_FLEX, LO_DEG + BRAKE_ZONE_DEG, LO_DEG, HI_DEG));
}

// En el límite exacto: factor 0.0 (bloquea ese sentido).
void test_factor_at_limit_is_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, limits_factor(DIR_FLEX, LO_DEG, LO_DEG, HI_DEG));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, limits_factor(DIR_EXT, HI_DEG, LO_DEG, HI_DEG));
}

// Fuera de rango por debajo de lo: FLEX bloqueado, EXT (vuelve hacia adentro) permitido.
void test_factor_below_range_only_allows_ext(void) {
    const float angle = LO_DEG - 10.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, limits_factor(DIR_FLEX, angle, LO_DEG, HI_DEG));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, limits_factor(DIR_EXT, angle, LO_DEG, HI_DEG));
}

// Fuera de rango por encima de hi: EXT bloqueado, FLEX (vuelve hacia adentro) permitido.
void test_factor_above_range_only_allows_flex(void) {
    const float angle = HI_DEG + 10.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, limits_factor(DIR_EXT, angle, LO_DEG, HI_DEG));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, limits_factor(DIR_FLEX, angle, LO_DEG, HI_DEG));
}

// DIR_STOP no tiene sentido de giro: factor 0.0.
void test_factor_stop_is_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, limits_factor(DIR_STOP, 60.0f, LO_DEG, HI_DEG));
}

// --- limits_must_stop --------------------------------------------------------

// Lejos del límite: no hay que cortar.
void test_must_stop_far_is_false(void) {
    TEST_ASSERT_FALSE(limits_must_stop(DIR_FLEX, 60.0f, LO_DEG, HI_DEG));
    TEST_ASSERT_FALSE(limits_must_stop(DIR_EXT, 60.0f, LO_DEG, HI_DEG));
}

// En el límite del sentido aplicado: corte inmediato (aunque esté en rampa).
void test_must_stop_at_limit_is_true(void) {
    TEST_ASSERT_TRUE(limits_must_stop(DIR_FLEX, LO_DEG, LO_DEG, HI_DEG));
    TEST_ASSERT_TRUE(limits_must_stop(DIR_EXT, HI_DEG, LO_DEG, HI_DEG));
}

// DIR_STOP nunca obliga a cortar.
void test_must_stop_stop_is_false(void) {
    TEST_ASSERT_FALSE(limits_must_stop(DIR_STOP, LO_DEG, LO_DEG, HI_DEG));
    TEST_ASSERT_FALSE(limits_must_stop(DIR_STOP, HI_DEG, LO_DEG, HI_DEG));
}

// Fuera de rango: solo corta el sentido que se aleja del rango.
void test_must_stop_out_of_range_only_receding(void) {
    const float below = LO_DEG - 10.0f;
    TEST_ASSERT_TRUE(limits_must_stop(DIR_FLEX, below, LO_DEG, HI_DEG));  // se aleja
    TEST_ASSERT_FALSE(limits_must_stop(DIR_EXT, below, LO_DEG, HI_DEG));  // vuelve

    const float above = HI_DEG + 10.0f;
    TEST_ASSERT_TRUE(limits_must_stop(DIR_EXT, above, LO_DEG, HI_DEG));   // se aleja
    TEST_ASSERT_FALSE(limits_must_stop(DIR_FLEX, above, LO_DEG, HI_DEG)); // vuelve
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_factor_far_is_one);
    RUN_TEST(test_factor_in_brake_zone);
    RUN_TEST(test_factor_at_limit_is_zero);
    RUN_TEST(test_factor_below_range_only_allows_ext);
    RUN_TEST(test_factor_above_range_only_allows_flex);
    RUN_TEST(test_factor_stop_is_zero);
    RUN_TEST(test_must_stop_far_is_false);
    RUN_TEST(test_must_stop_at_limit_is_true);
    RUN_TEST(test_must_stop_stop_is_false);
    RUN_TEST(test_must_stop_out_of_range_only_receding);
    return UNITY_END();
}
