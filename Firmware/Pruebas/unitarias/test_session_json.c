// test_session_json.c · Pruebas del módulo de sesión: generación del id único (formato),
// decisión de envío (id 0 o sin reps -> no se envía), duración en segundos (solo marcha)
// y armado del JSON @SESION con el formato EXACTO que espera POST /api/sesion, incluidos
// los escapes del id, el caso de 0 repeticiones y el de 500.

#include "unity.h"

#include <string.h>

#include "config.h"

#include "session.h"

void setUp(void) {}
void tearDown(void) {}

static char g_buf[48000]; // alcanza para el JSON de 500 repeticiones

// Reensambla todo el JSON de la sesión en `out`, drenándolo en trozos chicos (7 bytes)
// para ejercitar el corte en cualquier posición.
static void drain_all(session_t *s, char *out, size_t cap) {
    session_tx_begin(s);
    size_t total = 0u;
    bool done = false;
    while (!done) {
        char chunk[7];
        const size_t m = session_tx_next(s, chunk, sizeof(chunk), &done);
        for (size_t i = 0u; i < m && total < cap - 1u; ++i) {
            out[total++] = chunk[i];
        }
    }
    out[total] = '\0';
}

static int count_substr(const char *hay, const char *needle) {
    int c = 0;
    const char *p = hay;
    const size_t l = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
        c++;
        p += l;
    }
    return c;
}

// --- id_envio: "s" + 16 hex, con ceros a la izquierda -------------------------
void test_make_id_format(void) {
    char id[SESSION_ID_MAX];
    session_make_id(id, 0x3f9a1c00deadbeefULL);
    TEST_ASSERT_EQUAL_STRING("s3f9a1c00deadbeef", id);

    session_make_id(id, 0x1ULL);
    TEST_ASSERT_EQUAL_STRING("s0000000000000001", id);
}

// --- should_send: no se envía con id 0 ni sin repeticiones --------------------
void test_should_send_rules(void) {
    ui_session_result_t r;
    memset(&r, 0, sizeof(r));
    const char *motivo = NULL;

    r.id_paciente = 5;
    r.n_reps = 2u;
    TEST_ASSERT_TRUE(session_should_send(&r, &motivo));

    r.id_paciente = 0;
    motivo = NULL;
    TEST_ASSERT_FALSE(session_should_send(&r, &motivo));
    TEST_ASSERT_EQUAL_STRING("sin paciente (id 0)", motivo);

    r.id_paciente = 5;
    r.n_reps = 0u;
    motivo = NULL;
    TEST_ASSERT_FALSE(session_should_send(&r, &motivo));
    TEST_ASSERT_EQUAL_STRING("sin repeticiones validas", motivo);
}

// --- duracion_s: solo el tiempo en marcha -------------------------------------
void test_duration_from_run_ms(void) {
    session_t s;
    session_init(&s);
    session_begin(&s, "sx", 9, 'i');
    session_end(&s, 12345u, 42000u, 100.0f); // run_ms = 42 s de marcha
    TEST_ASSERT_EQUAL_UINT32(42u, s.result.duracion_s);
}

// --- JSON exacto con una repetición -------------------------------------------
void test_json_one_rep_exact(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "sdeadbeef00000001");
    s.result.id_paciente = 7;
    s.result.brazo = 'd';
    s.result.rom_calibrado_grados = 120.5f;
    s.result.duracion_s = 42u;
    s.result.n_reps = 1u;
    s.result.repeticiones[0].numero = 1u;
    s.result.repeticiones[0].rango_grados = 90.0f;
    s.result.repeticiones[0].asistencia_pct = 50.0f;
    s.result.repeticiones[0].duracion_ms = 900u;

    drain_all(&s, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING(
        "@SESION {\"id_envio\":\"sdeadbeef00000001\",\"id_paciente\":7,\"brazo\":\"derecho\","
        "\"rom_calibrado_grados\":120.5,\"duracion_s\":42,\"repeticiones\":["
        "{\"numero\":1,\"rango_grados\":90.0,\"asistencia_pct\":50.0,\"duracion_ms\":900}]}\n",
        g_buf);
}

// --- JSON con 0 repeticiones --------------------------------------------------
void test_json_zero_reps_exact(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "s0000000000000000");
    s.result.id_paciente = 3;
    s.result.brazo = 'i';
    s.result.rom_calibrado_grados = 0.0f;
    s.result.duracion_s = 0u;
    s.result.n_reps = 0u;

    drain_all(&s, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING(
        "@SESION {\"id_envio\":\"s0000000000000000\",\"id_paciente\":3,\"brazo\":\"izquierdo\","
        "\"rom_calibrado_grados\":0.0,\"duracion_s\":0,\"repeticiones\":[]}\n",
        g_buf);
}

// --- JSON con escapes en el id_envio ------------------------------------------
void test_json_escapes_in_id(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "x\"y\\z"); // contiene una comilla y una barra invertida
    s.result.id_paciente = 3;
    s.result.brazo = 'i';
    s.result.rom_calibrado_grados = 0.0f;
    s.result.duracion_s = 0u;
    s.result.n_reps = 0u;

    drain_all(&s, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING(
        "@SESION {\"id_envio\":\"x\\\"y\\\\z\",\"id_paciente\":3,\"brazo\":\"izquierdo\","
        "\"rom_calibrado_grados\":0.0,\"duracion_s\":0,\"repeticiones\":[]}\n",
        g_buf);
}

