// session.c · Implementación de la sesión y del emisor del JSON @SESION. Lógica pura:
// solo usa rep_stats, config y la librería estándar (snprintf/strncpy). Ver session.h.

#include "session.h"

#include <stdio.h>
#include <string.h>

void session_make_id(char *out, uint64_t rnd) {
    // "s" + 16 dígitos hex. Único por sesión: el valor aleatorio no se repite tras un
    // reinicio (a diferencia de un contador en RAM), así la API no lo descarta por duplicado.
    (void)snprintf(out, SESSION_ID_MAX, "s%016llx", (unsigned long long)rnd);
}

void session_init(session_t *s) {
    s->active = false;
    rep_stats_init(&s->rs);
    s->result.id_envio[0] = '\0';
    s->result.id_paciente = 0;
    s->result.brazo = 'i';
    s->result.rom_calibrado_grados = 0.0f;
    s->result.duracion_s = 0u;
    s->result.n_reps = 0u;
    s->tx.active = false;
    s->tx.stage = 3; // terminado: no hay nada que enviar
    s->tx.rep_i = 0u;
    s->tx.seg_len = 0u;
    s->tx.seg_pos = 0u;
}

void session_begin(session_t *s, const char *id_envio, int32_t id_paciente, char brazo) {
    s->active = true;
    rep_stats_init(&s->rs);
    strncpy(s->result.id_envio, id_envio, SESSION_ID_MAX - 1u);
    s->result.id_envio[SESSION_ID_MAX - 1u] = '\0';
    s->result.id_paciente = id_paciente;
    s->result.brazo = (brazo == 'd') ? 'd' : 'i';
    s->result.rom_calibrado_grados = 0.0f;
    s->result.duracion_s = 0u;
    s->result.n_reps = 0u;
}

void session_run_enter(session_t *s) {
    rep_stats_init(&s->rs);
}

// Agrega una repetición al resultado con su número de orden, respetando el tope.
static void append_rep(session_t *s, const rep_t *rep) {
    if (s->result.n_reps >= SESSION_MAX_REPS) {
        return; // tope alcanzado (= límite de la API): se descartan las excedentes
    }
    ui_rep_t *d = &s->result.repeticiones[s->result.n_reps];
    d->numero = (uint16_t)(s->result.n_reps + 1u);
    d->rango_grados = rep->rango_grados;
    d->asistencia_pct = rep->asistencia_pct;
    d->duracion_ms = rep->duracion_ms;
    s->result.n_reps++;
}

void session_sample(session_t *s, float angle_deg, float duty_applied, bool motor_active,
                    uint32_t now_ms) {
    if (!s->active) {
        return;
    }
    rep_t rep;
    if (rep_stats_sample(&s->rs, angle_deg, duty_applied, motor_active, now_ms, &rep)) {
        append_rep(s, &rep);
    }
}

void session_end(session_t *s, uint32_t now_ms, uint32_t run_ms, float rom_deg) {
    rep_t rep;
    if (s->active && rep_stats_finish(&s->rs, now_ms, &rep)) {
        append_rep(s, &rep); // cierra y guarda la repetición en curso
    }
    s->result.rom_calibrado_grados = rom_deg;
    s->result.duracion_s = run_ms / 1000u;
    s->active = false;
}

void session_cancel(session_t *s) { s->active = false; }

const ui_session_result_t *session_result(const session_t *s) { return &s->result; }

bool session_is_active(const session_t *s) { return s->active; }
int32_t session_patient(const session_t *s) { return s->result.id_paciente; }
char session_arm(const session_t *s) { return s->result.brazo; }
uint16_t session_reps(const session_t *s) { return s->result.n_reps; }

float session_last_rep_range(const session_t *s) {
    if (s->result.n_reps == 0u) {
        return 0.0f;
    }
    return s->result.repeticiones[s->result.n_reps - 1u].rango_grados;
}

bool session_should_send(const ui_session_result_t *r, const char **motivo) {
    if (r->id_paciente == 0) {
        if (motivo != NULL) {
            *motivo = "sin paciente (id 0)";
        }
        return false;
    }
    if (r->n_reps == 0u) {
        if (motivo != NULL) {
            *motivo = "sin repeticiones validas";
        }
        return false;
    }
    return true;
}

// =============================================================================
//  Emisor del JSON @SESION, por partes (sin un buffer gigante para 500 reps).
// =============================================================================

// Escribe `src` escapado como contenido de string JSON en `dst` (termina en '\0'). Escapa
// la comilla doble y la barra invertida; el id generado no trae otros caracteres, pero se
// escapa igual por robustez. No desborda `dst` (capacidad `cap`).
static void json_escape(char *dst, size_t cap, const char *src) {
    size_t j = 0u;
    for (size_t i = 0u; src[i] != '\0' && (j + 2u) < cap; ++i) {
        const char c = src[i];
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
        }
        dst[j++] = c;
    }
    dst[j] = '\0';
}

// --- Escritores de cada fragmento del JSON (única fuente del formato) ---------
// Los usan tanto el emisor por partes para USB (fill_segment) como el armado de una
// sola pasada para la API por Wi-Fi (session_build_body), así el formato no se duplica.
// Devuelven lo mismo que snprintf (los caracteres que se habrían escrito sin el '\0').

