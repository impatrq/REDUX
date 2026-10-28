// supervisor.c · Implementación de la máquina de estados y sus transiciones.
// Ver el diagrama y las reglas en docs/state_machine.md. Lógica pura, sin hardware.

#include "supervisor.h"

#include "config.h"

// Entra a ST_FAULT acumulando la causa. Reinicia los contadores de rearme y de
// sobrepaso para que el episodio de falla arranque limpio.
static void enter_fault(supervisor_t *sv, fault_t cause) {
    sv->state = ST_FAULT;
    sv->faults = (fault_t)(sv->faults | cause);
    sv->rearm_ticks = 0u;
    sv->overrun_count = 0u;
}

// Estado de calibración siguiente en la secuencia ROM -> REST -> MVC -> ARMING.
static sys_state_t next_calib_state(sys_state_t s) {
    switch (s) {
    case ST_CALIB_ROM:
        return ST_CALIB_REST;
    case ST_CALIB_REST:
        return ST_CALIB_MVC;
    case ST_CALIB_MVC:
        return ST_ARMING;
    default:
        return s;
    }
}

// Causa de falla en un estado armado (ARMING, RUN): fallas del encoder de este tick
// más FAULT_OVERRUN si se acumularon CTRL_OVERRUN_MAX sobrepasos seguidos.
static fault_t armed_fault_cause(const supervisor_t *sv, const supervisor_in_t *in) {
    fault_t cause = in->enc_fault;
    if (sv->overrun_count >= CTRL_OVERRUN_MAX) {
        cause = (fault_t)(cause | FAULT_OVERRUN);
    }
    return cause;
}

void supervisor_init(supervisor_t *sv) {
    sv->state = ST_BOOT;
    sv->faults = FAULT_NONE;
    sv->rearm_ticks = 0u;
    sv->overrun_count = 0u;
}

void supervisor_step(supervisor_t *sv, const supervisor_in_t *in, supervisor_out_t *out) {
    // Contador de sobrepasos consecutivos (se evalúa en los estados armados).
    if (in->cycle_overrun) {
        sv->overrun_count++;
    } else {
        sv->overrun_count = 0u;
    }

    switch (sv->state) {
    case ST_BOOT:
        // Arranca cuando el usuario lo pide (pulsador u "ok").
        if (in->ui_event == UI_EV_CONFIRM) {
            sv->state = ST_CHECK_ENC;
        }
        break;

    case ST_CHECK_ENC:
        // Espera, con el motor apagado, a que el encoder esté sano. No falla acá: si el
        // imán o el bus no responden, no se avanza a la calibración.
        if (in->ui_event == UI_EV_RECALIBRATE) {
            sv->state = ST_CALIB_ROM;
        } else if (in->enc_fault == FAULT_NONE) {
            sv->state = ST_CALIB_ROM;
        }
        break;

    case ST_CALIB_ROM:
    case ST_CALIB_REST:
    case ST_CALIB_MVC:
        // Seguridad primero: una falla del encoder durante la calibración va a FAULT.
        if (in->enc_fault != FAULT_NONE) {
            enter_fault(sv, in->enc_fault);
            break;
        }
        if (in->ui_event == UI_EV_RECALIBRATE) {
            sv->state = ST_CALIB_ROM; // reinicia toda la calibración
            break;
        }
        if (in->ui_event == UI_EV_CANCEL) {
            sv->state = ST_CHECK_ENC; // descarta la calibración en curso
            break;
        }
        if (in->calib_done) {
            sv->state = next_calib_state(sv->state);
        }
        // calib_failed: se queda en el estado; el módulo de calibración reintenta.
        break;

    case ST_ARMING: {
        const fault_t cause = armed_fault_cause(sv, in);
        if (cause != FAULT_NONE) {
            enter_fault(sv, cause);
            break;
        }
        if (in->ui_event == UI_EV_RECALIBRATE) {
            sv->state = ST_CALIB_ROM;
            break;
        }
        // CANCEL en ARMING no hace nada: el motor ya está apagado.
        // A RUN solo con ambos músculos en reposo (regla de rearme).
        if (in->emg_rest) {
            sv->state = ST_RUN;
        }
        break;
    }

    case ST_RUN: {
        fault_t cause = armed_fault_cause(sv, in);
        if (in->emg_saturated) {
            cause = (fault_t)(cause | FAULT_EMG_SAT);
        }
        if (cause != FAULT_NONE) {
            enter_fault(sv, cause);
            break;
        }
        if (in->ui_event == UI_EV_RECALIBRATE) {
            sv->state = ST_CALIB_ROM;
            break;
        }
        if (in->ui_event == UI_EV_CANCEL) {
            sv->state = ST_ARMING; // sale de marcha; el motor queda apagado
            break;
        }
        // Co-contracción y límite alcanzado NO cambian el estado: el corte inmediato
        // lo hace la cadena de control (hard_stop); el supervisor sigue en RUN.
        break;
    }

    case ST_FAULT:
        // Rearme: cuenta lecturas estables. Cualquier recaída reinicia el contador.
        if (in->enc_fault == FAULT_NONE && !in->emg_saturated && !in->cycle_overrun) {
            sv->rearm_ticks++;
            if (sv->rearm_ticks >= REARM_OK_TICKS) {
                // Si la falla involucró al encoder, el rango ya no es confiable y hay
                // que recalibrar; si no (OVERRUN, EMG_SAT), se conserva la calibración.
                const bool was_enc = (sv->faults & FAULT_ENC_MASK) != 0u;
                sv->faults = FAULT_NONE;
                sv->rearm_ticks = 0u;
                sv->overrun_count = 0u;
                sv->state = was_enc ? ST_CALIB_ROM : ST_ARMING;
            }
        } else {
            sv->rearm_ticks = 0u;
        }
        break;

    default:
        break;
    }

    out->state = sv->state;
    out->faults = sv->faults;
    // Invariante: el motor solo puede moverse en ST_RUN.
    out->motor_allowed = (sv->state == ST_RUN);
}
