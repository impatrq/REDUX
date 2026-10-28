// test_rom_calib.c · Pruebas de la calibración del rango (rom_calib + as5600 real +
// mock de I2C): calibración normal con θ y límites en los extremos, raw_flex cerca de
// 4095 con el recorrido cruzando el 0, sentido invertido (sign negativo), rango fuera
// de ROM_MIN/ROM_MAX rechazado, lectura inestable rechazada, imán ausente, falla de
// bus y los eventos RECALIBRAR/CANCELAR. Fase 3.

#include "unity.h"

#include "as5600.h"
#include "config.h" // AS5600_FILTER_SAMPLES, ROM_MARGIN_DEG, ROM_MIN_DEG, ROM_MAX_DEG
#include "mock_hal.h"
#include "rom_calib.h"

void setUp(void) {
    mock_i2c_reset(); // imán OK y status OK por defecto
}

void tearDown(void) {}

// Corre los ticks suficientes para completar un lote de promediado.
static void run_sampling(rom_calib_t *rc) {
    for (uint32_t i = 0u; i < AS5600_FILTER_SAMPLES; ++i) {
        rom_calib_tick(rc);
    }
}

// Lleva la calibración hasta confirmar flexión en raw_flex (lote estable).
static void confirm_flex(rom_calib_t *rc, int32_t raw_flex) {
    mock_i2c_push_raw(raw_flex);
    rom_calib_event(rc, UI_EV_CONFIRM);
    run_sampling(rc);
}

// Confirma extensión en raw_ext (lote estable).
static void confirm_ext(rom_calib_t *rc, int32_t raw_ext) {
    mock_i2c_push_raw(raw_ext);
    rom_calib_event(rc, UI_EV_CONFIRM);
    run_sampling(rc);
}

// Calibración completa flex->ext con lotes estables.
static void calibrate(rom_calib_t *rc, int32_t raw_flex, int32_t raw_ext) {
    rom_calib_begin(rc);
    confirm_flex(rc, raw_flex);
    confirm_ext(rc, raw_ext);
}

void test_begin_waits_for_flex(void) {
    rom_calib_t rc;
    rom_calib_begin(&rc);
    TEST_ASSERT_EQUAL_INT(ROM_WAIT_FLEX, rc.phase);
    TEST_ASSERT_FALSE(rom_calib_is_ready(&rc));
}

void test_normal_calibration_angle_and_limits(void) {
    rom_calib_t rc;
    // 1024 cuentas de recorrido = 90°, sentido directo.
    calibrate(&rc, 1000, 2024);

    TEST_ASSERT_TRUE(rom_calib_is_ready(&rc));
    TEST_ASSERT_EQUAL_INT(ROM_ERR_NONE, rc.error);
    TEST_ASSERT_EQUAL_INT32(1, rc.sign);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, rc.rom_deg);

    // θ = 0° en flexión máxima, rom_deg en extensión máxima.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, rom_calib_angle_deg(&rc, 1000));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, rom_calib_angle_deg(&rc, 2024));

    // Límites de trabajo: [ROM_MARGIN_DEG, rom_deg - ROM_MARGIN_DEG].
    TEST_ASSERT_FLOAT_WITHIN(0.1f, ROM_MARGIN_DEG, rom_calib_limit_lo(&rc));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f - ROM_MARGIN_DEG, rom_calib_limit_hi(&rc));
}

void test_calibration_crossing_zero(void) {
    rom_calib_t rc;
    // raw_flex cerca de 4095; extensión de +1024 cuentas cruza el 0 del imán.
    calibrate(&rc, 4090, (4090 + 1024) % 4096); // raw_ext = 1018

    TEST_ASSERT_TRUE(rom_calib_is_ready(&rc));
    TEST_ASSERT_EQUAL_INT32(1, rc.sign);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, rc.rom_deg);
    // El ángulo no salta al cruzar el cero: crece de 0 a 90 de forma monótona.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, rom_calib_angle_deg(&rc, 4090));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, rom_calib_angle_deg(&rc, 1018));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 45.0f, rom_calib_angle_deg(&rc, (4090 + 512) % 4096));
}

void test_calibration_inverted_sign(void) {
    rom_calib_t rc;
    // La extensión hace BAJAR la cuenta cruda: sign debe salir negativo y, aun así,
    // el ángulo de extensión es positivo.
    calibrate(&rc, 2000, 2000 - 1024); // raw_ext = 976

    TEST_ASSERT_TRUE(rom_calib_is_ready(&rc));
    TEST_ASSERT_EQUAL_INT32(-1, rc.sign);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, rc.rom_deg);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, rom_calib_angle_deg(&rc, 2000));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, rom_calib_angle_deg(&rc, 976));
}

