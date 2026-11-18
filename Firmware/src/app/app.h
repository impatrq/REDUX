// app.h · Orquesta un ciclo de control completo por llamada (app_init, app_tick).
// Conoce a todas las capas y decide el orden del ciclo. No tiene lógica que podría
// vivir en control/. Es la única capa, junto a main.c y hal/, que habla con el
// hardware (a través del HAL y los drivers); por eso no se prueba en la PC: lo que se
// prueba es el supervisor (lógica pura) y la cadena de control.
#ifndef REDUX_APP_H
#define REDUX_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "redux_types.h"

// Instantánea del estado del ciclo para el reporte por USB (lo arma main.c). No es la
// telemetría completa en core 1 (eso es Fase 7): son los campos mínimos de la Fase 6.
typedef struct {
    sys_state_t state;    // estado del supervisor
    fault_t faults;       // causa de falla vigente (máscara)
    float angle_deg;      // ángulo articular (0 si el ROM no está calibrado)
    int32_t encoder_raw;  // última lectura cruda del AS5600 (0..4095), 0 si falló el I2C
    float env_bic;        // envolvente/nivel del bíceps
    float env_tri;        // envolvente/nivel del tríceps
    float intensity_bic;  // intensidad del bíceps [0,1]
    float intensity_tri;  // intensidad del tríceps [0,1]
    direction_t dir;      // sentido aplicado al motor
    float duty;           // duty aplicado [0, DUTY_CEIL]
    bool sim_bic;         // bíceps simulado (fuente serie): calibración autocompletada
    bool sim_tri;         // tríceps simulado (fuente serie): calibración autocompletada
    bool session_active;  // hay una sesión en curso
    int32_t session_patient; // id del paciente de la sesión (0 = sin paciente)
    char session_arm;     // brazo de la sesión: 'i' o 'd'
    uint16_t session_reps; // repeticiones contadas hasta ahora
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

// Alimenta un comando de la fuente de EMG simulada:
//   "b N" -> bíceps al N %   "t N" -> tríceps al N %   "r" -> reposo de los simulados.
// "b N"/"t N" solo valen si ese canal es SERIAL; si es ADC avisa y consume la línea igual.
// Devuelve true si consumió la línea; false si no la reconoce (para que el llamador la
// pase a la interfaz de usuario).
bool app_feed_sim(const char *line);

// Procesa un comando de sesión por la terminal:
//   "sesion <id_paciente> <i|d>" -> inicia una sesión desde ST_IDLE (i = izquierdo,
//        d = derecho; id_paciente 0 = sin paciente, la sesión no se guarda);
//   "fin" -> termina la sesión en marcha (equivale a "cancel" en RUN).
// Valida el estado y el encoder y avisa por la interfaz. Devuelve true si consumió la
// línea; false si no es un comando de sesión (para que el llamador la pase a la interfaz).
bool app_feed_session(const char *line);

// --- Envío del resultado de la sesión (@SESION), no bloqueante ---------------
// true mientras haya un resultado de sesión pendiente de enviar por USB.
bool app_session_tx_busy(void);

// Copia hasta `cap` bytes del JSON @SESION pendiente en `buf`; devuelve cuántos copió y
// deja *done en true al terminar la línea. Pensada para drenar de a poco en el lazo
// principal, fuera de la medición del ciclo de control.
size_t app_session_tx_next(char *buf, size_t cap, bool *done);

#endif // REDUX_APP_H
