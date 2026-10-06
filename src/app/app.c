// app.c · Implementación de un ciclo: leer sensores → supervisor → intent → limits →
// power → driver. Orden fijo del plan (sección "Orden fijo de un ciclo"). Mide el
// tiempo de cada ciclo para detectar sobrepasos y habilita el watchdog al armar.

#include "app.h"

#include <stdlib.h> // atoi (solo fuente simulada)

#include "config.h"

#include "angle_watch.h"
#include "supervisor.h"

#include "intent.h"
#include "limits.h"
#include "power.h"

#include "as5600.h"
#include "rom_calib.h"

#include "bts7960.h"

#include "board.h"
#include "hal_adc.h"
#include "hal_time.h"
#include "hal_wdt.h"

#include "ui.h"

#if EMG_SOURCE == EMG_SOURCE_ADC
#include "emg.h"
#include "emg_calib.h"
#endif

// --- Estado del ciclo (sin globales ocultas fuera de este módulo) -------------
static supervisor_t g_sv;
static supervisor_out_t g_out;
static power_state_t g_ps;
static angle_watch_t g_aw;
static rom_calib_t g_rom;

static bool g_wdt_on = false;      // el watchdog se habilita una sola vez (al armar)
static uint32_t g_tick = 0u;       // contador de ticks (para la relectura del imán)
static bool g_last_overrun = false; // sobrepaso medido en el ciclo anterior
static fault_t g_magnet_fault = FAULT_NONE; // última lectura del estado del imán
static rom_phase_t g_rom_last_phase = ROM_IDLE;

static app_status_t g_status;

// --- Entradas de EMG de un tick ----------------------------------------------
typedef struct {
    bool bic_on;
    bool tri_on;
    float inten_b;
    float inten_t;
    float env_b;
    float env_t;
    bool saturated;
    bool rest; // ambos en reposo
} emg_in_t;

static direction_t dir_from_intent(intent_t in) {
    switch (in) {
    case INTENT_FLEX:
        return DIR_FLEX;
    case INTENT_EXT:
        return DIR_EXT;
    default: // INTENT_REST o INTENT_COCONTRACTION
        return DIR_STOP;
    }
}

// =============================================================================
//  Fuente de EMG: real por ADC, o simulada por serie (banco). Según EMG_SOURCE.
// =============================================================================
#if EMG_SOURCE == EMG_SOURCE_ADC

static emg_channel_t g_ch_b, g_ch_t;
static emg_calib_t g_cal_b, g_cal_t;

// Sub-secuencia de la toma de contracción máxima (bíceps y luego tríceps).
typedef enum { MVC_B_WAIT, MVC_B_RUN, MVC_T_WAIT, MVC_T_RUN, MVC_DONE } mvc_step_t;
static mvc_step_t g_mvc = MVC_B_WAIT;

static uint16_t read_oversampled(uint8_t adc_channel) {
    uint32_t acc = 0u;
    for (uint32_t i = 0u; i < EMG_OVERSAMPLE; ++i) {
        acc += hal_adc_read(adc_channel);
    }
    return (uint16_t)(acc / EMG_OVERSAMPLE);
}

static void read_emg(emg_in_t *e) {
    const float env_b = emg_update(&g_ch_b, read_oversampled(ADC_CH_BICEPS));
    const float env_t = emg_update(&g_ch_t, read_oversampled(ADC_CH_TRICEPS));
    e->env_b = env_b;
    e->env_t = env_t;
    e->bic_on = emg_is_active(&g_ch_b);
    e->tri_on = emg_is_active(&g_ch_t);
    e->inten_b = emg_intensity(&g_ch_b);
    e->inten_t = emg_intensity(&g_ch_t);
    e->saturated = emg_is_saturated(&g_ch_b) || emg_is_saturated(&g_ch_t);
    e->rest = !e->bic_on && !e->tri_on;
}

// Arranca la toma de reposo de ambos canales al entrar a ST_CALIB_REST.
static void emg_calib_enter_rest(void) {
    emg_calib_rest_begin(&g_cal_b, &g_ch_b);
    emg_calib_rest_begin(&g_cal_t, &g_ch_t);
    ui_message("Calib EMG reposo: relaje ambos musculos y sostenga.");
}

static void emg_calib_enter_mvc(void) {
    g_mvc = MVC_B_WAIT;
    ui_set_waiting(true);
    ui_message("Calib EMG: contraiga BICEPS al maximo y confirme (pulsador u 'ok').");
}

