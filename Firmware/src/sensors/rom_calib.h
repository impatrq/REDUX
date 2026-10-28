// rom_calib.h · Calibración del rango de movimiento (ROM) del paciente, no bloqueante.
// Convención: 0° = flexión máxima; el ángulo crece al extender; rom_deg = extensión
// máxima. El flujo es: el usuario lleva el brazo a flexión máxima y CONFIRMA (raw_flex),
// luego a extensión máxima y CONFIRMA (raw_ext); se calculan sign y rom_deg y se validan.
// Recibe eventos por ui_event_t (nunca lee el pulsador) y lee el ángulo por as5600.
// La calibración vive en RAM (persistirla en flash es de la Fase 7).
#ifndef REDUX_ROM_CALIB_H
#define REDUX_ROM_CALIB_H

#include <stdbool.h>
#include <stdint.h>

#include "redux_types.h" // ui_event_t

// Fase del procedimiento de calibración.
typedef enum {
    ROM_IDLE,          // sin empezar
    ROM_WAIT_FLEX,     // esperando que el usuario confirme la flexión máxima
    ROM_SAMPLING_FLEX, // promediando muestras para raw_flex (una por tick)
    ROM_WAIT_EXT,      // esperando que el usuario confirme la extensión máxima
    ROM_SAMPLING_EXT,  // promediando muestras para raw_ext
    ROM_DONE,          // calibración válida: rom_deg, sign y límites listos
    ROM_FAILED,        // el último intento falló (ver error); pedir RECALIBRAR
} rom_phase_t;

// Motivo por el que falló el último paso de calibración.
typedef enum {
    ROM_ERR_NONE,     // sin error
    ROM_ERR_MAGNET,   // imán no detectado o fuera de rango (MD/ML/MH)
    ROM_ERR_UNSTABLE, // la dispersión de las muestras superó ROM_STABLE_MAX_DEG
    ROM_ERR_RANGE,    // rom_deg fuera de [ROM_MIN_DEG, ROM_MAX_DEG]
    ROM_ERR_BUS,      // error del bus I2C durante la lectura
} rom_error_t;

// Estado completo de una calibración. El llamador lo pasa por puntero; sin globales.
typedef struct {
    rom_phase_t phase;
    rom_error_t error;
    int32_t raw_flex; // lectura cruda en flexión máxima (referencia de θ)
    int32_t raw_ext;  // lectura cruda en extensión máxima
    int32_t sign;     // +1 o -1: hace que la extensión sea positiva
    float rom_deg;    // recorrido total en grados
} rom_calib_t;

// Arranca la calibración: deja la máquina en ROM_WAIT_FLEX.
void rom_calib_begin(rom_calib_t *rc);

// Procesa un evento de la interfaz:
//   UI_EV_CONFIRM     en WAIT_FLEX/WAIT_EXT -> arranca el promedio de ese extremo;
//   UI_EV_RECALIBRATE -> reinicia desde WAIT_FLEX;
//   UI_EV_CANCEL      -> vuelve a ROM_IDLE.
// La lectura promediada se completa de a una muestra por tick en rom_calib_tick().
void rom_calib_event(rom_calib_t *rc, ui_event_t ev);

// Avanza la calibración un tick: si se está promediando, toma una muestra y, al
// completar el lote, valida y pasa al paso siguiente. No bloquea. Pensada para
// llamarse una vez por tick de 1 ms.
void rom_calib_tick(rom_calib_t *rc);

// true cuando la calibración terminó y es válida (phase == ROM_DONE).
bool rom_calib_is_ready(const rom_calib_t *rc);

// --- Funciones puras sobre una calibración válida ----------------------------
// Ángulo de trabajo en grados para una lectura cruda:
//   θ = sign × wrap_diff(raw, raw_flex) × 360/AS5600_COUNTS.
float rom_calib_angle_deg(const rom_calib_t *rc, int32_t raw);

// Límite inferior de trabajo en grados: ROM_MARGIN_DEG.
float rom_calib_limit_lo(const rom_calib_t *rc);

// Límite superior de trabajo en grados: rom_deg - ROM_MARGIN_DEG.
float rom_calib_limit_hi(const rom_calib_t *rc);

#endif // REDUX_ROM_CALIB_H
