// test_supervisor.c · Pruebas del supervisor (Fase 6): secuencia completa BOOT->RUN,
// falla del encoder en cada estado de calibración, RECALIBRATE desde RUN, CANCEL en
// calibración, FAULT_OVERRUN con 3 sobrepasos seguidos (y que 2 no alcanzan), las dos
// rutas de rearme (FAULT_ENC->CALIB_ROM; OVERRUN->ARMING sin perder calibración),
// rearme que exige reposo, saturación de EMG en marcha, la invariante de motor_allowed
// y un mini-test de integración (supervisor + cadena de control) para co-contracción y
// límite alcanzado en marcha. También cubre el detector de salto de ángulo.

#include "unity.h"

#include "config.h"

#include "angle_watch.h"
#include "intent.h"
#include "limits.h"
#include "power.h"
#include "supervisor.h"

void setUp(void) {}
void tearDown(void) {}

// Un paso del supervisor con la entrada dada; devuelve la salida.
static supervisor_out_t step(supervisor_t *sv, supervisor_in_t in) {
    supervisor_out_t out;
    supervisor_step(sv, &in, &out);
    // Invariante global: el motor solo puede moverse en ST_RUN.
    TEST_ASSERT_EQUAL_INT(out.state == ST_RUN, out.motor_allowed);
    return out;
}

// Lleva la máquina desde ST_BOOT hasta `target` por el camino nominal.
static supervisor_out_t drive_to(supervisor_t *sv, sys_state_t target) {
    supervisor_in_t in;

    in = (supervisor_in_t){0};
    in.ui_event = UI_EV_CONFIRM;
    supervisor_out_t out = step(sv, in); // BOOT -> CHECK_ENC
    if (target == ST_CHECK_ENC) {
        return out;
    }
    out = step(sv, (supervisor_in_t){0}); // CHECK_ENC -> CALIB_ROM (encoder OK)
    if (target == ST_CALIB_ROM) {
        return out;
    }
    in = (supervisor_in_t){0};
    in.calib_done = true;
    out = step(sv, in); // -> CALIB_REST
    if (target == ST_CALIB_REST) {
        return out;
    }
    in = (supervisor_in_t){0};
    in.calib_done = true;
    out = step(sv, in); // -> CALIB_MVC
    if (target == ST_CALIB_MVC) {
        return out;
    }
    in = (supervisor_in_t){0};
    in.calib_done = true;
    out = step(sv, in); // -> ARMING
    if (target == ST_ARMING) {
        return out;
    }
    in = (supervisor_in_t){0};
    in.emg_rest = true;
    return step(sv, in); // -> RUN
}

// Avanza `n` ticks estables (encoder OK, sin saturación ni sobrepaso) y devuelve la
// última salida. Útil para el rearme desde ST_FAULT.
static supervisor_out_t stable_ticks(supervisor_t *sv, uint32_t n) {
    supervisor_out_t out = {ST_FAULT, false, FAULT_NONE};
    for (uint32_t i = 0u; i < n; ++i) {
        out = step(sv, (supervisor_in_t){0});
    }
    return out;
}

// --- Secuencia completa -------------------------------------------------------
void test_full_sequence_boot_to_run(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    TEST_ASSERT_EQUAL_INT(ST_BOOT, sv.state);

    // Sin confirmación no arranca.
    supervisor_out_t out = step(&sv, (supervisor_in_t){0});
    TEST_ASSERT_EQUAL_INT(ST_BOOT, out.state);

    supervisor_in_t in = {0};
    in.ui_event = UI_EV_CONFIRM;
    out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_CHECK_ENC, out.state);

    out = step(&sv, (supervisor_in_t){0}); // encoder OK
    TEST_ASSERT_EQUAL_INT(ST_CALIB_ROM, out.state);

    in = (supervisor_in_t){0};
    in.calib_done = true;
    out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_CALIB_REST, out.state);

    in = (supervisor_in_t){0};
    in.calib_done = true;
    out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_CALIB_MVC, out.state);

    in = (supervisor_in_t){0};
    in.calib_done = true;
    out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_ARMING, out.state);

    // En ARMING, sin reposo no pasa a marcha.
    out = step(&sv, (supervisor_in_t){0});
    TEST_ASSERT_EQUAL_INT(ST_ARMING, out.state);
    TEST_ASSERT_FALSE(out.motor_allowed);

    in = (supervisor_in_t){0};
    in.emg_rest = true;
    out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_RUN, out.state);
    TEST_ASSERT_TRUE(out.motor_allowed);
}

