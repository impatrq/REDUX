// session.h · Sesión de un paciente: junta la calibración del rango con la estadística
// por repetición (control/rep_stats) y arma el resultado que consume la API y, a futuro,
// la pantalla. Lógica pura (sin hardware ni SDK): el id único y los tiempos los pasa el
// llamador (app), así se prueba en la PC. El resultado se publica por USB como una línea
// "@SESION " + el JSON exacto que espera POST /api/sesion, emitida por partes para no
// bloquear el ciclo de control.
#ifndef REDUX_SESSION_H
#define REDUX_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h" // SESSION_MAX_REPS, SESSION_ID_MAX

#include "rep_stats.h"

// Una repetición dentro del resultado (con su número de orden ya asignado).
typedef struct {
    uint16_t numero;
    float rango_grados;
    float asistencia_pct;
    uint32_t duracion_ms;
} ui_rep_t;

// Resultado completo de una sesión. Es lo que publica el firmware y lo que leerá la
// pantalla. `brazo` se guarda como 'i' (izquierdo) o 'd' (derecho); el JSON lo expande.
typedef struct {
    char id_envio[SESSION_ID_MAX]; // "s" + 16 hex; único por sesión (ver session_make_id)
    int32_t id_paciente;           // 0 = sin paciente (la sesión no se guarda)
    char brazo;                    // 'i' o 'd'
    float rom_calibrado_grados;    // recorrido calibrado del paciente
    uint32_t duracion_s;           // tiempo en marcha (ST_RUN) de la sesión
    uint16_t n_reps;               // repeticiones guardadas (<= SESSION_MAX_REPS)
    ui_rep_t repeticiones[SESSION_MAX_REPS];
} ui_session_result_t;

// Estado del emisor del JSON por partes. Interno; lo maneja session_tx_*.
typedef struct {
    bool active;     // hay un envío en curso
    int stage;       // 0 cabecera, 1 repeticiones, 2 cierre, 3 terminado
    uint16_t rep_i;  // próxima repetición a emitir
    size_t seg_len;  // bytes válidos en el segmento actual
    size_t seg_pos;  // bytes ya entregados del segmento actual
    char seg[192];   // segmento formateado en curso (cabecera, una repetición o cierre)
} session_tx_t;

// Estado de una sesión. El llamador lo pasa por puntero; sin globales. Un struct en cero
// NO es válido: usar session_init.
typedef struct {
    bool active;                 // sesión en curso (entre begin y end)
    rep_stats_t rs;              // detector/acumulador de repeticiones
    ui_session_result_t result;  // resultado acumulado
    session_tx_t tx;             // emisor del JSON
} session_t;

// Escribe en `out` (capacidad SESSION_ID_MAX) un id único derivado de un valor aleatorio
// de 64 bits: "s" seguido de 16 dígitos hexadecimales. El valor lo provee el llamador
// (en el firmware, el RNG por hardware); acá es un parámetro para poder probarlo en la PC.
void session_make_id(char *out, uint64_t rnd);

// Deja la sesión inactiva, sin resultado ni envío en curso.
void session_init(session_t *s);

// Arranca una sesión: fija id_envio, id_paciente y brazo ('i'/'d'), reinicia la
// estadística y el resultado. No toca el emisor en curso del resultado anterior.
void session_begin(session_t *s, const char *id_envio, int32_t id_paciente, char brazo);

// Reinicia el detector de repeticiones al (re)entrar a marcha (ST_RUN), para que una
// recalibración a mitad de sesión no deje una repetición a caballo entre dos tramos.
void session_run_enter(session_t *s);

// Alimenta una muestra del lazo mientras la sesión está activa (ver rep_stats_sample).
// Las repeticiones válidas se agregan al resultado hasta SESSION_MAX_REPS.
void session_sample(session_t *s, float angle_deg, float duty_applied, bool motor_active,
                    uint32_t now_ms);

// Cierra la sesión: finaliza la repetición en curso, fija el ROM calibrado y la duración
// en segundos (a partir de los ms acumulados en marcha). Deja la sesión inactiva.
void session_end(session_t *s, uint32_t now_ms, uint32_t run_ms, float rom_deg);

// Abandona la sesión sin publicarla (cancelación durante la calibración).
void session_cancel(session_t *s);

// Acceso de solo lectura al resultado acumulado (para el envío).
const ui_session_result_t *session_result(const session_t *s);

// Consultas para el reporte de estado.
bool session_is_active(const session_t *s);
int32_t session_patient(const session_t *s);
char session_arm(const session_t *s);
uint16_t session_reps(const session_t *s);

// Rango (grados) de la última repetición cerrada; 0 si todavía no hay ninguna. Para el
// campo last_rom_deg de ui_status_t (lo muestra el panel en marcha).
float session_last_rep_range(const session_t *s);

// Decide si el resultado debe enviarse a la API. Devuelve false (y, si `motivo` no es
// NULL, el motivo) cuando id_paciente == 0 o no hay repeticiones válidas.
bool session_should_send(const ui_session_result_t *r, const char **motivo);

// --- Emisor del JSON @SESION, por partes y sin bloquear ----------------------
// Arranca el envío del resultado actual desde el principio.
void session_tx_begin(session_t *s);

// Copia hasta `cap` bytes del JSON en `buf` y devuelve cuántos copió. Deja *done en true
// cuando ya emitió la línea completa ("@SESION " + JSON + '\n'). No bloquea.
size_t session_tx_next(session_t *s, char *buf, size_t cap, bool *done);

// true mientras haya un envío del resultado en curso.
bool session_tx_busy(const session_t *s);

#endif // REDUX_SESSION_H
