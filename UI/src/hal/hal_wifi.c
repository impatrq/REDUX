/**
 * @file hal_wifi.c
 * @brief Wi-Fi del CYW43: escaneo y conexión a redes abiertas, atendido desde el core 0.
 *
 * Usa pico_cyw43_arch_lwip_threadsafe_background: el driver trabaja en una interrupción
 * del core que llamó a cyw43_arch_init() (el 0). El callback del escaneo corre ahí.
 */
#include "hal/hal_wifi.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "pico/util/queue.h"

#include "ui/ui_config.h"

#define HAL_WIFI_CB_CONTINUE  0  /* El callback del escaneo devuelve 0 para seguir */
#define HAL_WIFI_SCAN_AUTH_OPEN  0u  /* auth_mode del escaneo: 0 = sin cifrado */

typedef enum {
    HAL_WIFI_REQ_SCAN = 0,
    HAL_WIFI_REQ_CONNECT
} hal_wifi_req_type_t;

typedef struct {
    hal_wifi_req_type_t type;
    char ssid[HAL_WIFI_SSID_MAX_LEN + 1u];
} hal_wifi_request_t;

static queue_t s_request_queue;  /* core 1 -> core 0 */
static queue_t s_report_queue;   /* core 0 -> core 1 */

/* Solo core 0 (y el callback del escaneo, en su interrupción). */
static hal_wifi_report_t s_report;
static hal_wifi_report_t s_dropped;  /* Destino del reporte descartado (no entra en la pila) */
static hal_wifi_net_t s_scan_nets[UI_WIFI_MAX_NETS];
static volatile uint32_t s_scan_count = 0u;
static uint32_t s_busy_start_ms = 0u;  /* Inicio del escaneo o de la conexión en curso */
static char s_connecting_ssid[HAL_WIFI_SSID_MAX_LEN + 1u];

static uint32_t hal_wifi_now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

/** Copia con corte y terminación garantizados. */
static void hal_wifi_copy_ssid(char *dst, const char *src)
{
    (void)snprintf(dst, HAL_WIFI_SSID_MAX_LEN + 1u, "%s", src);
}

/** true si a va antes que b en la lista: abiertas primero, después la de mejor señal. */
static bool hal_wifi_is_better(const hal_wifi_net_t *a, const hal_wifi_net_t *b)
{
    if (a->open != b->open) {
        return a->open;
    }
    return a->rssi_dbm > b->rssi_dbm;
}

/** Publica el estado actual; con la cola llena descarta el reporte más viejo. */
static void hal_wifi_publish(hal_wifi_state_t state)
{
    s_report.state = state;
    if (!queue_try_add(&s_report_queue, &s_report)) {
        (void)queue_try_remove(&s_report_queue, &s_dropped);
        (void)queue_try_add(&s_report_queue, &s_report);
    }
}

/**
 * Cada red que encuentra el escaneo. Ignora las ocultas y junta las de igual SSID
 * (queda la mejor). Con la lista llena reemplaza a la peor si la nueva es mejor.
 */
static int hal_wifi_scan_cb(void *env, const cyw43_ev_scan_result_t *result)
{
    (void)env;
    if ((result == NULL) || (result->ssid_len == 0u) || (result->ssid_len > HAL_WIFI_SSID_MAX_LEN)) {
        return HAL_WIFI_CB_CONTINUE;
    }

    hal_wifi_net_t net;
    memcpy(net.ssid, result->ssid, result->ssid_len);
    net.ssid[result->ssid_len] = '\0';
    net.rssi_dbm = result->rssi;
    net.open = (result->auth_mode == HAL_WIFI_SCAN_AUTH_OPEN);

    const uint32_t count = s_scan_count;
    for (uint32_t i = 0u; i < count; i++) {
        if (strcmp(s_scan_nets[i].ssid, net.ssid) == 0) {
            if (hal_wifi_is_better(&net, &s_scan_nets[i])) {
                s_scan_nets[i] = net;
            }
            return HAL_WIFI_CB_CONTINUE;
        }
    }

    if (count < UI_WIFI_MAX_NETS) {
        s_scan_nets[count] = net;
        s_scan_count = count + 1u;
        return HAL_WIFI_CB_CONTINUE;
    }

    uint32_t worst = 0u;
    for (uint32_t i = 1u; i < count; i++) {
        if (hal_wifi_is_better(&s_scan_nets[worst], &s_scan_nets[i])) {
            worst = i;
        }
    }
    if (hal_wifi_is_better(&net, &s_scan_nets[worst])) {
        s_scan_nets[worst] = net;
    }
    return HAL_WIFI_CB_CONTINUE;
}

/** Pasa el resultado del escaneo al reporte, ordenado (inserción: son pocas redes). */
static void hal_wifi_finish_scan(void)
{
    const uint32_t count = s_scan_count;
    for (uint32_t i = 0u; i < count; i++) {
        const hal_wifi_net_t net = s_scan_nets[i];
        uint32_t j = i;
        while ((j > 0u) && hal_wifi_is_better(&net, &s_report.nets[j - 1u])) {
            s_report.nets[j] = s_report.nets[j - 1u];
            j--;
        }
        s_report.nets[j] = net;
    }
    s_report.net_count = count;
    printf("WIFI | escaneo: %lu redes\n", (unsigned long)count);
    hal_wifi_publish(HAL_WIFI_STATE_SCAN_DONE);
}