void test_range_too_small_rejected(void) {
    rom_calib_t rc;
    // 100 cuentas ~= 8.8° < ROM_MIN_DEG (20°).
    calibrate(&rc, 1000, 1100);
    TEST_ASSERT_EQUAL_INT(ROM_FAILED, rc.phase);
    TEST_ASSERT_EQUAL_INT(ROM_ERR_RANGE, rc.error);
    TEST_ASSERT_FALSE(rom_calib_is_ready(&rc));
}

void test_range_too_large_rejected(void) {
    rom_calib_t rc;
    // 2000 cuentas ~= 175.8° > ROM_MAX_DEG (160°).
    calibrate(&rc, 1000, 3000);
    TEST_ASSERT_EQUAL_INT(ROM_FAILED, rc.phase);
    TEST_ASSERT_EQUAL_INT(ROM_ERR_RANGE, rc.error);
}

void test_unstable_reading_rejected(void) {
    rom_calib_t rc;
    rom_calib_begin(&rc);
    // Lote con dispersión grande (rampa de 2 cuentas): ~5.4° > ROM_STABLE_MAX_DEG.
    mock_i2c_push_ramp(1000, 2, AS5600_FILTER_SAMPLES);
    rom_calib_event(&rc, UI_EV_CONFIRM);
    run_sampling(&rc);
    TEST_ASSERT_EQUAL_INT(ROM_FAILED, rc.phase);
    TEST_ASSERT_EQUAL_INT(ROM_ERR_UNSTABLE, rc.error);
}

void test_magnet_absent_rejected(void) {
    rom_calib_t rc;
    rom_calib_begin(&rc);
    mock_i2c_set_magnet(false, false, false); // sin imán
    mock_i2c_push_raw(1000);
    rom_calib_event(&rc, UI_EV_CONFIRM); // start_sampling revisa el imán y falla ya
    TEST_ASSERT_EQUAL_INT(ROM_FAILED, rc.phase);
    TEST_ASSERT_EQUAL_INT(ROM_ERR_MAGNET, rc.error);
}

void test_bus_error_rejected(void) {
    rom_calib_t rc;
    rom_calib_begin(&rc);
    mock_i2c_set_status(STATUS_TIMEOUT);
    rom_calib_event(&rc, UI_EV_CONFIRM); // falla al leer el imán
    TEST_ASSERT_EQUAL_INT(ROM_FAILED, rc.phase);
    TEST_ASSERT_EQUAL_INT(ROM_ERR_BUS, rc.error);
}

void test_recalibrate_restarts(void) {
    rom_calib_t rc;
    calibrate(&rc, 1000, 1100); // falla por rango
    TEST_ASSERT_EQUAL_INT(ROM_FAILED, rc.phase);
    rom_calib_event(&rc, UI_EV_RECALIBRATE);
    TEST_ASSERT_EQUAL_INT(ROM_WAIT_FLEX, rc.phase);
    TEST_ASSERT_EQUAL_INT(ROM_ERR_NONE, rc.error);
}

void test_cancel_goes_idle(void) {
    rom_calib_t rc;
    rom_calib_begin(&rc);
    rom_calib_event(&rc, UI_EV_CANCEL);
    TEST_ASSERT_EQUAL_INT(ROM_IDLE, rc.phase);
}

void test_flex_then_wait_ext(void) {
    rom_calib_t rc;
    rom_calib_begin(&rc);
    confirm_flex(&rc, 1000);
    TEST_ASSERT_EQUAL_INT(ROM_WAIT_EXT, rc.phase);
    TEST_ASSERT_EQUAL_INT32(1000, rc.raw_flex);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_waits_for_flex);
    RUN_TEST(test_normal_calibration_angle_and_limits);
    RUN_TEST(test_calibration_crossing_zero);
    RUN_TEST(test_calibration_inverted_sign);
    RUN_TEST(test_range_too_small_rejected);
    RUN_TEST(test_range_too_large_rejected);
    RUN_TEST(test_unstable_reading_rejected);
    RUN_TEST(test_magnet_absent_rejected);
    RUN_TEST(test_bus_error_rejected);
    RUN_TEST(test_recalibrate_restarts);
    RUN_TEST(test_cancel_goes_idle);
    RUN_TEST(test_flex_then_wait_ext);
    return UNITY_END();
}
