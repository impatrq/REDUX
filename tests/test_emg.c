// test_emg.c · Pruebas del canal EMG con señales sintéticas: reposo con ruido no
// activa, escalón sostenido activa recién tras EMG_ON_MIN_MS, un pico aislado corto no
// activa, oscilación alrededor del umbral no genera parpadeo, intensidad en [0,1] y
// creciente. Cubre también emg_calib: reposo calcula umbrales, ruido alto y MVC débil
// dan error, saturación detectada y alpha correcto para 4 Hz / tick de 1 ms. Fase 4.

#include "unity.h"

#include "config.h"
#include "emg.h"
#include "emg_calib.h"

void setUp(void) {}
void tearDown(void) {}

// Cantidad de ticks que equivale a una duración en ms (igual que el módulo).
static uint32_t ms_to_ticks(uint32_t ms) {
    return (ms * 1000u) / CTRL_TICK_US;
}

// Alimenta el canal con una muestra cruda constante durante n ticks.
static void feed(emg_channel_t *ch, uint16_t raw, uint32_t ticks) {
    for (uint32_t i = 0u; i < ticks; ++i) {
        emg_update(ch, raw);
    }
}

// Corre una toma de reposo con envolvente constante y la cierra.
static void calibrate_rest_const(emg_calib_t *c, emg_channel_t *ch, float env) {
    emg_calib_rest_begin(c, ch);
    const uint32_t target = c->n_target;
    for (uint32_t i = 0u; i < target; ++i) {
        emg_calib_add(c, env);
    }
}

// Corre una toma de MVC con envolvente de pico constante y la cierra.
static void calibrate_mvc_const(emg_calib_t *c, emg_channel_t *ch, float peak) {
    emg_calib_mvc_begin(c, ch);
    const uint32_t target = c->n_target;
    for (uint32_t i = 0u; i < target; ++i) {
        emg_calib_add(c, peak);
    }
}

// --- alpha del filtro ---------------------------------------------------------
void test_env_alpha_4hz_1ms(void) {
    // RC = 1/(2π·4) = 0.0397887 s; dt = 1e-3 s; alpha = dt/(RC+dt) = 0.0245165.
    TEST_ASSERT_FLOAT_WITHIN(5e-4f, 0.024516f, emg_env_alpha(4.0f, 1000u));
}

// --- activación ---------------------------------------------------------------
void test_rest_with_noise_does_not_activate(void) {
    emg_channel_t ch;
    emg_init(&ch);
    emg_set_thresholds(&ch, 130.0f, 118.0f); // como dejaría un reposo en ~100

    // Reposo ruidoso alrededor de 100 (muy por debajo del umbral de 130).
    for (uint32_t i = 0u; i < 1000u; ++i) {
        emg_update(&ch, (i % 2u) ? 140u : 60u);
        TEST_ASSERT_FALSE(emg_is_active(&ch));
    }
}

void test_sustained_activates_after_on_min(void) {
    emg_channel_t ch;
    emg_init(&ch);
    emg_set_thresholds(&ch, 100.0f, 60.0f);

    uint32_t above = 0u;    // ticks con envolvente sobre el umbral
    uint32_t activate_at = 0u;
    bool activated = false;
    for (uint32_t t = 0u; t < 500u && !activated; ++t) {
        const float env = emg_update(&ch, 4000u);
        if (env >= ch.thr_on) {
            above++;
        }
        if (emg_is_active(&ch)) {
            activate_at = above;
            activated = true;
        }
    }
    TEST_ASSERT_TRUE(activated);
    // Se activa exactamente al completar EMG_ON_MIN_MS sobre el umbral, no antes.
    TEST_ASSERT_EQUAL_UINT32(ms_to_ticks(EMG_ON_MIN_MS), activate_at);
}