// --- Falla del encoder en cada estado de calibración --------------------------
static void check_enc_fault_in_calib(sys_state_t calib_state) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, calib_state);

    supervisor_in_t in = {0};
    in.enc_fault = FAULT_MAGNET;
    supervisor_out_t out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_FAULT, out.state);
    TEST_ASSERT_FALSE(out.motor_allowed);
    TEST_ASSERT_TRUE(out.faults & FAULT_MAGNET);
}

void test_enc_fault_in_calib_rom(void) { check_enc_fault_in_calib(ST_CALIB_ROM); }
void test_enc_fault_in_calib_rest(void) { check_enc_fault_in_calib(ST_CALIB_REST); }
void test_enc_fault_in_calib_mvc(void) { check_enc_fault_in_calib(ST_CALIB_MVC); }

// --- Eventos de UI ------------------------------------------------------------
void test_recalibrate_from_run(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_RUN);

    supervisor_in_t in = {0};
    in.ui_event = UI_EV_RECALIBRATE;
    supervisor_out_t out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_CALIB_ROM, out.state);
    TEST_ASSERT_FALSE(out.motor_allowed);
}

void test_cancel_in_calibration(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_CALIB_REST);

    supervisor_in_t in = {0};
    in.ui_event = UI_EV_CANCEL;
    supervisor_out_t out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_CHECK_ENC, out.state);
}

void test_cancel_in_run_goes_to_arming(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_RUN);

    supervisor_in_t in = {0};
    in.ui_event = UI_EV_CANCEL;
    supervisor_out_t out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_ARMING, out.state);
    TEST_ASSERT_FALSE(out.motor_allowed);
}

// --- Sobrepaso de ciclo -------------------------------------------------------
void test_overrun_three_consecutive_faults_two_not(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_RUN);

    supervisor_in_t ov = {0};
    ov.emg_rest = true; // seguir en RUN no exige reposo, pero evita cualquier confusión
    ov.cycle_overrun = true;

    // Dos sobrepasos no alcanzan.
    TEST_ASSERT_EQUAL_INT(ST_RUN, step(&sv, ov).state);
    TEST_ASSERT_EQUAL_INT(ST_RUN, step(&sv, ov).state);

    // Un tick limpio reinicia el contador.
    supervisor_in_t clean = {0};
    clean.emg_rest = true;
    TEST_ASSERT_EQUAL_INT(ST_RUN, step(&sv, clean).state);

    // Ahora sí: tres seguidos disparan FAULT_OVERRUN.
    TEST_ASSERT_EQUAL_INT(ST_RUN, step(&sv, ov).state);
    TEST_ASSERT_EQUAL_INT(ST_RUN, step(&sv, ov).state);
    supervisor_out_t out = step(&sv, ov);
    TEST_ASSERT_EQUAL_INT(ST_FAULT, out.state);
    TEST_ASSERT_TRUE(out.faults & FAULT_OVERRUN);
    TEST_ASSERT_FALSE(out.motor_allowed);
}

// --- Rearme: dos rutas según la causa -----------------------------------------
void test_rearm_after_enc_fault_goes_to_calib_rom(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_RUN);

    supervisor_in_t in = {0};
    in.enc_fault = FAULT_I2C;
    TEST_ASSERT_EQUAL_INT(ST_FAULT, step(&sv, in).state);

    // Antes de REARM_OK_TICKS sigue en FAULT.
    supervisor_out_t out = stable_ticks(&sv, REARM_OK_TICKS - 1u);
    TEST_ASSERT_EQUAL_INT(ST_FAULT, out.state);

    // El tick número REARM_OK_TICKS rearma: encoder involucrado -> CALIB_ROM.
    out = step(&sv, (supervisor_in_t){0});
    TEST_ASSERT_EQUAL_INT(ST_CALIB_ROM, out.state);
    TEST_ASSERT_EQUAL_INT(FAULT_NONE, out.faults);
}

void test_rearm_after_overrun_goes_to_arming(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_RUN);

    supervisor_in_t ov = {0};
    ov.cycle_overrun = true;
    step(&sv, ov);
    step(&sv, ov);
    TEST_ASSERT_EQUAL_INT(ST_FAULT, step(&sv, ov).state);

    // Sin bits de encoder -> conserva la calibración y rearma a ARMING.
    supervisor_out_t out = stable_ticks(&sv, REARM_OK_TICKS);
    TEST_ASSERT_EQUAL_INT(ST_ARMING, out.state);
    TEST_ASSERT_EQUAL_INT(FAULT_NONE, out.faults);

    // Y desde ARMING solo pasa a RUN con ambos en reposo.
    out = step(&sv, (supervisor_in_t){0});
    TEST_ASSERT_EQUAL_INT(ST_ARMING, out.state);
    supervisor_in_t rest = {0};
    rest.emg_rest = true;
    out = step(&sv, rest);
    TEST_ASSERT_EQUAL_INT(ST_RUN, out.state);
}