// Alimenta la toma de reposo; devuelve done/failed del par de canales.
static void emg_rest_feed(const emg_in_t *e, bool *done, bool *failed) {
    emg_calib_add(&g_cal_b, e->env_b);
    emg_calib_add(&g_cal_t, e->env_t);
    const bool b_run = (g_cal_b.phase == EMG_CAL_REST_RUN);
    const bool t_run = (g_cal_t.phase == EMG_CAL_REST_RUN);
    if (b_run || t_run) {
        return; // todavía tomando
    }
    *failed = emg_calib_failed(&g_cal_b) || emg_calib_failed(&g_cal_t);
    *done = !*failed;
    ui_message(*done ? "Reposo OK." : "Reposo invalido (electrodo flojo). 'recal'.");
}

// Avanza la sub-secuencia de MVC con el evento y la envolvente del tick.
static void emg_mvc_feed(const emg_in_t *e, ui_event_t ev, bool *done) {
    switch (g_mvc) {
    case MVC_B_WAIT:
        if (ev == UI_EV_CONFIRM) {
            emg_calib_mvc_begin(&g_cal_b, &g_ch_b);
            ui_set_waiting(false);
            g_mvc = MVC_B_RUN;
        }
        break;
    case MVC_B_RUN:
        emg_calib_add(&g_cal_b, e->env_b);
        if (g_cal_b.phase != EMG_CAL_MVC_RUN) {
            if (emg_calib_failed(&g_cal_b)) {
                ui_set_waiting(true);
                ui_message("MVC biceps debil. Reintente y confirme.");
                g_mvc = MVC_B_WAIT;
            } else {
                ui_set_waiting(true);
                ui_message("Biceps OK. Contraiga TRICEPS al maximo y confirme.");
                g_mvc = MVC_T_WAIT;
            }
        }
        break;
    case MVC_T_WAIT:
        if (ev == UI_EV_CONFIRM) {
            emg_calib_mvc_begin(&g_cal_t, &g_ch_t);
            ui_set_waiting(false);
            g_mvc = MVC_T_RUN;
        }
        break;
    case MVC_T_RUN:
        emg_calib_add(&g_cal_t, e->env_t);
        if (g_cal_t.phase != EMG_CAL_MVC_RUN) {
            if (emg_calib_failed(&g_cal_t)) {
                ui_set_waiting(true);
                ui_message("MVC triceps debil. Reintente y confirme.");
                g_mvc = MVC_T_WAIT;
            } else {
                ui_message("Triceps OK. Calibracion EMG completa.");
                g_mvc = MVC_DONE;
            }
        }
        break;
    case MVC_DONE:
    default:
        break;
    }
    *done = (g_mvc == MVC_DONE);
}

static void emg_source_init(void) {
    emg_init(&g_ch_b);
    emg_init(&g_ch_t);
}

#else // EMG_SOURCE == EMG_SOURCE_SERIAL (banco: señal simulada por serie)

static int g_sim_bic_pct = 0;
static int g_sim_tri_pct = 0;

static void read_emg(emg_in_t *e) {
    e->inten_b = (float)g_sim_bic_pct / 100.0f;
    e->inten_t = (float)g_sim_tri_pct / 100.0f;
    e->env_b = (float)g_sim_bic_pct;
    e->env_t = (float)g_sim_tri_pct;
    e->bic_on = (g_sim_bic_pct > 0);
    e->tri_on = (g_sim_tri_pct > 0);
    e->saturated = false; // la señal simulada no satura
    e->rest = !e->bic_on && !e->tri_on;
}

static void emg_source_init(void) {}

#endif

// =============================================================================
//  Lectura del encoder y armado del enc_fault según el estado.
// =============================================================================
static bool is_armed_state(sys_state_t s) {
    return (s == ST_ARMING) || (s == ST_RUN);
}

// Lee el encoder, actualiza el ángulo y devuelve la máscara de fallas del encoder
// apropiada para el estado actual. El salto de ángulo solo se evalúa en los estados
// armados (ARMING, RUN) y con el ROM ya calibrado.
static fault_t read_encoder(sys_state_t state) {
    int32_t raw = 0;
    const status_t st = as5600_read_raw(&raw);
    g_status.encoder_raw = raw; // crudo siempre visible en el reporte (0 si falló el I2C)
    fault_t enc = (st != STATUS_OK) ? FAULT_I2C : FAULT_NONE;

    if ((g_tick % MAGNET_CHECK_TICKS) == 0u) {
        as5600_magnet_t m;
        if (as5600_read_magnet(&m) != STATUS_OK) {
            g_magnet_fault = FAULT_I2C;
        } else if (!m.detected || m.too_weak || m.too_strong) {
            g_magnet_fault = FAULT_MAGNET;
        } else {
            g_magnet_fault = FAULT_NONE;
        }
    }
    enc = (fault_t)(enc | g_magnet_fault);

    if (rom_calib_is_ready(&g_rom) && st == STATUS_OK) {
        g_status.angle_deg = rom_calib_angle_deg(&g_rom, raw);
        if (is_armed_state(state) && angle_watch_jump(&g_aw, g_status.angle_deg)) {
            enc = (fault_t)(enc | FAULT_ANGLE_JUMP);
        }
    }
    return enc;
}

