// emg_calib.h · Calibración de un canal EMG, no bloqueante (una muestra de envolvente
// por tick). Dos tomas: reposo (media y desvío -> umbrales con histéresis) y
// contracción máxima (pico -> envolvente de intensidad 1.0). Instala el resultado en
// el emg_channel_t destino. No mueve el motor. Depende de emg y config (no de hal_time:
// las duraciones CAL_*_MS se convierten a cantidad de muestras con CTRL_TICK_US).
#ifndef REDUX_EMG_CALIB_H
#define REDUX_EMG_CALIB_H

#include <stdbool.h>
#include <stdint.h>

#include "emg.h" // emg_channel_t

// Fase del procedimiento de calibración de un canal.
typedef enum {
    EMG_CAL_IDLE,     // sin empezar
    EMG_CAL_REST_RUN, // tomando reposo (acumula media y desvío)
    EMG_CAL_MVC_RUN,  // tomando contracción máxima (busca el pico)
    EMG_CAL_DONE,     // la última toma terminó y es válida
    EMG_CAL_FAILED,   // la última toma falló (ver error)
} emg_cal_phase_t;

// Motivo por el que falló la última toma.
typedef enum {
    EMG_CAL_ERR_NONE,       // sin error
    EMG_CAL_ERR_REST_NOISE, // desvío en reposo demasiado alto (electrodo flojo)
    EMG_CAL_ERR_WEAK_MVC,   // la contracción máxima fue demasiado débil
} emg_cal_error_t;

// Estado de una calibración. Hay uno por canal (el MVC necesita la media y el umbral
// que dejó el reposo de ese mismo canal). El llamador lo pasa por puntero; sin globales.
typedef struct {
    emg_channel_t *ch;     // canal destino (al terminar se instala el resultado)
    emg_cal_phase_t phase;
    emg_cal_error_t error;
    uint32_t n;            // muestras acumuladas en la toma actual
    uint32_t n_target;     // muestras objetivo de la toma actual
    double sum;            // acumulador para la media (reposo)
    double sum_sq;         // acumulador para el desvío (reposo)
    float peak;            // pico de envolvente (MVC)
    float rest_mean;       // media de reposo, guardada para validar el MVC
    float rest_std;        // desvío de reposo (informativo)
    float thr_on;          // umbral de activación resultante del reposo
    float env_at_max;      // envolvente de intensidad 1.0 resultante del MVC
} emg_calib_t;

// Arranca la toma de reposo del canal: deja la máquina en EMG_CAL_REST_RUN.
void emg_calib_rest_begin(emg_calib_t *c, emg_channel_t *ch);

// Arranca la toma de contracción máxima del canal. Usa la media y el umbral que dejó
// el reposo previo del mismo canal (debe haberse calibrado el reposo antes).
void emg_calib_mvc_begin(emg_calib_t *c, emg_channel_t *ch);

// Alimenta UNA muestra de envolvente (la que devolvió emg_update ese tick). Al
// completar la cantidad objetivo, calcula el resultado, lo valida, lo instala en el
// canal y pasa a EMG_CAL_DONE o EMG_CAL_FAILED. No bloquea.
void emg_calib_add(emg_calib_t *c, float env);

// true si la última toma terminó y fue válida.
bool emg_calib_is_done(const emg_calib_t *c);

// true si la última toma falló (consultar el campo error).
bool emg_calib_failed(const emg_calib_t *c);

#endif // REDUX_EMG_CALIB_H