void test_rearm_resets_on_relapse(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_RUN);

    supervisor_in_t in = {0};
    in.enc_fault = FAULT_I2C;
    step(&sv, in);

    stable_ticks(&sv, 100u); // progreso parcial del rearme

    // Una recaída (sobrepaso) reinicia el contador: no debe rearmar pronto.
    supervisor_in_t ov = {0};
    ov.cycle_overrun = true;
    step(&sv, ov);

    supervisor_out_t out = stable_ticks(&sv, REARM_OK_TICKS - 1u);
    TEST_ASSERT_EQUAL_INT(ST_FAULT, out.state);
    out = step(&sv, (supervisor_in_t){0});
    TEST_ASSERT_EQUAL_INT(ST_CALIB_ROM, out.state); // la causa seguía siendo de encoder
}

// --- Saturación de EMG en marcha ----------------------------------------------
void test_emg_saturation_faults_in_run(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_RUN);

    supervisor_in_t in = {0};
    in.emg_saturated = true;
    supervisor_out_t out = step(&sv, in);
    TEST_ASSERT_EQUAL_INT(ST_FAULT, out.state);
    TEST_ASSERT_TRUE(out.faults & FAULT_EMG_SAT);
    TEST_ASSERT_FALSE(out.motor_allowed);

    // Saturación no es falla de encoder: rearma a ARMING.
    out = stable_ticks(&sv, REARM_OK_TICKS);
    TEST_ASSERT_EQUAL_INT(ST_ARMING, out.state);
}

// --- motor_allowed solo en RUN (barrido) --------------------------------------
void test_motor_allowed_only_in_run(void) {
    const sys_state_t states[] = {ST_CHECK_ENC, ST_CALIB_ROM, ST_CALIB_REST,
                                  ST_CALIB_MVC,  ST_ARMING,    ST_RUN};
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
        supervisor_t sv;
        supervisor_init(&sv);
        supervisor_out_t out = drive_to(&sv, states[i]);
        // step() ya verifica la invariante en cada transición; acá la reconfirmamos.
        TEST_ASSERT_EQUAL_INT(out.state == ST_RUN, out.motor_allowed);
    }
}

// =============================================================================
//  Integración: supervisor + cadena de control (misma que app_tick en ST_RUN).
// =============================================================================
#define LO_DEG 5.0f
#define HI_DEG 125.0f
#define ANGLE_PER_DUTY_DEG 0.2f

static direction_t dir_from_intent(intent_t in) {
    switch (in) {
    case INTENT_FLEX:
        return DIR_FLEX;
    case INTENT_EXT:
        return DIR_EXT;
    default:
        return DIR_STOP;
    }
}

// Un ciclo completo: supervisor decide, y si permite el motor corre intent->limits->
// power y mueve un brazo de juguete. Devuelve si el supervisor quedó en RUN.
static bool chain_tick(supervisor_t *sv, power_state_t *ps, float *angle, bool bic, bool tri,
                       float intensity) {
    supervisor_in_t in = {0};
    in.emg_rest = !bic && !tri;
    supervisor_out_t out = step(sv, in);

    if (out.motor_allowed) {
        const intent_t it = intent_decide(bic, tri);
        const direction_t dir = dir_from_intent(it);
        const float factor = limits_factor(dir, *angle, LO_DEG, HI_DEG);
        const float intens = (dir == DIR_STOP) ? 0.0f : intensity;
        float target = power_target(intens, factor);
        if (target > DUTY_CEIL) {
            target = DUTY_CEIL;
        }
        const bool hard =
            (it == INTENT_COCONTRACTION) || limits_must_stop(dir, *angle, LO_DEG, HI_DEG);
        power_step(ps, dir, target, hard);
        if (ps->dir == DIR_FLEX) {
            *angle -= ps->duty * ANGLE_PER_DUTY_DEG;
        } else if (ps->dir == DIR_EXT) {
            *angle += ps->duty * ANGLE_PER_DUTY_DEG;
        }
    } else {
        power_step(ps, DIR_STOP, 0.0f, true);
    }
    return out.state == ST_RUN;
}