void test_short_spike_does_not_activate(void) {
    emg_channel_t ch;
    emg_init(&ch);
    emg_set_thresholds(&ch, 100.0f, 60.0f);

    // Pico corto de amplitud moderada: la envolvente sube un poco por encima de
    // thr_on pero durante menos de EMG_ON_MIN_MS. El conteo de activación corre
    // sobre la envolvente (la cruda real ya viene rectificada y sin suavizar), así
    // que lo que descarta este pico es el tiempo mínimo, no la amplitud.
    feed(&ch, 320u, 20u); // 20 ms < EMG_ON_MIN_MS
    TEST_ASSERT_TRUE(emg_envelope(&ch) > ch.thr_on); // de verdad cruzó el umbral
    TEST_ASSERT_FALSE(emg_is_active(&ch));
    // Luego vuelve a reposo: nunca debe activarse.
    for (uint32_t i = 0u; i < 300u; ++i) {
        emg_update(&ch, 0u);
        TEST_ASSERT_FALSE(emg_is_active(&ch));
    }
}

// Un artefacto enorme y breve no activa (lo filtra EMG_ON_MIN_MS igual que uno
// moderado); el rechazo de un artefacto grande sostenido es por saturación, no por
// la lógica de intención. En Fase 8 este rechazo se afina con la señal real.
void test_huge_artifact_flagged_by_saturation(void) {
    emg_channel_t ch;
    emg_init(&ch);
    emg_set_thresholds(&ch, 100.0f, 60.0f);

    // Pico enorme pero breve (20 ms): ni activa ni satura todavía.
    feed(&ch, 4095u, 20u);
    TEST_ASSERT_FALSE(emg_is_active(&ch));
    TEST_ASSERT_FALSE(emg_is_saturated(&ch));

    // Si el artefacto grande se sostiene hasta EMG_SAT_MS, se marca por saturación.
    feed(&ch, 4095u, ms_to_ticks(EMG_SAT_MS));
    TEST_ASSERT_TRUE(emg_is_saturated(&ch));
}

void test_oscillation_around_threshold_no_flicker(void) {
    emg_channel_t ch;
    emg_init(&ch);
    emg_set_thresholds(&ch, 100.0f, 60.0f);

    // Primero activar con un escalón sostenido.
    feed(&ch, 4000u, 200u);
    TEST_ASSERT_TRUE(emg_is_active(&ch));

    // Oscilar la envolvente alrededor del umbral de activación (cruzándolo) pero
    // siempre por encima del de desactivación: el estado no debe parpadear.
    for (uint32_t i = 0u; i < 500u; ++i) {
        emg_update(&ch, (i % 2u) ? 110u : 70u);
        TEST_ASSERT_TRUE(emg_is_active(&ch));
    }
}

// --- intensidad ---------------------------------------------------------------
void test_intensity_bounded_and_increasing(void) {
    emg_channel_t ch;
    emg_calib_t c;
    emg_init(&ch);
    calibrate_rest_const(&c, &ch, 100.0f); // thr_on = 130
    calibrate_mvc_const(&c, &ch, 1000.0f); // env_at_max = 100 + 0.8·900 = 820

    const float levels[] = {100.0f, 130.0f, 200.0f, 400.0f, 820.0f, 1500.0f};
    float prev = -1.0f;
    for (unsigned i = 0u; i < sizeof(levels) / sizeof(levels[0]); ++i) {
        ch.env = levels[i];
        const float in = emg_intensity(&ch);
        TEST_ASSERT_TRUE(in >= 0.0f && in <= 1.0f);
        TEST_ASSERT_TRUE(in >= prev); // no decreciente al crecer la contracción
        prev = in;
    }
    // Extremos: 0 en el umbral, 1 en el máximo (y saturada por encima).
    ch.env = 130.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, emg_intensity(&ch));
    ch.env = 820.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, emg_intensity(&ch));
    ch.env = 1500.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, emg_intensity(&ch));
}

void test_intensity_high_baseline(void) {
    emg_channel_t ch;
    emg_calib_t c;
    emg_init(&ch);
    calibrate_rest_const(&c, &ch, 1000.0f); // línea de base alta; thr_on = 1030
    calibrate_mvc_const(&c, &ch, 2000.0f);  // env_at_max = 1000 + 0.8·1000 = 1800

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1030.0f, emg_threshold_on(&ch));

    ch.env = 1030.0f; // en el umbral -> 0
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, emg_intensity(&ch));
    ch.env = 1415.0f; // punto medio -> 0.5
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.5f, emg_intensity(&ch));
    ch.env = 1800.0f; // máximo -> 1
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, emg_intensity(&ch));
    ch.env = 900.0f; // por debajo del reposo -> 0 (acotada)
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, emg_intensity(&ch));
}

