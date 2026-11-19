/**
 * @file hal_wifi.c
 * @brief Wi-Fi del CYW43 atendido desde el core 1 (el mismo que LVGL).
 *
 * Usa pico_cyw43_arch_lwip_threadsafe_background: el driver trabaja en una interrupción
 * del core que llamó a cyw43_arch_init(). Acá ese core es el 1, así que el callback del
 * escaneo y el trabajo de fondo caen en el core 1 y no molestan al core 0 (motor).
 * Como todo corre en un solo núcleo no hay colas: la UI pide y lee en el mismo core.
 */
#include "hal/hal_wifi.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "ui/ui_config.h"

/* Redes guardadas: wifi_redes.h está en .gitignore (no se sube). Si falta, se compila
 * con la lista vacía y un aviso para copiar la plantilla. */
#if defined(__has_include)
#  if __has_include("ui/wifi_redes.h")
#    include "ui/wifi_redes.h"
#    define HAL_WIFI_REDES_PRESENT 1
#  endif
#endif

#ifndef HAL_WIFI_REDES_PRESENT
#  pragma GCC diagnostic push
#  pragma GCC diagnostic warning "-Wcpp"
#  warning "sin redes guardadas: copia src/ui/wifi_redes.example.h a src/ui/wifi_redes.h"
#  pragma GCC diagnostic pop
static const hal_wifi_saved_t WIFI_REDES[] = { { NULL, NULL } };
#endif

#define HAL_WIFI_REDES_COUNT     (sizeof(WIFI_REDES) / sizeof(WIFI_REDES[0]))
#define HAL_WIFI_CB_CONTINUE     0    /* El callback del escaneo devuelve 0 para seguir */
#define HAL_WIFI_SCAN_AUTH_OPEN  0u   /* auth_mode del escaneo: 0 = sin cifrado */

typedef enum {
    HAL_WIFI_REQ_NONE = 0,
    HAL_WIFI_REQ_SCAN,
    HAL_WIFI_REQ_CONNECT
} hal_wifi_req_t;

/* Estado publicado (lo leen los getters desde el mismo core). */
static hal_wifi_state_t s_state = HAL_WIFI_STATE_OFF;
static hal_wifi_net_t s_nets[UI_WIFI_MAX_NETS];     /* Último escaneo, ordenado */
static uint32_t s_net_count = 0u;
static uint32_t s_scan_gen = 0u;                    /* Sube con cada escaneo terminado */
static char s_connected_ssid[HAL_WIFI_SSID_MAX_LEN + 1u] = "";

/* Escaneo en curso. */
static hal_wifi_net_t s_scan_tmp[UI_WIFI_MAX_NETS];
static volatile uint32_t s_scan_tmp_count = 0u;
static uint32_t s_busy_start_ms = 0u;               /* Inicio del escaneo o conexión en curso */
static bool s_scan_is_auto = false;                 /* El escaneo en curso es de autoconexión */
static char s_connecting_ssid[HAL_WIFI_SSID_MAX_LEN + 1u] = "";

/* Pedido manual de la UI (un solo slot: el último pisa al anterior). */
static hal_wifi_req_t s_req = HAL_WIFI_REQ_NONE;
static char s_req_ssid[HAL_WIFI_SSID_MAX_LEN + 1u] = "";

/* Autoconexión / reconexión. */
static char s_last_ssid[HAL_WIFI_SSID_MAX_LEN + 1u] = "";  /* Última red conectada */
static bool s_auto_first = true;                           /* Fuerza el primer intento al arrancar */
static uint32_t s_auto_last_ms = 0u;                       /* Último intento automático */

/* Antirrebote de la pérdida de enlace: una caída transitoria (renovación de DHCP) no cuenta. */
static bool s_link_down = false;
static uint32_t s_link_down_since_ms = 0u;

static uint32_t hal_wifi_now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

/** Copia con corte y terminación garantizados. */
static void hal_wifi_copy_ssid(char *dst, const char *src)
{
    (void)snprintf(dst, HAL_WIFI_SSID_MAX_LEN + 1u, "%s", src);
}

/** RSSI (dBm) del enlace actual; 0 si no se puede leer. */
static int32_t hal_wifi_rssi(void)
{
    int32_t rssi = 0;
    (void)cyw43_wifi_get_rssi(&cyw43_state, &rssi);
    return rssi;
}