void test_integration_cocontraction_cuts_but_stays_run(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_ARMING);

    power_state_t ps;
    power_init(&ps);
    float angle = 60.0f; // lejos de los límites

    // Entra a RUN con un tick de reposo y luego flexiona un rato.
    TEST_ASSERT_TRUE(chain_tick(&sv, &ps, &angle, false, false, 1.0f));
    for (int i = 0; i < 400; ++i) {
        chain_tick(&sv, &ps, &angle, true, false, 1.0f);
    }
    TEST_ASSERT_TRUE(ps.duty > 0.0f);
    TEST_ASSERT_EQUAL_INT(DIR_FLEX, ps.dir);

    // Co-contracción: la cadena corta al instante; el supervisor NO sale de RUN.
    const bool still_run = chain_tick(&sv, &ps, &angle, true, true, 1.0f);
    TEST_ASSERT_TRUE(still_run);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);
    TEST_ASSERT_EQUAL_INT(DIR_STOP, ps.dir);
}

void test_integration_limit_cuts_but_stays_run(void) {
    supervisor_t sv;
    supervisor_init(&sv);
    drive_to(&sv, ST_ARMING);

    power_state_t ps;
    power_init(&ps);
    float angle = 40.0f;

    TEST_ASSERT_TRUE(chain_tick(&sv, &ps, &angle, false, false, 1.0f)); // -> RUN

    bool still_run = true;
    int guard = 0;
    while (ps.duty > 0.0f || angle > LO_DEG + 1.0f) {
        still_run = chain_tick(&sv, &ps, &angle, true, false, 1.0f);
        TEST_ASSERT_TRUE(++guard < 200000);
    }
    // Llegó al límite: cadena cortada (duty 0) y supervisor sigue en RUN.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ps.duty);
    TEST_ASSERT_TRUE(still_run);
    TEST_ASSERT_TRUE(angle <= LO_DEG + 1e-3f);
}

// =============================================================================
//  Detector de salto de ángulo.
// =============================================================================
void test_angle_watch_first_sample_never_jumps(void) {
    angle_watch_t w;
    angle_watch_reset(&w);
    // Primera muestra tras reset: solo fija la referencia, nunca marca salto.
    TEST_ASSERT_FALSE(angle_watch_jump(&w, 1000.0f));
    TEST_ASSERT_FALSE(angle_watch_jump(&w, 1000.0f + ANGLE_JUMP_MAX_DEG * 0.5f));
    TEST_ASSERT_TRUE(angle_watch_jump(&w, 1000.0f + ANGLE_JUMP_MAX_DEG * 3.0f));
}

void test_angle_watch_reset_ignores_previous_movement(void) {
    angle_watch_t w;
    angle_watch_reset(&w);
    angle_watch_jump(&w, 100.0f);
    TEST_ASSERT_FALSE(angle_watch_jump(&w, 101.0f)); // salto chico
    TEST_ASSERT_TRUE(angle_watch_jump(&w, 200.0f));  // salto grande

    // Como al entrar a ARMING/RUN: el reset hace que el salto previo no cuente.
    angle_watch_reset(&w);
    TEST_ASSERT_FALSE(angle_watch_jump(&w, 300.0f)); // 100° de diferencia, pero tras reset
    TEST_ASSERT_FALSE(angle_watch_jump(&w, 301.0f));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_sequence_boot_to_run);
    RUN_TEST(test_enc_fault_in_calib_rom);
    RUN_TEST(test_enc_fault_in_calib_rest);
    RUN_TEST(test_enc_fault_in_calib_mvc);
    RUN_TEST(test_recalibrate_from_run);
    RUN_TEST(test_cancel_in_calibration);
    RUN_TEST(test_cancel_in_run_goes_to_arming);
    RUN_TEST(test_overrun_three_consecutive_faults_two_not);
    RUN_TEST(test_rearm_after_enc_fault_goes_to_calib_rom);
    RUN_TEST(test_rearm_after_overrun_goes_to_arming);
    RUN_TEST(test_rearm_resets_on_relapse);
    RUN_TEST(test_emg_saturation_faults_in_run);
    RUN_TEST(test_motor_allowed_only_in_run);
    RUN_TEST(test_integration_cocontraction_cuts_but_stays_run);
    RUN_TEST(test_integration_limit_cuts_but_stays_run);
    RUN_TEST(test_angle_watch_first_sample_never_jumps);
    RUN_TEST(test_angle_watch_reset_ignores_previous_movement);
    return UNITY_END();
}