// Cabecera del JSON. Con `with_prefix` antepone "@SESION " (línea por USB); sin él, empieza
// directamente en '{' (cuerpo HTTP para POST /api/sesion).
static int write_header(char *buf, size_t cap, const ui_session_result_t *r, bool with_prefix) {
    char id_esc[SESSION_ID_MAX * 2u];
    json_escape(id_esc, sizeof(id_esc), r->id_envio);
    const char *brazo = (r->brazo == 'd') ? "derecho" : "izquierdo";
    return snprintf(buf, cap,
                    "%s{\"id_envio\":\"%s\",\"id_paciente\":%ld,\"brazo\":\"%s\","
                    "\"rom_calibrado_grados\":%.1f,\"duracion_s\":%lu,\"repeticiones\":[",
                    with_prefix ? "@SESION " : "", id_esc, (long)r->id_paciente, brazo,
                    (double)r->rom_calibrado_grados, (unsigned long)r->duracion_s);
}

// Una repetición. `first` controla la coma separadora (la primera no la lleva).
static int write_rep(char *buf, size_t cap, const ui_rep_t *rep, bool first) {
    return snprintf(buf, cap,
                    "%s{\"numero\":%u,\"rango_grados\":%.1f,\"asistencia_pct\":%.1f,"
                    "\"duracion_ms\":%lu}",
                    first ? "" : ",", (unsigned)rep->numero, (double)rep->rango_grados,
                    (double)rep->asistencia_pct, (unsigned long)rep->duracion_ms);
}

// Cierre. Con `with_newline` termina en '\n' (línea por USB); sin él, en '}' (cuerpo HTTP).
static int write_close(char *buf, size_t cap, bool with_newline) {
    return snprintf(buf, cap, with_newline ? "]}\n" : "]}");
}

// Formatea el segmento siguiente (cabecera, una repetición o cierre) en tx.seg. Devuelve
// false cuando ya no queda nada por emitir.
static bool fill_segment(session_t *s) {
    session_tx_t *t = &s->tx;
    const ui_session_result_t *r = &s->result;
    int m = 0;

    if (t->stage == 0) { // cabecera (con prefijo "@SESION " para la línea por USB)
        m = write_header(t->seg, sizeof(t->seg), r, true);
        t->stage = (r->n_reps > 0u) ? 1 : 2;
    } else if (t->stage == 1) { // una repetición
        m = write_rep(t->seg, sizeof(t->seg), &r->repeticiones[t->rep_i], t->rep_i == 0u);
        t->rep_i++;
        if (t->rep_i >= r->n_reps) {
            t->stage = 2;
        }
    } else if (t->stage == 2) { // cierre (con '\n' final para la línea por USB)
        m = write_close(t->seg, sizeof(t->seg), true);
        t->stage = 3;
    } else {
        return false; // terminado
    }

    t->seg_len = (m > 0 && (size_t)m < sizeof(t->seg)) ? (size_t)m : 0u;
    t->seg_pos = 0u;
    return true;
}

size_t session_build_body(const session_t *s, char *buf, size_t cap) {
    const ui_session_result_t *r = &s->result;
    // Cabecera sin el prefijo "@SESION " ni '\n': es el cuerpo exacto de POST /api/sesion.
    int m = write_header(buf, cap, r, false);
    if (m < 0 || (size_t)m >= cap) {
        return 0u; // no entra (o error de formato)
    }
    size_t n = (size_t)m;
    for (uint16_t i = 0u; i < r->n_reps; ++i) {
        m = write_rep(buf + n, cap - n, &r->repeticiones[i], i == 0u);
        if (m < 0 || (size_t)m >= cap - n) {
            return 0u; // no entra el JSON completo: el llamador avisa y deja el respaldo por USB
        }
        n += (size_t)m;
    }
    m = write_close(buf + n, cap - n, false);
    if (m < 0 || (size_t)m >= cap - n) {
        return 0u;
    }
    return n + (size_t)m;
}

void session_tx_begin(session_t *s) {
    s->tx.active = true;
    s->tx.stage = 0;
    s->tx.rep_i = 0u;
    s->tx.seg_len = 0u;
    s->tx.seg_pos = 0u;
}

size_t session_tx_next(session_t *s, char *buf, size_t cap, bool *done) {
    session_tx_t *t = &s->tx;
    size_t n = 0u;
    for (;;) {
        if (t->seg_pos >= t->seg_len) {
            if (!fill_segment(s)) {
                break; // no queda más por emitir
            }
        }
        if (n >= cap) {
            break; // se llenó el buffer de salida; se sigue en la próxima llamada
        }
        const size_t avail = t->seg_len - t->seg_pos;
        const size_t room = cap - n;
        const size_t take = (avail < room) ? avail : room;
        memcpy(buf + n, t->seg + t->seg_pos, take);
        t->seg_pos += take;
        n += take;
    }
    *done = (t->stage == 3) && (t->seg_pos >= t->seg_len);
    t->active = !(*done);
    return n;
}

bool session_tx_busy(const session_t *s) { return s->tx.active; }