static void hal_wifi_start_scan(void)
{
    cyw43_wifi_scan_options_t options;
    memset(&options, 0, sizeof(options));
    s_scan_count = 0u;
    if (cyw43_wifi_scan(&cyw43_state, &options, NULL, hal_wifi_scan_cb) != 0) {
        printf("WIFI | no se pudo empezar el escaneo\n");
        hal_wifi_publish(HAL_WIFI_STATE_SCAN_FAILED);
        return;
    }
    s_busy_start_ms = hal_wifi_now_ms();
    hal_wifi_publish(HAL_WIFI_STATE_SCANNING);
}

static void hal_wifi_start_connect(const char *ssid)
{
    if (s_report.connected_ssid[0] != '\0') {
        (void)cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        s_report.connected_ssid[0] = '\0';
    }
    hal_wifi_copy_ssid(s_connecting_ssid, ssid);
    if (cyw43_arch_wifi_connect_async(s_connecting_ssid, NULL, CYW43_AUTH_OPEN) != 0) {
        printf("WIFI | no se pudo empezar la conexion a %s\n", s_connecting_ssid);
        hal_wifi_publish(HAL_WIFI_STATE_CONNECT_FAILED);
        return;
    }
    s_busy_start_ms = hal_wifi_now_ms();
    hal_wifi_publish(HAL_WIFI_STATE_CONNECTING);
}

static void hal_wifi_poll_scan(void)
{
    if (!cyw43_wifi_scan_active(&cyw43_state)) {
        hal_wifi_finish_scan();
    } else if ((hal_wifi_now_ms() - s_busy_start_ms) >= UI_WIFI_SCAN_TIMEOUT_MS) {
        printf("WIFI | escaneo sin terminar tras %lu ms\n", (unsigned long)UI_WIFI_SCAN_TIMEOUT_MS);
        hal_wifi_publish(HAL_WIFI_STATE_SCAN_FAILED);
    } else {
        /* Sigue escaneando. */
    }
}

static void hal_wifi_poll_connect(void)
{
    const int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link == CYW43_LINK_UP) {
        hal_wifi_copy_ssid(s_report.connected_ssid, s_connecting_ssid);
        printf("WIFI | conectado a %s\n", s_report.connected_ssid);
        hal_wifi_publish(HAL_WIFI_STATE_CONNECTED);
    } else if (link < 0) {
        printf("WIFI | fallo la conexion a %s (%d)\n", s_connecting_ssid, link);
        hal_wifi_publish(HAL_WIFI_STATE_CONNECT_FAILED);
    } else if ((hal_wifi_now_ms() - s_busy_start_ms) >= UI_WIFI_CONNECT_TIMEOUT_MS) {
        (void)cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        printf("WIFI | conexion a %s sin respuesta tras %lu ms\n", s_connecting_ssid,
               (unsigned long)UI_WIFI_CONNECT_TIMEOUT_MS);
        hal_wifi_publish(HAL_WIFI_STATE_CONNECT_FAILED);
    } else {
        /* Sigue conectando (asociación o DHCP). */
    }
}

/** Si se cae la conexión, se avisa y la red deja de figurar como conectada. */
static void hal_wifi_check_link(void)
{
    if ((s_report.connected_ssid[0] != '\0') &&
        (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP)) {
        printf("WIFI | se perdio la conexion a %s\n", s_report.connected_ssid);
        s_report.connected_ssid[0] = '\0';
        hal_wifi_publish(HAL_WIFI_STATE_IDLE);
    }
}

bool hal_wifi_init(void)
{
    queue_init(&s_request_queue, sizeof(hal_wifi_request_t), UI_WIFI_REQUEST_QUEUE_LEN);
    queue_init(&s_report_queue, sizeof(hal_wifi_report_t), UI_WIFI_REPORT_QUEUE_LEN);
    memset(&s_report, 0, sizeof(s_report));
    s_report.state = HAL_WIFI_STATE_OFF;

    if (cyw43_arch_init() != 0) {
        printf("WIFI | no arranco el CYW43\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();
    s_report.state = HAL_WIFI_STATE_IDLE;
    return true;
}

void hal_wifi_poll(void)
{
    switch (s_report.state) {
    case HAL_WIFI_STATE_SCANNING:   hal_wifi_poll_scan();    return;
    case HAL_WIFI_STATE_CONNECTING: hal_wifi_poll_connect(); return;
    default:                        break;
    }

    hal_wifi_request_t request;
    if (s_report.state == HAL_WIFI_STATE_OFF) {
        if (queue_try_remove(&s_request_queue, &request)) {
            hal_wifi_publish(HAL_WIFI_STATE_OFF);  /* Que la UI sepa que no hay Wi-Fi */
        }
        return;
    }

    hal_wifi_check_link();
    if (!queue_try_remove(&s_request_queue, &request)) {
        return;
    }
    if (request.type == HAL_WIFI_REQ_SCAN) {
        hal_wifi_start_scan();
    } else {
        hal_wifi_start_connect(request.ssid);
    }
}

bool hal_wifi_request_scan(void)
{
    hal_wifi_request_t request = { .type = HAL_WIFI_REQ_SCAN, .ssid = "" };
    return queue_try_add(&s_request_queue, &request);
}

bool hal_wifi_request_connect(const char *ssid)
{
    hal_wifi_request_t request = { .type = HAL_WIFI_REQ_CONNECT, .ssid = "" };
    hal_wifi_copy_ssid(request.ssid, ssid);
    return queue_try_add(&s_request_queue, &request);
}

bool hal_wifi_get_report(hal_wifi_report_t *report)
{
    return queue_try_remove(&s_report_queue, report);
}