/** Copia la IP de la estación ("0.0.0.0" si todavía no hay). lwIP se toca bajo su candado. */
static void hal_wifi_ip_str(char *dst, size_t len)
{
    cyw43_arch_lwip_begin();
    const ip4_addr_t *ip = netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA]);
    (void)snprintf(dst, len, "%s", ip4addr_ntoa(ip));
    cyw43_arch_lwip_end();
}

/** Clave de una red guardada, o NULL si @p ssid no está en wifi_redes.h. */
static const char *hal_wifi_saved_password(const char *ssid)
{
    for (size_t i = 0u; i < HAL_WIFI_REDES_COUNT; i++) {
        const char *s = WIFI_REDES[i].ssid;
        if ((s != NULL) && (s[0] != '\0') && (strcmp(s, ssid) == 0)) {
            return WIFI_REDES[i].password;
        }
    }
    return NULL;
}

/** Cuántas redes guardadas válidas hay. */
static uint32_t hal_wifi_saved_count(void)
{
    uint32_t count = 0u;
    for (size_t i = 0u; i < HAL_WIFI_REDES_COUNT; i++) {
        const char *s = WIFI_REDES[i].ssid;
        if ((s != NULL) && (s[0] != '\0')) {
            count++;
        }
    }
    return count;
}

/** true si @p ssid salió en el último escaneo. */
static bool hal_wifi_scan_has(const char *ssid)
{
    for (uint32_t i = 0u; i < s_net_count; i++) {
        if (strcmp(s_nets[i].ssid, ssid) == 0) {
            return true;
        }
    }
    return false;
}

/** Primera red guardada (en orden de la lista) que apareció en el escaneo, o NULL. */
static const char *hal_wifi_first_saved_present(void)
{
    for (size_t i = 0u; i < HAL_WIFI_REDES_COUNT; i++) {
        const char *s = WIFI_REDES[i].ssid;
        if ((s != NULL) && (s[0] != '\0') && hal_wifi_scan_has(s)) {
            return s;
        }
    }
    return NULL;
}

/** true si a va antes que b: primero las conectables (abiertas o guardadas), después por señal. */
static bool hal_wifi_is_better(const hal_wifi_net_t *a, const hal_wifi_net_t *b)
{
    const bool conn_a = a->open || a->saved;
    const bool conn_b = b->open || b->saved;
    if (conn_a != conn_b) {
        return conn_a;
    }
    return a->rssi_dbm > b->rssi_dbm;
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
    net.saved = (hal_wifi_saved_password(net.ssid) != NULL);

    const uint32_t count = s_scan_tmp_count;
    for (uint32_t i = 0u; i < count; i++) {
        if (strcmp(s_scan_tmp[i].ssid, net.ssid) == 0) {
            if (hal_wifi_is_better(&net, &s_scan_tmp[i])) {
                s_scan_tmp[i] = net;
            }
            return HAL_WIFI_CB_CONTINUE;
        }
    }

    if (count < UI_WIFI_MAX_NETS) {
        s_scan_tmp[count] = net;
        s_scan_tmp_count = count + 1u;
        return HAL_WIFI_CB_CONTINUE;
    }

    uint32_t worst = 0u;
    for (uint32_t i = 1u; i < count; i++) {
        if (hal_wifi_is_better(&s_scan_tmp[worst], &s_scan_tmp[i])) {
            worst = i;
        }
    }
    if (hal_wifi_is_better(&net, &s_scan_tmp[worst])) {
        s_scan_tmp[worst] = net;
    }
    return HAL_WIFI_CB_CONTINUE;
}

static void hal_wifi_start_connect(const char *ssid)
{
    const char *password = hal_wifi_saved_password(ssid);
    if (s_connected_ssid[0] != '\0') {
        (void)cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        s_connected_ssid[0] = '\0';
    }
    hal_wifi_copy_ssid(s_connecting_ssid, ssid);
    /* Con clave (guardada y no vacía): WPA2/WPA3 mixto. Sin clave (guardada con "", o no
     * guardada y abierta): red abierta, sin contraseña. */
    const bool use_wpa = (password != NULL) && (password[0] != '\0');
    const uint32_t auth = use_wpa ? (uint32_t)CYW43_AUTH_WPA2_MIXED_PSK : (uint32_t)CYW43_AUTH_OPEN;
    if (cyw43_arch_wifi_connect_async(s_connecting_ssid, use_wpa ? password : NULL, auth) != 0) {
        printf("WIFI | no se pudo empezar la conexion a %s\n", s_connecting_ssid);
        s_state = HAL_WIFI_STATE_CONNECT_FAILED;
        return;
    }
    s_busy_start_ms = hal_wifi_now_ms();
    s_state = HAL_WIFI_STATE_CONNECTING;
}

