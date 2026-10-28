// ui_basic.c · Interfaz de usuario básica: pulsador GP3 (CONFIRMAR), LED GP2 que
// parpadea lento mientras se espera una confirmación, y mensajes/eventos por USB.
// Es una implementación del contrato ui.h; traduce pulsador y comandos serie a
// eventos ui_event_t y los deja en una cola que ui_poll() va vaciando.

#include "ui_basic.h"

#include <stdio.h>
#include <string.h>

#include "config.h" // LED_BLINK_TICKS
#include "hal_button.h"
#include "hal_gpio.h"
#include "hal_time.h"

#define UI_EVENT_QUEUE_LEN 8u // eventos encolados antes de descartar

static ui_event_t s_queue[UI_EVENT_QUEUE_LEN];
static uint32_t s_head; // índice de lectura
static uint32_t s_tail; // índice de escritura
static bool s_waiting;  // true mientras se espera una confirmación (parpadeo del LED)

// Encola un evento; si la cola está llena, lo descarta (nunca bloquea ni pisa).
static void queue_push(ui_event_t ev) {
    const uint32_t next = (s_tail + 1u) % UI_EVENT_QUEUE_LEN;
    if (next != s_head) {
        s_queue[s_tail] = ev;
        s_tail = next;
    }
}

void ui_init(void) {
    s_head = 0u;
    s_tail = 0u;
    s_waiting = false;
    hal_button_init();
    hal_gpio_led_init();
    hal_gpio_led_set(false);
}

void ui_tick(void) {
    // El pulsador es la confirmación física. hal_button_poll espera cadencia de 1 ms.
    if (hal_button_poll()) {
        queue_push(UI_EV_CONFIRM);
    }
    // Parpadeo lento solo mientras se espera una confirmación; si no, LED apagado.
    if (s_waiting) {
        const bool on = ((hal_time_ticks() / LED_BLINK_TICKS) & 1u) != 0u;
        hal_gpio_led_set(on);
    } else {
        hal_gpio_led_set(false);
    }
}

ui_event_t ui_poll(void) {
    if (s_head == s_tail) {
        return UI_EV_NONE;
    }
    const ui_event_t ev = s_queue[s_head];
    s_head = (s_head + 1u) % UI_EVENT_QUEUE_LEN;
    return ev;
}

void ui_message(const char *msg) {
    printf("%s\n", msg);
}

void ui_set_waiting(bool waiting) {
    s_waiting = waiting;
    if (!waiting) {
        hal_gpio_led_set(false);
    }
}

void ui_basic_feed_line(const char *line) {
    if (strcmp(line, "ok") == 0) {
        queue_push(UI_EV_CONFIRM);
    } else if (strcmp(line, "cancel") == 0) {
        queue_push(UI_EV_CANCEL);
    } else if (strcmp(line, "recal") == 0) {
        queue_push(UI_EV_RECALIBRATE);
    }
}