// =============================================================================
//  Calibración activa: done/failed para el supervisor + anuncios por la ui.
// =============================================================================
// Anuncia los cambios de fase del ROM (prompts de flexión/extensión).
static void rom_announce(void) {
    if (g_rom.phase == g_rom_last_phase) {
        return;
    }
    g_rom_last_phase = g_rom.phase;
    switch (g_rom.phase) {
    case ROM_WAIT_FLEX:
        ui_set_waiting(true);
        ui_message("Calib ROM: lleve a FLEXION maxima y confirme (pulsador u 'ok').");
        break;
    case ROM_WAIT_EXT:
        ui_set_waiting(true);
        ui_message("Flexion OK. Lleve a EXTENSION maxima y confirme.");
        break;
    case ROM_SAMPLING_FLEX:
    case ROM_SAMPLING_EXT:
        ui_set_waiting(false);
        ui_message("Midiendo, sostenga la posicion...");
        break;
    case ROM_DONE:
        ui_set_waiting(false);
        ui_message("Calib ROM OK.");
        break;
    case ROM_FAILED:
        ui_set_waiting(false);
        ui_message("Calib ROM invalida. 'recal' para reintentar.");
        break;
    default:
        break;
    }
}

// Procesa la calibración del estado actual y devuelve done/failed para el supervisor.
static void run_calibration(sys_state_t state, const emg_in_t *e, ui_event_t ev, bool *done,
                            bool *failed) {
    *done = false;
    *failed = false;
    switch (state) {
    case ST_CALIB_ROM:
        rom_calib_event(&g_rom, ev);
        rom_calib_tick(&g_rom);
        rom_announce();
        *done = rom_calib_is_ready(&g_rom);
        *failed = (g_rom.phase == ROM_FAILED);
        break;
    case ST_CALIB_REST:
    case ST_CALIB_MVC:
#if EMG_SOURCE == EMG_SOURCE_ADC
        if (state == ST_CALIB_REST) {
            emg_rest_feed(e, done, failed);
        } else {
            emg_mvc_feed(e, ev, done);
        }
#else
        (void)e;
        (void)ev;
        g_status.emg_simulated = true; // calibración de EMG autocompletada (banco)
        *done = true;
#endif
        break;
    default:
        break;
    }
}

// =============================================================================
//  Acciones al entrar a un estado nuevo.
// =============================================================================
static void on_enter(sys_state_t to) {
    switch (to) {
    case ST_CHECK_ENC:
        ui_set_waiting(false);
        ui_message("Encoder: verificando iman y bus...");
        break;
    case ST_CALIB_ROM:
        rom_calib_begin(&g_rom);
        g_rom_last_phase = ROM_IDLE; // fuerza el anuncio de WAIT_FLEX
        break;
    case ST_CALIB_REST:
#if EMG_SOURCE == EMG_SOURCE_ADC
        emg_calib_enter_rest();
#endif
        break;
    case ST_CALIB_MVC:
#if EMG_SOURCE == EMG_SOURCE_ADC
        emg_calib_enter_mvc();
#endif
        break;
    case ST_ARMING:
        if (!g_wdt_on) {
            hal_wdt_enable(WDT_TIMEOUT_MS); // una sola vez, tras calibrar
            g_wdt_on = true;
        }
        power_init(&g_ps);
        bts7960_stop();
        angle_watch_reset(&g_aw); // referencia nueva: ignora el movimiento previo
        ui_set_waiting(false);
        ui_message("Armado: relaje ambos musculos para marchar.");
        break;
    case ST_RUN:
        angle_watch_reset(&g_aw); // referencia nueva al entrar a marcha
        ui_message("En marcha.");
        break;
    case ST_FAULT:
        power_init(&g_ps);
        bts7960_stop();
        ui_set_waiting(false);
        ui_message("FALLA: motor cortado. Rearme tras lecturas estables.");
        break;
    default:
        break;
    }
}

// =============================================================================
//  Cadena de control en marcha (solo ST_RUN).
// =============================================================================
static void run_control(const emg_in_t *e) {
    const intent_t it = intent_decide(e->bic_on, e->tri_on);
    const direction_t dir = dir_from_intent(it);
    const float lo = rom_calib_limit_lo(&g_rom);
    const float hi = rom_calib_limit_hi(&g_rom);
    const float angle = g_status.angle_deg;

    const float factor = limits_factor(dir, angle, lo, hi);
    const float intensity = (dir == DIR_FLEX) ? e->inten_b : (dir == DIR_EXT) ? e->inten_t : 0.0f;
    float target = power_target(intensity, factor);
    if (target > DUTY_CEIL) {
        target = DUTY_CEIL; // techo de banco (BENCH_MODE)
    }
    const bool hard = (it == INTENT_COCONTRACTION) || limits_must_stop(dir, angle, lo, hi);

    power_step(&g_ps, dir, target, hard);
    bts7960_apply(g_ps.dir, g_ps.duty);
}