/** Pasa el resultado del escaneo a la lista publicada, ordenado (inserción: son pocas redes). */
static void hal_wifi_finish_scan(void)
{
    const uint32_t count = s_scan_tmp_count;
    for (uint32_t i = 0u; i < count; i++) {
        const hal_wifi_net_t net = s_scan_tmp[i];
        uint32_t j = i;
        while ((j > 0u) && hal_wifi_is_better(&net, &s_nets[j - 1u])) {
            s_nets[j] = s_nets[j - 1u];
            j--;
        }
        s_nets[j] = net;
    }
    s_net_count = count;
    s_scan_gen++;
    s_state = HAL_WIFI_STATE_SCAN_DONE;
    printf("WIFI | escaneo: %lu redes\n", (unsigned long)count);

    if (s_scan_is_auto) {
        const char *ssid = hal_wifi_first_saved_present();
        if (ssid != NULL) {
            hal_wifi_start_connect(ssid);  /* Arranque: conecta a la primera guardada presente */
        }
        /* Si no hay ninguna guardada a la vista, queda en SCAN_DONE y reintenta luego. */
    }
}

static void hal_wifi_start_scan(bool is_auto)
{
    cyw43_wifi_scan_options_t options;
    memset(&options, 0, sizeof(options));
    s_scan_tmp_count = 0u;
    s_scan_is_auto = is_auto;
    if (cyw43_wifi_scan(&cyw43_state, &options, NULL, hal_wifi_scan_cb) != 0) {
        printf("WIFI | no se pudo empezar el escaneo\n");
        s_state = HAL_WIFI_STATE_SCAN_FAILED;
        return;
    }
    s_busy_start_ms = hal_wifi_now_ms();
    s_state = HAL_WIFI_STATE_SCANNING;
}

static void hal_wifi_poll_scan(void)
{
    if (!cyw43_wifi_scan_active(&cyw43_state)) {
        hal_wifi_finish_scan();
    } else if ((hal_wifi_now_ms() - s_busy_start_ms) >= UI_WIFI_SCAN_TIMEOUT_MS) {
        printf("WIFI | escaneo sin terminar tras %lu ms\n", (unsigned long)UI_WIFI_SCAN_TIMEOUT_MS);
        s_state = HAL_WIFI_STATE_SCAN_FAILED;
    } else {
        /* Sigue escaneando. */
    }
}

static void hal_wifi_poll_connect(void)
{
    const int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link == CYW43_LINK_UP) {
        hal_wifi_copy_ssid(s_connected_ssid, s_connecting_ssid);
        hal_wifi_copy_ssid(s_last_ssid, s_connecting_ssid);  /* La reconexión vuelve a esta */
        s_link_down = false;                                 /* Enlace sano: reinicia el antirrebote */
        char ip_str[IP4ADDR_STRLEN_MAX];
        hal_wifi_ip_str(ip_str, sizeof(ip_str));
        printf("WIFI | conectado a %s (%ld dBm, IP %s)\n", s_connected_ssid,
               (long)hal_wifi_rssi(), ip_str);
        s_state = HAL_WIFI_STATE_CONNECTED;
    } else if (link < 0) {
        printf("WIFI | fallo la conexion a %s (link %d, %ld dBm)\n", s_connecting_ssid, link,
               (long)hal_wifi_rssi());
        s_state = HAL_WIFI_STATE_CONNECT_FAILED;
    } else if ((hal_wifi_now_ms() - s_busy_start_ms) >= UI_WIFI_CONNECT_TIMEOUT_MS) {
        (void)cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
        printf("WIFI | conexion a %s sin respuesta tras %lu ms (link %d, %ld dBm)\n",
               s_connecting_ssid, (unsigned long)UI_WIFI_CONNECT_TIMEOUT_MS, link,
               (long)hal_wifi_rssi());
        s_state = HAL_WIFI_STATE_CONNECT_FAILED;
    } else {
        /* Sigue conectando (asociación o DHCP). */
    }
}

/**
 * Vigila el enlace. Una caída se da por perdida solo si sigue caída UI_WIFI_LINK_LOST_MS
 * seguidos: los estados intermedios (JOIN/NOIP) de una renovación de DHCP no cuentan.
 */
