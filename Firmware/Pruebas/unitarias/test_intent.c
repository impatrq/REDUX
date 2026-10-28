// test_intent.c · Pruebas de intent_decide: las cuatro combinaciones de la tabla XOR. Fase 5.

#include "unity.h"

#include "intent.h"

void setUp(void) {}
void tearDown(void) {}

// Ningún músculo activo -> reposo.
void test_none_active_is_rest(void) {
    TEST_ASSERT_EQUAL_INT(INTENT_REST, intent_decide(false, false));
}

// Solo bíceps -> flexión.
void test_only_biceps_is_flex(void) {
    TEST_ASSERT_EQUAL_INT(INTENT_FLEX, intent_decide(true, false));
}

// Solo tríceps -> extensión.
void test_only_triceps_is_ext(void) {
    TEST_ASSERT_EQUAL_INT(INTENT_EXT, intent_decide(false, true));
}

// Ambos activos -> co-contracción (intención ambigua, el motor se corta).
void test_both_active_is_cocontraction(void) {
    TEST_ASSERT_EQUAL_INT(INTENT_COCONTRACTION, intent_decide(true, true));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_none_active_is_rest);
    RUN_TEST(test_only_biceps_is_flex);
    RUN_TEST(test_only_triceps_is_ext);
    RUN_TEST(test_both_active_is_cocontraction);
    return UNITY_END();
}
