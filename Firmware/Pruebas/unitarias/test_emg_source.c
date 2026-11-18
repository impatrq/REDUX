// test_emg_source.c · Pruebas de la fuente de EMG por canal. Combinación mixta (bíceps
// ADC, tríceps SERIAL): el bíceps no es simulado y rechaza "b N"; el tríceps responde a
// "t N" y "r" lo pone en reposo sin tocar el bíceps. También los casos ambos SERIAL
// (banco) y ambos ADC, el clamp de porcentaje y las líneas que no son comandos. Fase EMG real.

#include "unity.h"

#include "emg_source.h"

void setUp(void) {}
void tearDown(void) {}

// --- ambos SERIAL (banco) -----------------------------------------------------
void test_both_serial_levels_and_rest(void) {
    emg_source_t s;
    emg_source_init(&s, EMG_SRC_SERIAL, EMG_SRC_SERIAL);

    TEST_ASSERT_TRUE(emg_source_bic_is_sim(&s));
    TEST_ASSERT_TRUE(emg_source_tri_is_sim(&s));

    TEST_ASSERT_EQUAL_INT(EMG_CMD_OK, emg_source_feed(&s, "b 50"));
    TEST_ASSERT_EQUAL_INT(EMG_CMD_OK, emg_source_feed(&s, "t 30"));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.50f, emg_source_bic_level(&s));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.30f, emg_source_tri_level(&s));

    TEST_ASSERT_EQUAL_INT(EMG_CMD_OK, emg_source_feed(&s, "r"));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, emg_source_bic_level(&s));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, emg_source_tri_level(&s));
}

// --- combinación mixta: bíceps ADC, tríceps SERIAL ----------------------------
void test_mixed_bic_adc_tri_serial(void) {
    emg_source_t s;
    emg_source_init(&s, EMG_SRC_ADC, EMG_SRC_SERIAL);

    // simB=false, simT=true: solo el tríceps se informa como simulado.
    TEST_ASSERT_FALSE(emg_source_bic_is_sim(&s));
    TEST_ASSERT_TRUE(emg_source_tri_is_sim(&s));

    // El bíceps es ADC: "b N" se rechaza y no fija ningún nivel simulado.
    TEST_ASSERT_EQUAL_INT(EMG_CMD_BIC_IS_ADC, emg_source_feed(&s, "b 80"));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, emg_source_bic_level(&s));

    // El tríceps es SERIAL: "t N" se aplica.
    TEST_ASSERT_EQUAL_INT(EMG_CMD_OK, emg_source_feed(&s, "t 50"));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.50f, emg_source_tri_level(&s));

    // "r" pone en reposo SOLO los canales simulados: el tríceps vuelve a 0.
    TEST_ASSERT_EQUAL_INT(EMG_CMD_OK, emg_source_feed(&s, "r"));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, emg_source_tri_level(&s));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, emg_source_bic_level(&s)); // ADC: siempre 0 acá
}

// --- ambos ADC ----------------------------------------------------------------
void test_both_adc_reject_commands(void) {
    emg_source_t s;
    emg_source_init(&s, EMG_SRC_ADC, EMG_SRC_ADC);

    TEST_ASSERT_FALSE(emg_source_bic_is_sim(&s));
    TEST_ASSERT_FALSE(emg_source_tri_is_sim(&s));

    TEST_ASSERT_EQUAL_INT(EMG_CMD_BIC_IS_ADC, emg_source_feed(&s, "b 40"));
    TEST_ASSERT_EQUAL_INT(EMG_CMD_TRI_IS_ADC, emg_source_feed(&s, "t 40"));
    // "r" es válido aunque no haya nada que resetear (no toca canales ADC).
    TEST_ASSERT_EQUAL_INT(EMG_CMD_OK, emg_source_feed(&s, "r"));
}

// --- clamp de porcentaje y líneas que no son comandos -------------------------
void test_clamp_and_unknown(void) {
    emg_source_t s;
    emg_source_init(&s, EMG_SRC_SERIAL, EMG_SRC_SERIAL);

    emg_source_feed(&s, "t 150"); // por encima de 100 -> 1.0
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, emg_source_tri_level(&s));
    emg_source_feed(&s, "b -5"); // negativo -> 0
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, emg_source_bic_level(&s));

    // Una línea que no es "b"/"t"/"r" no se consume (la procesa la interfaz).
    TEST_ASSERT_EQUAL_INT(EMG_CMD_NONE, emg_source_feed(&s, "ok"));
    TEST_ASSERT_EQUAL_INT(EMG_CMD_NONE, emg_source_feed(&s, "b")); // sin argumento
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_both_serial_levels_and_rest);
    RUN_TEST(test_mixed_bic_adc_tri_serial);
    RUN_TEST(test_both_adc_reject_commands);
    RUN_TEST(test_clamp_and_unknown);
    return UNITY_END();
}
