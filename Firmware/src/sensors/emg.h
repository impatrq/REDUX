// emg.h · Un canal EMG: rectificado opcional, envolvente exponencial, umbral con
// histéresis (más tiempo mínimo sobre el umbral), intensidad 0–1 y detección de
// saturación. Lógica pura: recibe la muestra ya leída (no toca el ADC) y guarda su
// estado en un emg_channel_t que pasa el llamador, sin variables globales ocultas.
// No decide la dirección del motor. Depende solo de config y util.
#ifndef REDUX_EMG_H
#define REDUX_EMG_H

#include <stdbool.h>
#include <stdint.h>

// Estado completo de un canal EMG. El llamador declara uno por músculo (bíceps y
// tríceps) y lo pasa por puntero; emg no mantiene estado interno propio.
typedef struct {
    float alpha;          // coeficiente del filtro exponencial (de EMG_ENV_CUTOFF_HZ y el tick)
    float env;            // envolvente actual, en cuentas de ADC
    bool active;          // estado de activación con histéresis
    bool saturated;       // la señal cruda estuvo saturada EMG_SAT_MS seguidos
    bool calibrated;      // umbrales instalados: la activación está habilitada
    uint32_t above_ticks; // ticks consecutivos con env >= thr_on
    uint32_t sat_ticks;   // ticks consecutivos con la muestra cruda > EMG_SAT_COUNTS
    float thr_on;         // umbral de activación (cuentas de envolvente)
    float thr_off;        // umbral de desactivación (< thr_on): histéresis
    float env_at_max;     // envolvente que corresponde a intensidad 1.0 (del MVC)
} emg_channel_t;

// Deja el canal en reposo y sin umbrales válidos (no se activa hasta calibrar).
// Calcula alpha a partir de EMG_ENV_CUTOFF_HZ y CTRL_TICK_US.
void emg_init(emg_channel_t *ch);

// Procesa UNA muestra ya leída y promediada (cuentas de 12 bits) y devuelve la
// envolvente actualizada. Pensada para llamarse una vez por tick. Si EMG_RAW_SIGNAL
// vale 1, resta EMG_OFFSET_COUNTS y rectifica; si vale 0, usa la muestra tal cual.
float emg_update(emg_channel_t *ch, uint16_t raw);

// true si el canal está activo (histéresis + tiempo mínimo sobre el umbral).
bool emg_is_active(const emg_channel_t *ch);

// Intensidad acotada a 0.0–1.0: 0.0 en el umbral de activación, 1.0 en el máximo
// calibrado. Devuelve 0.0 si el canal no tiene umbrales/máximo válidos.
float emg_intensity(const emg_channel_t *ch);

// true si la señal cruda estuvo por encima de EMG_SAT_COUNTS durante EMG_SAT_MS.
bool emg_is_saturated(const emg_channel_t *ch);

// Envolvente actual (cuentas). Útil para telemetría y calibración.
float emg_envelope(const emg_channel_t *ch);

// Umbral de activación actual (cuentas de envolvente); 0.0 si no está calibrado.
float emg_threshold_on(const emg_channel_t *ch);

// --- Instalación de la calibración (las usa emg_calib) -----------------------
// Fija los umbrales de activación/desactivación y habilita la activación.
void emg_set_thresholds(emg_channel_t *ch, float thr_on, float thr_off);

// Fija la envolvente que corresponde a intensidad 1.0.
void emg_set_max(emg_channel_t *ch, float env_at_max);

// --- Función pura (se prueba en PC) ------------------------------------------
// Coeficiente del filtro exponencial de primer orden para una frecuencia de corte
// y un período de tick: RC = 1/(2π·fc), alpha = dt/(RC + dt), con dt = tick_us·1e-6.
float emg_env_alpha(float cutoff_hz, uint32_t tick_us);

#endif // REDUX_EMG_H
