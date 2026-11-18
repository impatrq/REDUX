// rep_stats.h · Estadística por repetición del ejercicio, lógica pura (sin hardware).
// Una repetición va del inicio de una flexión al inicio de la siguiente. Convención de
// ángulo (Fase 3): 0° = flexión máxima; flexionar BAJA el ángulo, extender lo SUBE. El
// detector segmenta las reps en los picos de extensión (donde arranca la flexión) usando
// una zona muerta (REP_DIR_DEADBAND_DEG) para tolerar el ruido del ángulo y los
// micro-cambios de dirección. Se alimenta una muestra por tick y no depende del motor:
// las repeticiones se cuentan aunque la asistencia sea nula. Se prueba en la PC.
#ifndef REDUX_REP_STATS_H
#define REDUX_REP_STATS_H

#include <stdbool.h>
#include <stdint.h>

// Estadística de una repetición ya cerrada. El número de repetición lo asigna el llamador
// (la sesión), no este módulo.
typedef struct {
    float rango_grados;   // ángulo máximo − mínimo durante la repetición
    float asistencia_pct; // media del duty aplicado / DUTY_CEIL × 100 mientras el motor
                          // estuvo activo; 0 si el motor no se activó en la repetición
    uint32_t duracion_ms; // duración de la repetición (ms)
} rep_t;

// Estado del detector/acumulador. Un struct en cero es válido (equivale a rep_stats_init).
typedef struct {
    bool have_ref;     // ya se tomó la primera muestra de referencia
    bool rep_open;     // hay una repetición en curso (tras la primera flexión detectada)
    int8_t move_dir;   // sentido con zona muerta: +1 extiende (sube), −1 flexiona (baja), 0 inicial
    float extreme;     // ángulo del extremo local vigente (pivote de la reversión)
    float min_angle;   // mínimo de la repetición en curso
    float max_angle;   // máximo de la repetición en curso
    uint32_t start_ms; // instante de inicio de la repetición en curso
    double duty_sum;   // suma del duty aplicado mientras el motor estuvo activo (en la rep)
    uint32_t active_n; // muestras con el motor activo (en la rep)
} rep_stats_t;

// Deja el detector sin referencia ni repetición abierta. Llamar al entrar a RUN.
void rep_stats_init(rep_stats_t *rs);

// Alimenta una muestra del lazo (ángulo, duty aplicado, si el motor estuvo activo e
// instante en ms). Al detectar el inicio de una nueva flexión cierra la repetición en
// curso: si es válida (rango ≥ REP_MIN_RANGE_DEG y duración ≥ REP_MIN_DURATION_MS)
// devuelve true y escribe sus estadísticas en *out; las repeticiones descartadas no se
// reportan. No bloquea.
bool rep_stats_sample(rep_stats_t *rs, float angle_deg, float duty_applied, bool motor_active,
                      uint32_t now_ms, rep_t *out);

// Cierra la repetición en curso (al terminar la sesión). Devuelve true y escribe *out si
// la repetición abierta resultó válida; false si no había rep abierta o fue descartada.
bool rep_stats_finish(rep_stats_t *rs, uint32_t now_ms, rep_t *out);

#endif // REDUX_REP_STATS_H
