// supervisor.h · Máquina de estados de seguridad: arranque, calibración, rearme,
// marcha, falla. Depende de redux_types y config. No escribe en el PWM.
//
// Es lógica pura: todo el estado vive en un supervisor_t que pasa el llamador, sin
// variables globales. Por eso se compila y se prueba en la PC. Una llamada por tick:
// recibe las entradas del ciclo (estado del encoder, calibración, EMG, sobrepaso,
// evento de ui) y decide el estado y si el motor puede moverse. El motor solo se
// mueve en ST_RUN; cualquier falla lleva a ST_FAULT con corte inmediato.
#ifndef REDUX_SUPERVISOR_H
#define REDUX_SUPERVISOR_H

#include <stdbool.h>
#include <stdint.h>

#include "redux_types.h"

// Entradas de un tick. El llamador (app) las arma a partir de los sensores y la ui.
typedef struct {
    fault_t enc_fault;     // encoder: 0 = OK, o FAULT_I2C / FAULT_MAGNET / FAULT_ANGLE_JUMP
    bool calib_done;       // la calibración activa en este estado terminó y es válida
    bool calib_failed;     // la calibración activa falló (se queda para reintentar)
    bool emg_rest;         // ambos músculos en reposo (requisito para pasar a ST_RUN)
    bool emg_saturated;    // algún canal de EMG saturado (falla en marcha)
    bool cycle_overrun;    // este ciclo excedió CTRL_TICK_US (medición o ticks perdidos)
    ui_event_t ui_event;   // evento de la interfaz (CONFIRM, CANCEL, RECALIBRATE)
} supervisor_in_t;

// Salidas de un tick.
typedef struct {
    sys_state_t state;  // estado resultante
    bool motor_allowed; // true SOLO en ST_RUN; en cualquier otro estado es false
    fault_t faults;     // causa de falla vigente (máscara); FAULT_NONE si no hay
} supervisor_out_t;

// Estado interno de la máquina. Un struct en cero NO es válido: usar supervisor_init.
typedef struct {
    sys_state_t state;      // estado actual
    fault_t faults;         // causa de la falla vigente (se fija al entrar a ST_FAULT)
    uint32_t rearm_ticks;   // lecturas estables acumuladas en ST_FAULT
    uint32_t overrun_count; // sobrepasos de ciclo consecutivos
} supervisor_t;

// Deja la máquina en ST_BOOT, sin fallas ni contadores. Llamar una vez al arrancar.
void supervisor_init(supervisor_t *sv);

// Avanza la máquina un tick con las entradas `in` y escribe el resultado en `out`.
// Orden de decisión en cada estado: primero las fallas de seguridad (llevan a
// ST_FAULT en el mismo tick, con motor_allowed = false), luego los eventos de ui,
// y por último la progresión normal. Garantiza out->motor_allowed == (state == ST_RUN).
void supervisor_step(supervisor_t *sv, const supervisor_in_t *in, supervisor_out_t *out);

#endif // REDUX_SUPERVISOR_H
