/**
 * @file hal_api.c
 * @brief POST /api/sesion por Wi-Fi con lwIP crudo (TCP raw), sin bloquear.
 *
 * Máquina de estados atendida desde hal_api_poll() (core 1). Las operaciones de lwIP que
 * inicia el lazo van entre cyw43_arch_lwip_begin()/end(); dentro de los callbacks de lwIP
 * (conexión, envío, recepción, error) se llama directo (ya corren en su contexto). Mismo
 * criterio que hal_wifi.c. Ver hal_api.h.
 */
#include "hal/hal_api.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "ui/ui_config.h"

/* Config local (IP de la compu, puerto, clave). api_config.h está en .gitignore; si falta,
 * se compila sin envío (host vacío) y con un aviso para copiar la plantilla. */
#if defined(__has_include)
#  if __has_include("ui/api_config.h")
#    include "ui/api_config.h"
#    define HAL_API_CONFIG_PRESENT 1
#  endif
#endif

#ifndef HAL_API_CONFIG_PRESENT
#  pragma GCC diagnostic push
#  pragma GCC diagnostic warning "-Wcpp"
#  warning "sin api_config.h: copia src/ui/api_config.example.h a src/ui/api_config.h"
#  pragma GCC diagnostic pop
#  define REDUX_API_HOST ""
#  define REDUX_API_PORT 5000
#  define REDUX_API_KEY  ""
#endif

/* Fase interna de un intento de envío. */
typedef enum {
    PH_IDLE = 0,
    PH_CONNECTING,  /* tcp_connect en curso */
    PH_WRITING,     /* enviando encabezados + cuerpo */
    PH_WAITING      /* esperando la respuesta */
} hal_api_phase_t;

static hal_api_state_t s_state = HAL_API_IDLE;
static hal_api_phase_t s_phase = PH_IDLE;
static bool s_config_ok = false;

/* Cuerpo a enviar (NO se copia: lo mantiene vivo el core 0 mientras hal_api_busy()). */
static const char *s_body = NULL;
static uint32_t s_body_len = 0u;
static uint32_t s_body_off = 0u;
static int32_t s_paciente = 0;

/* Encabezados de la petición (se rearman en cada intento). */
static char s_header[UI_API_HEADER_MAX];
static uint32_t s_header_len = 0u;
static uint32_t s_header_off = 0u;

/* Conexión y tiempos. */
static struct tcp_pcb *s_pcb = NULL;
static uint32_t s_attempt = 0u;        /* intentos realizados (1..UI_API_RETRY_MAX) */
static uint32_t s_phase_start_ms = 0u; /* para los timeouts de cada fase */
static uint32_t s_retry_at_ms = 0u;    /* cuándo arrancar el próximo intento */
static bool s_want_start = false;      /* hay un pedido nuevo para arrancar */

/* Respuesta HTTP (línea de estado + principio del cuerpo, para el código y el motivo). */
static char s_resp[UI_API_RESP_MAX];
static volatile uint32_t s_resp_len = 0u;
static volatile bool s_resp_done = false; /* el servidor cerró (FIN) */
static char s_motivo[UI_API_MOTIVO_MAX] = "";

/* Banderas que escriben los callbacks de lwIP y lee el poll (bool: escritura atómica). */
static volatile bool s_cb_connected = false;
static volatile bool s_cb_error = false;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

/* ---- Callbacks de lwIP (corren en el contexto de lwIP: no usan begin/end) ---- */

static err_t api_connected_cb(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)arg;
    (void)pcb;
    if (err != ERR_OK) {
        s_cb_error = true;
    } else {
        s_cb_connected = true;
    }
    return ERR_OK;
}

static void api_err_cb(void *arg, err_t err)
{
    (void)arg;
    (void)err;
    s_pcb = NULL; /* lwIP ya liberó el pcb: no volver a cerrarlo */
    s_cb_error = true;
}

static err_t api_sent_cb(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    (void)arg;
    (void)pcb;
    (void)len;
    return ERR_OK; /* el poll empuja más datos según tcp_sndbuf */
}

