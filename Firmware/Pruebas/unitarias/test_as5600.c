// test_as5600.c · Pruebas del driver AS5600: wrap_diff cruzando 4095->0 en ambos
// sentidos, conversión a grados, lectura cruda y de imán vía mock de I2C, y la lectura
// promediada no bloqueante (estable y cruzando el 0 del imán). Fase 3.

#include "unity.h"

#include "as5600.h"
#include "config.h" // AS5600_FILTER_SAMPLES
#include "mock_hal.h"

void setUp(void) {
    mock_i2c_reset();
}

void tearDown(void) {}

// --- wrap_diff: función pura, sin overflow al cruzar el 0 del imán ------------

void test_wrap_diff_zero(void) {
    TEST_ASSERT_EQUAL_INT32(0, as5600_wrap_diff(2000, 2000));
}

void test_wrap_diff_small_positive_no_cross(void) {
    TEST_ASSERT_EQUAL_INT32(50, as5600_wrap_diff(2050, 2000));
    TEST_ASSERT_EQUAL_INT32(-50, as5600_wrap_diff(1950, 2000));
}

void test_wrap_diff_crosses_zero_forward(void) {
    // raw sube de 4090 a 10 cruzando 0: la diferencia real es +16, no -4080.
    TEST_ASSERT_EQUAL_INT32(16, as5600_wrap_diff(10, 4090));
}

void test_wrap_diff_crosses_zero_backward(void) {
    // raw baja de 10 a 4090 cruzando 0: la diferencia real es -16.
    TEST_ASSERT_EQUAL_INT32(-16, as5600_wrap_diff(4090, 10));
}

void test_wrap_diff_extremes(void) {
    // Rango simétrico [-2048, 2047].
    TEST_ASSERT_EQUAL_INT32(-1, as5600_wrap_diff(4095, 0));    // 4095 está 1 cuenta antes de 0
    TEST_ASSERT_EQUAL_INT32(1, as5600_wrap_diff(0, 4095));     // 0 está 1 cuenta después de 4095
    TEST_ASSERT_EQUAL_INT32(2047, as5600_wrap_diff(2047, 0));  // máximo positivo
    TEST_ASSERT_EQUAL_INT32(-2048, as5600_wrap_diff(2048, 0)); // mitad exacta -> -2048 (mínimo)
}

// --- counts_to_deg -----------------------------------------------------------

void test_counts_to_deg(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, as5600_counts_to_deg(0));
    TEST_ASSERT_EQUAL_FLOAT(180.0f, as5600_counts_to_deg(2048));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, as5600_counts_to_deg(1024));
    TEST_ASSERT_EQUAL_FLOAT(360.0f, as5600_counts_to_deg(4096));
}

// --- Lectura cruda y de imán vía mock ----------------------------------------

void test_read_raw_from_mock(void) {
    mock_i2c_push_raw(1234);
    int32_t raw = -1;
    TEST_ASSERT_EQUAL_INT(STATUS_OK, as5600_read_raw(&raw));
    TEST_ASSERT_EQUAL_INT32(1234, raw);
}

void test_read_raw_bad_arg(void) {
    TEST_ASSERT_EQUAL_INT(STATUS_BAD_ARG, as5600_read_raw(NULL));
}

void test_read_raw_bus_error(void) {
    mock_i2c_set_status(STATUS_TIMEOUT);
    int32_t raw = 0;
    TEST_ASSERT_EQUAL_INT(STATUS_TIMEOUT, as5600_read_raw(&raw));
}

void test_read_magnet_decodes_bits(void) {
    as5600_magnet_t m;

    mock_i2c_set_magnet(true, false, false); // imán OK
    TEST_ASSERT_EQUAL_INT(STATUS_OK, as5600_read_magnet(&m));
    TEST_ASSERT_TRUE(m.detected);
    TEST_ASSERT_FALSE(m.too_weak);
    TEST_ASSERT_FALSE(m.too_strong);

    mock_i2c_set_magnet(false, true, true); // sin imán, débil y fuerte a la vez
    TEST_ASSERT_EQUAL_INT(STATUS_OK, as5600_read_magnet(&m));
    TEST_ASSERT_FALSE(m.detected);
    TEST_ASSERT_TRUE(m.too_weak);
    TEST_ASSERT_TRUE(m.too_strong);
}

// --- Lectura promediada no bloqueante ----------------------------------------

// Recorre un lote completo tomando una muestra por "tick".
static void run_full_batch(void) {
    as5600_avg_begin();
    for (uint32_t i = 0u; i < AS5600_FILTER_SAMPLES; ++i) {
        TEST_ASSERT_FALSE(as5600_avg_ready()); // no listo hasta la última
        TEST_ASSERT_EQUAL_INT(STATUS_OK, as5600_avg_sample());
    }
    TEST_ASSERT_TRUE(as5600_avg_ready());
}

void test_avg_stable_single_value(void) {
    mock_i2c_push_raw(1500); // se repite para todas las muestras
    run_full_batch();
    int32_t raw = -1;
    int32_t spread = -1;
    as5600_avg_result(&raw, &spread);
    TEST_ASSERT_EQUAL_INT32(1500, raw);
    TEST_ASSERT_EQUAL_INT32(0, spread);
}

void test_avg_crossing_zero(void) {
    // Muestras repartidas alrededor del 0 del imán (4094, 4095, 0): el promedio queda
    // en 4095 y la dispersión es de solo 2 cuentas, no un salto enorme. Esto es lo que
    // el promedio envuelto garantiza: restar crudo daría una dispersión de ~4095.
    mock_i2c_push_raw(4095); // referencia del lote
    mock_i2c_push_raw_n(4094, (AS5600_FILTER_SAMPLES - 1u) / 2u);
    mock_i2c_push_raw_n(0, AS5600_FILTER_SAMPLES - 1u - (AS5600_FILTER_SAMPLES - 1u) / 2u);
    run_full_batch();
    int32_t raw = -1;
    int32_t spread = -1;
    as5600_avg_result(&raw, &spread);
    TEST_ASSERT_EQUAL_INT32(4095, raw);
    TEST_ASSERT_EQUAL_INT32(2, spread); // de -1 (4094) a +1 (0) respecto de la referencia
}

void test_avg_sample_bus_error(void) {
    as5600_avg_begin();
    mock_i2c_set_status(STATUS_NACK);
    TEST_ASSERT_EQUAL_INT(STATUS_NACK, as5600_avg_sample());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wrap_diff_zero);
    RUN_TEST(test_wrap_diff_small_positive_no_cross);
    RUN_TEST(test_wrap_diff_crosses_zero_forward);
    RUN_TEST(test_wrap_diff_crosses_zero_backward);
    RUN_TEST(test_wrap_diff_extremes);
    RUN_TEST(test_counts_to_deg);
    RUN_TEST(test_read_raw_from_mock);
    RUN_TEST(test_read_raw_bad_arg);
    RUN_TEST(test_read_raw_bus_error);
    RUN_TEST(test_read_magnet_decodes_bits);
    RUN_TEST(test_avg_stable_single_value);
    RUN_TEST(test_avg_crossing_zero);
    RUN_TEST(test_avg_sample_bus_error);
    return UNITY_END();
}