// Deja el motor cortado y la rampa en reposo (cualquier estado que no sea RUN).
static void stop_motor(void) {
    bts7960_stop();
    power_init(&g_ps);
}

// =============================================================================
//  API pública.
// =============================================================================
void app_init(void) {
    supervisor_init(&g_sv);
    power_init(&g_ps);
    angle_watch_reset(&g_aw);
    rom_calib_begin(&g_rom); // deja g_rom en estado conocido (no calibrado)
    g_rom.phase = ROM_IDLE;
    emg_source_init();
    g_wdt_on = false;
    g_tick = 0u;
    g_last_overrun = false;
    g_magnet_fault = FAULT_NONE;
    g_rom_last_phase = ROM_IDLE;

    g_status.state = ST_BOOT;
    g_status.faults = FAULT_NONE;
    g_status.angle_deg = 0.0f;
    g_status.encoder_raw = 0;
    g_status.dir = DIR_STOP;
    g_status.duty = 0.0f;
    g_status.emg_simulated = (EMG_SOURCE == EMG_SOURCE_SERIAL);

    ui_message("REDUX listo. Confirme (pulsador u 'ok') para iniciar.");
    supervisor_out_t out = {ST_BOOT, false, FAULT_NONE};
    g_out = out;
}

void app_tick(bool loop_behind) {
    const uint64_t t0 = hal_time_us();

    // 1) Watchdog: alimentar una vez por ciclo mientras esté habilitado.
    if (g_wdt_on) {
        hal_wdt_feed();
    }

    // Interfaz: trabajo periódico y un evento por tick (lo ven supervisor y calibración).
    ui_tick();
    const ui_event_t ev = ui_poll();

    // 2) EMG (sobremuestreo real o fuente simulada).
    emg_in_t e;
    read_emg(&e);

    // 3) Encoder: ángulo y fallas según el estado actual.
    const sys_state_t state_now = g_sv.state;
    const fault_t enc_fault = read_encoder(state_now);

    // Calibración del estado actual (su done/failed alimenta al supervisor este tick).
    bool calib_done = false;
    bool calib_failed = false;
    run_calibration(state_now, &e, ev, &calib_done, &calib_failed);

    // 4) Supervisor: decide el estado y si el motor puede moverse.
    supervisor_in_t in;
    in.enc_fault = enc_fault;
    in.calib_done = calib_done;
    in.calib_failed = calib_failed;
    in.emg_rest = e.rest;
    in.emg_saturated = e.saturated;
    in.cycle_overrun = g_last_overrun || loop_behind;
    in.ui_event = ev;

    const sys_state_t prev = g_sv.state;
    supervisor_step(&g_sv, &in, &g_out);
    if (g_out.state != prev) {
        on_enter(g_out.state);
    }

    // 5-6) Cadena de control o corte según el permiso del supervisor.
    if (g_out.motor_allowed) {
        run_control(&e);
    } else {
        stop_motor();
    }

    // Instantánea para el reporte (no entra en la medición del ciclo: es asignación).
    g_status.state = g_out.state;
    g_status.faults = g_out.faults;
    g_status.env_bic = e.env_b;
    g_status.env_tri = e.env_t;
    g_status.intensity_bic = e.inten_b;
    g_status.intensity_tri = e.inten_t;
    g_status.dir = g_ps.dir;
    g_status.duty = g_ps.duty;

    g_tick++;

    // Medición del ciclo: el reporte por USB queda fuera (lo hace main.c).
    const uint64_t elapsed = hal_time_us() - t0;
    g_last_overrun = (elapsed > CTRL_TICK_US);
}

void app_get_status(app_status_t *out) {
    *out = g_status;
}

bool app_feed_sim(const char *line) {
#if EMG_SOURCE == EMG_SOURCE_SERIAL
    if (line[0] == 'r' && line[1] == '\0') {
        g_sim_bic_pct = 0;
        g_sim_tri_pct = 0;
        return true;
    }
    if ((line[0] == 'b' || line[0] == 't') && line[1] == ' ') {
        int pct = atoi(&line[2]);
        if (pct < 0) {
            pct = 0;
        } else if (pct > 100) {
            pct = 100;
        }
        if (line[0] == 'b') {
            g_sim_bic_pct = pct;
        } else {
            g_sim_tri_pct = pct;
        }
        return true;
    }
    return false;
#else
    (void)line;
    return false;
#endif
}
