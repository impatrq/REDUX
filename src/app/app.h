// app.h · Orquesta un ciclo de control completo por llamada (app_init, app_tick).
// Conoce a todas las capas y decide el orden del ciclo. No tiene lógica que podría
// vivir en control/. Es la única capa, junto a main.c y hal/, que habla con el
// hardware (a través del HAL y los drivers); por eso no se prueba en la PC: lo que se
// prueba es el supervisor (lógica pura) y la cadena de control.
#ifndef REDUX_APP_H
#define REDUX_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "redux_types.h"

// Instantánea del estado del ciclo para el reporte por USB (lo arma main.c). No es la
// telemetría completa en core 1 (eso es Fase 7): son los campos mínimos de la Fase 6.
typedef struct {
    sys_state_t state;    // estado del supervisor
    fault_t faults;       // causa de falla vigente (máscara)
    float angle_deg;      // ángulo articular (0 si el ROM no está calibrado)
    float env_bic;        // envolvente/nivel del bíceps
    float env_tri;        // envolvente/nivel del tríceps
    float intensity_bic;  // intensidad del bíceps [0,1]
    float intensity_tri;  // intensidad del tríceps [0,1]
    direction_t dir;      // sentido aplicado al motor
    float duty;           // duty aplicado [0, DUTY_CEIL]
    bool emg_simulated;   // true si la señal EMG viene de serie (banco) y la calib es simulada
} app_status_t;

// Inicializa el supervisor, los módulos y el estado del ciclo. Llamar una vez tras
// inicializar el HAL. No habilita el watchdog (se habilita al armar, dentro de app_tick).
void app_init(void);

// Ejecuta un ciclo de control completo, en el orden fijo del plan. `loop_behind` indica
// que el lazo principal detectó ticks perdidos desde el ciclo anterior (se cuenta como
// sobrepaso). Llamar una vez por tick de 1 ms.
void app_tick(bool loop_behind);

// Copia la última instantánea del ciclo. No bloquea. Pensada para el reporte por USB.
void app_get_status(app_status_t *out);

// Alimenta un comando de la fuente de EMG simulada (solo con EMG_SOURCE_SERIAL):
//   "b N" -> bíceps al N %   "t N" -> tríceps al N %   "r" -> ambos en reposo.
// Devuelve true si consumió la línea; false si no la reconoce (para que el llamador la
// pase a la interfaz de usuario). Con EMG_SOURCE_ADC siempre devuelve false.
bool app_feed_sim(const char *line);

#endif // REDUX_APP_H