// --- calibración --------------------------------------------------------------
void test_rest_calibration_thresholds(void) {
    emg_channel_t ch;
    emg_calib_t c;
    emg_init(&ch);
    calibrate_rest_const(&c, &ch, 100.0f);

    TEST_ASSERT_TRUE(emg_calib_is_done(&c));
    TEST_ASSERT_EQUAL_INT(EMG_CAL_ERR_NONE, c.error);
    // Sin ruido: margen = EMG_MARGIN_MIN. thr_on = 100 + 30; thr_off = 100 + 30·0.6.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f + EMG_MARGIN_MIN, ch.thr_on);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f + EMG_MARGIN_MIN * EMG_OFF_FRAC, ch.thr_off);
    TEST_ASSERT_TRUE(ch.calibrated);
}

void test_rest_high_noise_fails(void) {
    emg_channel_t ch;
    emg_calib_t c;
    emg_init(&ch);

    // Envolvente que alterna 50/250: media 150, desvío 100 > EMG_REST_NOISE_MAX.
    emg_calib_rest_begin(&c, &ch);
    const uint32_t target = c.n_target;
    for (uint32_t i = 0u; i < target; ++i) {
        emg_calib_add(&c, (i % 2u) ? 250.0f : 50.0f);
    }
    TEST_ASSERT_TRUE(emg_calib_failed(&c));
    TEST_ASSERT_EQUAL_INT(EMG_CAL_ERR_REST_NOISE, c.error);
    TEST_ASSERT_FALSE(ch.calibrated); // no se instalaron umbrales
}

void test_mvc_weak_fails(void) {
    emg_channel_t ch;
    emg_calib_t c;
    emg_init(&ch);
    calibrate_rest_const(&c, &ch, 100.0f); // thr_on = 130, reposo 100

    // Pico 135: sobre el reposo 35 < EMG_MVC_MIN_RATIO·(130-100) = 45 -> débil.
    calibrate_mvc_const(&c, &ch, 135.0f);
    TEST_ASSERT_TRUE(emg_calib_failed(&c));
    TEST_ASSERT_EQUAL_INT(EMG_CAL_ERR_WEAK_MVC, c.error);
}

void test_mvc_strong_ok(void) {
    emg_channel_t ch;
    emg_calib_t c;
    emg_init(&ch);
    calibrate_rest_const(&c, &ch, 100.0f);
    calibrate_mvc_const(&c, &ch, 1000.0f);
    TEST_ASSERT_TRUE(emg_calib_is_done(&c));
    TEST_ASSERT_EQUAL_INT(EMG_CAL_ERR_NONE, c.error);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 820.0f, ch.env_at_max);
}

// --- saturación ---------------------------------------------------------------
void test_saturation_detected(void) {
    emg_channel_t ch;
    emg_init(&ch);

    // Justo por debajo del tiempo de saturación: todavía no marca.
    feed(&ch, 4095u, ms_to_ticks(EMG_SAT_MS) - 1u);
    TEST_ASSERT_FALSE(emg_is_saturated(&ch));

    // Un tick más completa EMG_SAT_MS: queda saturado.
    emg_update(&ch, 4095u);
    TEST_ASSERT_TRUE(emg_is_saturated(&ch));

    // Al bajar la señal cruda, la saturación se limpia.
    emg_update(&ch, 0u);
    TEST_ASSERT_FALSE(emg_is_saturated(&ch));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_env_alpha_4hz_1ms);
    RUN_TEST(test_rest_with_noise_does_not_activate);
    RUN_TEST(test_sustained_activates_after_on_min);
    RUN_TEST(test_short_spike_does_not_activate);
    RUN_TEST(test_huge_artifact_flagged_by_saturation);
    RUN_TEST(test_oscillation_around_threshold_no_flicker);
    RUN_TEST(test_intensity_bounded_and_increasing);
    RUN_TEST(test_intensity_high_baseline);
    RUN_TEST(test_rest_calibration_thresholds);
    RUN_TEST(test_rest_high_noise_fails);
    RUN_TEST(test_mvc_weak_fails);
    RUN_TEST(test_mvc_strong_ok);
    RUN_TEST(test_saturation_detected);
    return UNITY_END();
}