static void hal_wifi_check_link(void)
{
    if (s_connected_ssid[0] == '\0') {
        return;
    }
    const int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link == CYW43_LINK_UP) {
        s_link_down = false;  /* Enlace sano */
        return;
    }

    const uint32_t now_ms = hal_wifi_now_ms();
    if (!s_link_down) {
        s_link_down = true;
        s_link_down_since_ms = now_ms;  /* Empieza a contar el antirrebote */
        return;
    }
    if ((now_ms - s_link_down_since_ms) < UI_WIFI_LINK_LOST_MS) {
        return;  /* Caída transitoria: se le da margen a que el enlace vuelva */
    }

    printf("WIFI | se perdio la conexion a %s (link %d, %ld dBm)\n", s_connected_ssid, link,
           (long)hal_wifi_rssi());
    s_connected_ssid[0] = '\0';
    s_state = HAL_WIFI_STATE_IDLE;
    s_link_down = false;
    s_auto_last_ms = now_ms;  /* Reintenta recién tras UI_WIFI_RECONNECT_MS */
}

bool hal_wifi_init(void)
{
    if (cyw43_arch_init() != 0) {
        printf("WIFI | no arranco el CYW43\n");
        s_state = HAL_WIFI_STATE_OFF;
        return false;
    }
    cyw43_arch_enable_sta_mode();
    /* Sin ahorro de energía: el ahorro por defecto (agresivo) duerme la radio entre balizas y
     * provoca asociaciones lentas y caídas. El consumo del chip es despreciable frente al motor,
     * así que se prioriza un enlace estable y de baja latencia. */
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
    s_state = HAL_WIFI_STATE_IDLE;
    s_auto_first = true;
    s_auto_last_ms = hal_wifi_now_ms();
    return true;
}

void hal_wifi_poll(void)
{
    if (s_state == HAL_WIFI_STATE_OFF) {
        return;  /* Sin chip no hay nada que atender */
    }
    switch (s_state) {
    case HAL_WIFI_STATE_SCANNING:   hal_wifi_poll_scan();    return;
    case HAL_WIFI_STATE_CONNECTING: hal_wifi_poll_connect(); return;
    default:                        break;
    }

    hal_wifi_check_link();

    /* 1) Pedido manual de la UI: tiene prioridad sobre la autoconexión. */
    if (s_req == HAL_WIFI_REQ_SCAN) {
        s_req = HAL_WIFI_REQ_NONE;
        hal_wifi_start_scan(false);
        return;
    }
    if (s_req == HAL_WIFI_REQ_CONNECT) {
        s_req = HAL_WIFI_REQ_NONE;
        hal_wifi_start_connect(s_req_ssid);
        return;
    }

    /* 2) Autoconexión / reconexión: solo sin conexión y con el Wi-Fi libre (no escaneando
     *    ni conectando, porque esos estados salen arriba). */
    if (s_connected_ssid[0] != '\0') {
        return;
    }
    const uint32_t now_ms = hal_wifi_now_ms();
    if (!s_auto_first && ((now_ms - s_auto_last_ms) < UI_WIFI_RECONNECT_MS)) {
        return;
    }
    if (s_last_ssid[0] != '\0') {
        /* Reconexión: directo a la última red (guardada o elegida a mano). */
        s_auto_first = false;
        s_auto_last_ms = now_ms;
        hal_wifi_start_connect(s_last_ssid);
    } else if (hal_wifi_saved_count() > 0u) {
        /* Arranque, todavía sin conexión previa: escanea y elige la primera guardada. */
        s_auto_first = false;
        s_auto_last_ms = now_ms;
        hal_wifi_start_scan(true);
    } else {
        /* Sin redes guardadas y sin red previa: nada que autoconectar. */
    }
}

void hal_wifi_request_scan(void)
{
    s_req = HAL_WIFI_REQ_SCAN;
}

void hal_wifi_request_connect(const char *ssid)
{
    s_req = HAL_WIFI_REQ_CONNECT;
    hal_wifi_copy_ssid(s_req_ssid, ssid);
}

hal_wifi_state_t hal_wifi_get_state(void)
{
    return s_state;
}

const char *hal_wifi_connected_ssid(void)
{
    return s_connected_ssid;
}

uint32_t hal_wifi_scan_generation(void)
{
    return s_scan_gen;
}

uint32_t hal_wifi_get_nets(hal_wifi_net_t *out, uint32_t max)
{
    const uint32_t count = (s_net_count < max) ? s_net_count : max;
    for (uint32_t i = 0u; i < count; i++) {
        out[i] = s_nets[i];
    }
    return count;
}