static err_t api_recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    (void)arg;
    if (p == NULL) {
        s_resp_done = true; /* el servidor cerró la conexión */
        return ERR_OK;
    }
    if (err == ERR_OK) {
        if (s_resp_len < (UI_API_RESP_MAX - 1u)) {
            const uint32_t room = (UI_API_RESP_MAX - 1u) - s_resp_len;
            const uint16_t take = (p->tot_len < room) ? p->tot_len : (uint16_t)room;
            const uint16_t got = pbuf_copy_partial(p, s_resp + s_resp_len, take, 0);
            s_resp_len += got;
            s_resp[s_resp_len] = '\0';
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

/* ---- Utilidades ---- */

static void close_pcb(void)
{
    if (s_pcb == NULL) {
        return;
    }
    cyw43_arch_lwip_begin();
    tcp_arg(s_pcb, NULL);
    tcp_err(s_pcb, NULL);
    tcp_recv(s_pcb, NULL);
    tcp_sent(s_pcb, NULL);
    if (tcp_close(s_pcb) != ERR_OK) {
        tcp_abort(s_pcb); /* si no se pudo cerrar limpio, se aborta */
    }
    cyw43_arch_lwip_end();
    s_pcb = NULL;
}

/* Código HTTP de la línea de estado ("HTTP/1.1 201 ..."); 0 si no se pudo leer. */
static int parse_http_code(void)
{
    const char *sp = memchr(s_resp, ' ', s_resp_len);
    if (sp == NULL) {
        return 0;
    }
    const char *d = sp + 1;
    if ((d + 3) > (s_resp + s_resp_len)) {
        return 0;
    }
    for (int i = 0; i < 3; i++) {
        if ((d[i] < '0') || (d[i] > '9')) {
            return 0;
        }
    }
    return ((d[0] - '0') * 100) + ((d[1] - '0') * 10) + (d[2] - '0');
}

/* Copia el motivo del error de la API ({"error":"..."}) en s_motivo; si no está, un texto fijo. */
static void parse_motivo(int code)
{
    const char *key = "\"error\":\"";
    const char *p = strstr(s_resp, key);
    if (p != NULL) {
        p += strlen(key);
        size_t i = 0u;
        while ((p[i] != '\0') && (p[i] != '"') && (i < (UI_API_MOTIVO_MAX - 1u))) {
            s_motivo[i] = p[i];
            i++;
        }
        s_motivo[i] = '\0';
        if (i > 0u) {
            return;
        }
    }
    (void)snprintf(s_motivo, sizeof(s_motivo), "la API rechazo la sesion (%d)", code);
}

/* Cierra el intento con éxito (200/201). */
static void finish_sent(int code)
{
    close_pcb();
    s_state = HAL_API_SENT;
    s_body = NULL;
    printf("API | POST " UI_API_PATH_SESION " -> %d (enviado)\n", code);
}

/* Cierra todo con error definitivo. Usa el s_motivo ya puesto (no lo copia: evita el aliasing
 * cuando el motivo es el propio s_motivo, p. ej. en el caso 4xx). */
static void finalize_error(int code)
{
    close_pcb();
    s_state = HAL_API_ERROR;
    s_body = NULL;
    if (code > 0) {
        printf("API | POST " UI_API_PATH_SESION " -> %d (error: %s)\n", code, s_motivo);
    } else {
        printf("API | POST " UI_API_PATH_SESION " -> sin respuesta (error: %s)\n", s_motivo);
    }
}

/* Error definitivo con un motivo literal (distinto de s_motivo). */
static void set_error(int code, const char *motivo)
{
    (void)snprintf(s_motivo, sizeof(s_motivo), "%s", motivo);
    finalize_error(code);
}

/* Falla recuperable: si quedan reintentos, programa el próximo; si no, error definitivo. */
static void fail_recoverable(int code, const char *motivo)
{
    close_pcb();
    if (code > 0) {
        printf("API | POST " UI_API_PATH_SESION " -> %d (reintento %lu/%lu)\n", code,
               (unsigned long)s_attempt, (unsigned long)UI_API_RETRY_MAX);
    } else {
        printf("API | POST " UI_API_PATH_SESION " -> %s (reintento %lu/%lu)\n", motivo,
               (unsigned long)s_attempt, (unsigned long)UI_API_RETRY_MAX);
    }
    if (s_attempt >= UI_API_RETRY_MAX) {
        set_error(0, "sin conexion con el servidor");
        return;
    }
    s_phase = PH_IDLE;
    s_state = HAL_API_RETRYING;
    s_retry_at_ms = now_ms() + UI_API_RETRY_INTERVAL_MS;
}

/* Clasifica el código HTTP y cierra el intento según corresponda. */
static void classify_response(void)
{
    const int code = parse_http_code();
    if ((code == 200) || (code == 201)) {
        finish_sent(code);
    } else if ((code >= 400) && (code < 500)) {
        parse_motivo(code); // deja el motivo en s_motivo
        finalize_error(code);
    } else {
        fail_recoverable(code, "sin respuesta valida"); /* 5xx, código raro o vacío */
    }
}

static void build_header(void)
{
    const int n = snprintf(s_header, sizeof(s_header),
                           "POST " UI_API_PATH_SESION " HTTP/1.1\r\n"
                           "Host: %s:%d\r\n"
                           "Content-Type: application/json\r\n"
                           "X-Redux-Key: %s\r\n"
                           "Content-Length: %lu\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           REDUX_API_HOST, (int)REDUX_API_PORT, REDUX_API_KEY,
                           (unsigned long)s_body_len);
    s_header_len = (n > 0 && (size_t)n < sizeof(s_header)) ? (uint32_t)n : 0u;
    s_header_off = 0u;
}

/* Empuja encabezados y cuerpo según el espacio disponible en el buffer de envío (tcp_sndbuf).
 * Encabezados con copia (se rearman por intento); cuerpo sin copia (persiste y no cambia). */
static void try_write(void)
{
    cyw43_arch_lwip_begin();
    while (s_header_off < s_header_len) {
        const u16_t sb = tcp_sndbuf(s_pcb);
        if (sb == 0u) {
            break;
        }
        const uint32_t rem = s_header_len - s_header_off;
        const u16_t chunk = (rem < sb) ? (u16_t)rem : sb;
        if (tcp_write(s_pcb, s_header + s_header_off, chunk,
                      TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE) != ERR_OK) {
            break;
        }
        s_header_off += chunk;
    }
    while ((s_header_off >= s_header_len) && (s_body_off < s_body_len)) {
        const u16_t sb = tcp_sndbuf(s_pcb);
        if (sb == 0u) {
            break;
        }
        const uint32_t rem = s_body_len - s_body_off;
        const u16_t chunk = (rem < sb) ? (u16_t)rem : sb;
        const bool last = ((s_body_off + chunk) >= s_body_len);
        const u8_t flags = last ? 0u : (u8_t)TCP_WRITE_FLAG_MORE; /* sin copia: el buffer persiste */
        if (tcp_write(s_pcb, s_body + s_body_off, chunk, flags) != ERR_OK) {
            break; /* ERR_MEM: se sigue en el próximo poll cuando se libere (sent_cb) */
        }
        s_body_off += chunk;
    }
    (void)tcp_output(s_pcb);
    cyw43_arch_lwip_end();
}

/* Arranca un intento: valida Wi-Fi/config, crea el pcb y lanza tcp_connect. */
static void start_attempt(void)
{
    s_attempt++;
    s_cb_connected = false;
    s_cb_error = false;
    s_resp_len = 0u;
    s_resp_done = false;
    s_resp[0] = '\0';
    s_header_off = 0u;
    s_body_off = 0u;

    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {
        fail_recoverable(0, "sin conexion Wi-Fi");
        return;
    }
    ip_addr_t ip;
    if (!ip4addr_aton(REDUX_API_HOST, &ip)) {
        set_error(0, "api_config: host invalido");
        return;
    }

    cyw43_arch_lwip_begin();
    s_pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    err_t err = ERR_MEM;
    if (s_pcb != NULL) {
        tcp_arg(s_pcb, NULL);
        tcp_err(s_pcb, api_err_cb);
        tcp_recv(s_pcb, api_recv_cb);
        tcp_sent(s_pcb, api_sent_cb);
        err = tcp_connect(s_pcb, &ip, (u16_t)REDUX_API_PORT, api_connected_cb);
    }
    cyw43_arch_lwip_end();

    if ((s_pcb == NULL) || (err != ERR_OK)) {
        fail_recoverable(0, "no se pudo abrir la conexion");
        return;
    }
    build_header();
    s_phase = PH_CONNECTING;
    s_state = HAL_API_SENDING;
    s_phase_start_ms = now_ms();
    printf("API | POST " UI_API_PATH_SESION " paciente %ld, %lu bytes (intento %lu/%lu)\n",
           (long)s_paciente, (unsigned long)s_body_len, (unsigned long)s_attempt,
           (unsigned long)UI_API_RETRY_MAX);
}

/* ---- API pública ---- */

bool hal_api_init(void)
{
    s_state = HAL_API_IDLE;
    s_phase = PH_IDLE;
    s_pcb = NULL;
    s_body = NULL;
    s_want_start = false;
    s_motivo[0] = '\0';
    s_config_ok = (REDUX_API_HOST[0] != '\0');
    if (!s_config_ok) {
        printf("API | sin api_config.h: no se enviaran sesiones por Wi-Fi (respaldo por USB)\n");
    }
    return s_config_ok;
}

void hal_api_send_session(const char *body, uint32_t len, int32_t paciente)
{
    if (hal_api_busy()) {
        return; /* ya hay un envío en curso: no se pisa */
    }
    if (!s_config_ok || (body == NULL) || (len == 0u)) {
        return; /* sin config o cuerpo vacío: queda el respaldo por USB */
    }
    s_body = body;
    s_body_len = len;
    s_paciente = paciente;
    s_attempt = 0u;
    s_motivo[0] = '\0';
    s_phase = PH_IDLE;
    s_state = HAL_API_SENDING;
    s_want_start = true;
}

void hal_api_poll(void)
{
    if (s_want_start) {
        s_want_start = false;
        start_attempt();
        return;
    }
    if (s_state == HAL_API_RETRYING) {
        if ((int32_t)(now_ms() - s_retry_at_ms) >= 0) {
            start_attempt();
        }
        return;
    }
    if (s_state != HAL_API_SENDING) {
        return; /* IDLE / SENT / ERROR: nada que atender */
    }

    /* La respuesta completa (el servidor cerró con Connection: close) tiene prioridad sobre
     * un posible corte posterior: ya tenemos el código y el motivo del cuerpo. */
    if ((s_phase == PH_WAITING) && s_resp_done) {
        classify_response();
        return;
    }
    if (s_cb_error) {
        fail_recoverable(0, "conexion cortada");
        return;
    }

    switch (s_phase) {
    case PH_CONNECTING:
        if (s_cb_connected) {
            s_phase = PH_WRITING;
            s_phase_start_ms = now_ms();
        } else if ((now_ms() - s_phase_start_ms) >= UI_API_CONNECT_TIMEOUT_MS) {
            fail_recoverable(0, "timeout de conexion");
        }
        break;
    case PH_WRITING:
        try_write();
        if ((s_header_off >= s_header_len) && (s_body_off >= s_body_len)) {
            s_phase = PH_WAITING;
            s_phase_start_ms = now_ms();
        } else if ((now_ms() - s_phase_start_ms) >= UI_API_RESPONSE_TIMEOUT_MS) {
            fail_recoverable(0, "timeout al enviar");
        }
        break;
    case PH_WAITING:
        /* El cierre del servidor (s_resp_done) se atiende arriba. Acá solo el timeout: si
         * llegó algo con código válido, se clasifica; si no, se trata como sin respuesta. */
        if ((now_ms() - s_phase_start_ms) >= UI_API_RESPONSE_TIMEOUT_MS) {
            if ((s_resp_len > 0u) && (parse_http_code() > 0)) {
                classify_response();
            } else {
                fail_recoverable(0, "sin respuesta");
            }
        }
        break;
    case PH_IDLE:
    default:
        break;
    }
}

hal_api_state_t hal_api_get_state(void) { return s_state; }

const char *hal_api_error_motivo(void) { return s_motivo; }

bool hal_api_busy(void)
{
    return (s_state == HAL_API_SENDING) || (s_state == HAL_API_RETRYING) || s_want_start;
}