// --- JSON con 500 repeticiones ------------------------------------------------
void test_json_max_reps(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "sfull");
    s.result.id_paciente = 11;
    s.result.brazo = 'd';
    s.result.rom_calibrado_grados = 150.0f;
    s.result.duracion_s = 600u;
    s.result.n_reps = (uint16_t)SESSION_MAX_REPS;
    for (uint16_t i = 0u; i < (uint16_t)SESSION_MAX_REPS; ++i) {
        s.result.repeticiones[i].numero = (uint16_t)(i + 1u);
        s.result.repeticiones[i].rango_grados = 90.0f;
        s.result.repeticiones[i].asistencia_pct = 50.0f;
        s.result.repeticiones[i].duracion_ms = 900u;
    }

    drain_all(&s, g_buf, sizeof(g_buf));

    // Las 500 repeticiones están presentes y bien numeradas (primera y última).
    TEST_ASSERT_EQUAL_INT((int)SESSION_MAX_REPS, count_substr(g_buf, "\"numero\":"));
    TEST_ASSERT_NOT_NULL(strstr(g_buf, "\"repeticiones\":[{\"numero\":1,"));
    TEST_ASSERT_NOT_NULL(strstr(g_buf, "\"numero\":500,\"rango_grados\":90.0,"
                                       "\"asistencia_pct\":50.0,\"duracion_ms\":900}]}\n"));
    // Sin coma de más: no hay "[," ni "},]" ni ",]".
    TEST_ASSERT_NULL(strstr(g_buf, "[,"));
    TEST_ASSERT_NULL(strstr(g_buf, ",]"));
}

// --- session_build_body: cuerpo HTTP (sin "@SESION " ni '\n') --------------------
void test_build_body_one_rep_exact(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "sdeadbeef00000001");
    s.result.id_paciente = 7;
    s.result.brazo = 'd';
    s.result.rom_calibrado_grados = 120.5f;
    s.result.duracion_s = 42u;
    s.result.n_reps = 1u;
    s.result.repeticiones[0].numero = 1u;
    s.result.repeticiones[0].rango_grados = 90.0f;
    s.result.repeticiones[0].asistencia_pct = 50.0f;
    s.result.repeticiones[0].duracion_ms = 900u;

    const size_t n = session_build_body(&s, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING(
        "{\"id_envio\":\"sdeadbeef00000001\",\"id_paciente\":7,\"brazo\":\"derecho\","
        "\"rom_calibrado_grados\":120.5,\"duracion_s\":42,\"repeticiones\":["
        "{\"numero\":1,\"rango_grados\":90.0,\"asistencia_pct\":50.0,\"duracion_ms\":900}]}",
        g_buf);
    TEST_ASSERT_EQUAL_UINT(strlen(g_buf), n);
}

void test_build_body_zero_reps_exact(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "s0000000000000000");
    s.result.id_paciente = 3;
    s.result.brazo = 'i';
    s.result.rom_calibrado_grados = 0.0f;
    s.result.duracion_s = 0u;
    s.result.n_reps = 0u;

    const size_t n = session_build_body(&s, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING(
        "{\"id_envio\":\"s0000000000000000\",\"id_paciente\":3,\"brazo\":\"izquierdo\","
        "\"rom_calibrado_grados\":0.0,\"duracion_s\":0,\"repeticiones\":[]}",
        g_buf);
    TEST_ASSERT_EQUAL_UINT(strlen(g_buf), n);
}

// El cuerpo completo entra en SESSION_API_BODY_MAX incluso con 500 repeticiones.
void test_build_body_max_reps_fits(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "sfull");
    s.result.id_paciente = 11;
    s.result.brazo = 'd';
    s.result.rom_calibrado_grados = 150.0f;
    s.result.duracion_s = 600u;
    s.result.n_reps = (uint16_t)SESSION_MAX_REPS;
    for (uint16_t i = 0u; i < (uint16_t)SESSION_MAX_REPS; ++i) {
        s.result.repeticiones[i].numero = (uint16_t)(i + 1u);
        s.result.repeticiones[i].rango_grados = 90.0f;
        s.result.repeticiones[i].asistencia_pct = 50.0f;
        s.result.repeticiones[i].duracion_ms = 900u;
    }

    static char body[SESSION_API_BODY_MAX];
    const size_t n = session_build_body(&s, body, sizeof(body));
    TEST_ASSERT_TRUE(n > 0u);                     // entró (no devolvió 0)
    TEST_ASSERT_EQUAL_UINT(strlen(body), n);
    TEST_ASSERT_EQUAL_INT((int)SESSION_MAX_REPS, count_substr(body, "\"numero\":"));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"duracion_ms\":900}]}"));  // cierra sin '\n'
}

// Si el buffer no alcanza, devuelve 0 y no desborda (el llamador deja el respaldo por USB).
void test_build_body_too_small(void) {
    session_t s;
    session_init(&s);
    strcpy(s.result.id_envio, "sx");
    s.result.id_paciente = 1;
    s.result.brazo = 'i';
    s.result.n_reps = 1u;
    s.result.repeticiones[0].numero = 1u;

    char tiny[16];
    const size_t n = session_build_body(&s, tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_UINT(0u, n);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_make_id_format);
    RUN_TEST(test_should_send_rules);
    RUN_TEST(test_duration_from_run_ms);
    RUN_TEST(test_json_one_rep_exact);
    RUN_TEST(test_json_zero_reps_exact);
    RUN_TEST(test_json_escapes_in_id);
    RUN_TEST(test_json_max_reps);
    RUN_TEST(test_build_body_one_rep_exact);
    RUN_TEST(test_build_body_zero_reps_exact);
    RUN_TEST(test_build_body_max_reps_fits);
    RUN_TEST(test_build_body_too_small);
    return UNITY_END();
}
