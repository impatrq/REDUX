/**
 * @file test_keypad.c
 * @brief Pruebas en PC de hal_keypad_logic: antirrebote, varias teclas a la vez,
 *        teclas fantasma y cola de eventos.
 *
 * Compilar y correr: ver tests/CMakeLists.txt.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hal/hal_keypad_logic.h"

/* Teclas por posición (fila × 4 + columna), con la disposición 123A/456B/789C/ *0#D */
#define KEY_1    0u
#define KEY_2    1u
#define KEY_4    4u
#define KEY_5    5u
#define KEY_D    15u
#define BIT(key) ((uint16_t)(1u << (key)))
#define NO_KEYS  ((uint16_t)0u)

static int s_failures = 0;
static int s_checks = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        s_checks++;                                                         \
        if (!(cond)) {                                                      \
            s_failures++;                                                   \
            printf("  FALLA %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                   \
    } while (0)

/** Pasa la misma lectura cruda @p scans veces. */
static void feed_n(hal_keypad_logic_t *k, uint16_t raw, uint32_t scans)
{
    for (uint32_t i = 0u; i < scans; i++) {
        hal_keypad_logic_feed(k, raw);
    }
}

/** true si el próximo evento es (@p key, @p type). */
static bool next_event_is(hal_keypad_logic_t *k, uint8_t key, hal_keypad_ev_type_t type)
{
    hal_keypad_event_t ev;
    return hal_keypad_logic_pop(k, &ev) && (ev.key == key) && (ev.type == type);
}

static bool queue_empty(hal_keypad_logic_t *k)
{
    hal_keypad_event_t ev;
    return !hal_keypad_logic_pop(k, &ev);
}

static void test_idle_no_events(void)
{
    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);
    feed_n(&k, NO_KEYS, 100u);
    CHECK(queue_empty(&k));
    CHECK(k.stable == NO_KEYS);
}

static void test_single_press_release(void)
{
    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);

    feed_n(&k, BIT(KEY_5), UI_KEYPAD_DEBOUNCE_SCANS - 1u);
    CHECK(queue_empty(&k)); /* Todavía no alcanza el antirrebote */

    feed_n(&k, BIT(KEY_5), 1u);
    CHECK(next_event_is(&k, KEY_5, HAL_KEYPAD_EV_PRESS));
    CHECK(queue_empty(&k));
    CHECK(k.stable == BIT(KEY_5));

    feed_n(&k, BIT(KEY_5), 50u); /* Mantenida: no repite */
    CHECK(queue_empty(&k));

    feed_n(&k, NO_KEYS, UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_5, HAL_KEYPAD_EV_RELEASE));
    CHECK(queue_empty(&k));
    CHECK(k.stable == NO_KEYS);
}

static void test_bounce_ignored(void)
{
    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);

    /* Rebote al presionar: alterna más rápido que el antirrebote */
    for (uint32_t i = 0u; i < 10u; i++) {
        feed_n(&k, BIT(KEY_1), UI_KEYPAD_DEBOUNCE_SCANS - 1u);
        feed_n(&k, NO_KEYS, 1u);
    }
    CHECK(queue_empty(&k));

    /* Se asienta: un solo evento */
    feed_n(&k, BIT(KEY_1), UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_1, HAL_KEYPAD_EV_PRESS));
    CHECK(queue_empty(&k));

    /* Rebote al soltar */
    for (uint32_t i = 0u; i < 10u; i++) {
        feed_n(&k, NO_KEYS, UI_KEYPAD_DEBOUNCE_SCANS - 1u);
        feed_n(&k, BIT(KEY_1), 1u);
    }
    CHECK(queue_empty(&k));
    CHECK(k.stable == BIT(KEY_1));

    feed_n(&k, NO_KEYS, UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_1, HAL_KEYPAD_EV_RELEASE));
    CHECK(queue_empty(&k));
}

static void test_glitch_ignored(void)
{
    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);
    /* Un barrido suelto con ruido no genera nada */
    feed_n(&k, BIT(KEY_D), 1u);
    feed_n(&k, NO_KEYS, 20u);
    CHECK(queue_empty(&k));
}

static void test_two_keys(void)
{
    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);

    /* 1 y 5: filas y columnas distintas */
    feed_n(&k, BIT(KEY_1) | BIT(KEY_5), UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_1, HAL_KEYPAD_EV_PRESS));
    CHECK(next_event_is(&k, KEY_5, HAL_KEYPAD_EV_PRESS));
    CHECK(queue_empty(&k));

    /* Se suelta solo el 1 */
    feed_n(&k, BIT(KEY_5), UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_1, HAL_KEYPAD_EV_RELEASE));
    CHECK(queue_empty(&k));
    CHECK(k.stable == BIT(KEY_5));

    /* 1 y 2: misma fila */
    hal_keypad_logic_init(&k);
    feed_n(&k, BIT(KEY_1) | BIT(KEY_2), UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_1, HAL_KEYPAD_EV_PRESS));
    CHECK(next_event_is(&k, KEY_2, HAL_KEYPAD_EV_PRESS));
    CHECK(k.ghost_scans == 0u);

    /* 1 y 4: misma columna */
    hal_keypad_logic_init(&k);
    feed_n(&k, BIT(KEY_1) | BIT(KEY_4), UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_1, HAL_KEYPAD_EV_PRESS));
    CHECK(next_event_is(&k, KEY_4, HAL_KEYPAD_EV_PRESS));
    CHECK(k.ghost_scans == 0u);
}

static void test_ghost_detection(void)
{
    /* Rectángulo 1-2-4-5: lo que se lee con tres de ellas apretadas */
    const uint16_t rect = BIT(KEY_1) | BIT(KEY_2) | BIT(KEY_4) | BIT(KEY_5);
    CHECK(hal_keypad_logic_is_ghost(rect));
    CHECK(!hal_keypad_logic_is_ghost(BIT(KEY_1) | BIT(KEY_2) | BIT(KEY_4))); /* L sin cerrar */
    CHECK(!hal_keypad_logic_is_ghost(BIT(KEY_1) | BIT(KEY_5)));
    CHECK(!hal_keypad_logic_is_ghost(NO_KEYS));
    CHECK(hal_keypad_logic_is_ghost(BIT(0u) | BIT(3u) | BIT(12u) | BIT(15u))); /* Esquinas */

    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);

    /* Con el 5 ya presionado, aparece el rectángulo: no cambia nada */
    feed_n(&k, BIT(KEY_5), UI_KEYPAD_DEBOUNCE_SCANS);
    CHECK(next_event_is(&k, KEY_5, HAL_KEYPAD_EV_PRESS));
    feed_n(&k, rect, 20u);
    CHECK(queue_empty(&k));
    CHECK(k.stable == BIT(KEY_5));
    CHECK(k.ghost_scans == 20u);

    /* Un barrido fantasma reinicia el antirrebote en curso */
    hal_keypad_logic_init(&k);
    feed_n(&k, BIT(KEY_1), UI_KEYPAD_DEBOUNCE_SCANS - 1u);
    feed_n(&k, rect, 1u);
    feed_n(&k, BIT(KEY_1), UI_KEYPAD_DEBOUNCE_SCANS - 1u);
    CHECK(queue_empty(&k));
    feed_n(&k, BIT(KEY_1), 1u);
    CHECK(next_event_is(&k, KEY_1, HAL_KEYPAD_EV_PRESS));
}

static void test_queue_full(void)
{
    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);

    /* Cada pulsación + soltar = 2 eventos; se generan más de los que entran */
    const uint32_t presses = UI_KEYPAD_EVENT_QUEUE_LEN;
    for (uint32_t i = 0u; i < presses; i++) {
        feed_n(&k, BIT(KEY_2), UI_KEYPAD_DEBOUNCE_SCANS);
        feed_n(&k, NO_KEYS, UI_KEYPAD_DEBOUNCE_SCANS);
    }
    CHECK(k.dropped_events == presses * 2u - UI_KEYPAD_EVENT_QUEUE_LEN);

    /* Quedan los primeros, en orden, y alternados */
    for (uint32_t i = 0u; i < UI_KEYPAD_EVENT_QUEUE_LEN; i++) {
        const hal_keypad_ev_type_t type = ((i % 2u) == 0u) ? HAL_KEYPAD_EV_PRESS
                                                           : HAL_KEYPAD_EV_RELEASE;
        CHECK(next_event_is(&k, KEY_2, type));
    }
    CHECK(queue_empty(&k));

    /* La cola sigue funcionando después de vaciarse (índices dan la vuelta) */
    for (uint32_t i = 0u; i < 3u * UI_KEYPAD_EVENT_QUEUE_LEN; i++) {
        feed_n(&k, BIT(KEY_D), UI_KEYPAD_DEBOUNCE_SCANS);
        CHECK(next_event_is(&k, KEY_D, HAL_KEYPAD_EV_PRESS));
        feed_n(&k, NO_KEYS, UI_KEYPAD_DEBOUNCE_SCANS);
        CHECK(next_event_is(&k, KEY_D, HAL_KEYPAD_EV_RELEASE));
    }
    CHECK(queue_empty(&k));
}

static void test_all_keys(void)
{
    hal_keypad_logic_t k;
    hal_keypad_logic_init(&k);
    for (uint32_t key = 0u; key < HAL_KEYPAD_KEY_COUNT; key++) {
        feed_n(&k, BIT(key), UI_KEYPAD_DEBOUNCE_SCANS);
        CHECK(next_event_is(&k, (uint8_t)key, HAL_KEYPAD_EV_PRESS));
        feed_n(&k, NO_KEYS, UI_KEYPAD_DEBOUNCE_SCANS);
        CHECK(next_event_is(&k, (uint8_t)key, HAL_KEYPAD_EV_RELEASE));
    }
    CHECK(queue_empty(&k));
    CHECK(k.dropped_events == 0u);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} test_case_t;

int main(void)
{
    static const test_case_t cases[] = {
        {"reposo sin eventos", test_idle_no_events},
        {"presionar y soltar una tecla", test_single_press_release},
        {"rebotes ignorados", test_bounce_ignored},
        {"ruido de un barrido ignorado", test_glitch_ignored},
        {"dos teclas a la vez", test_two_keys},
        {"teclas fantasma", test_ghost_detection},
        {"cola llena", test_queue_full},
        {"las 16 teclas", test_all_keys},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const int before = s_failures;
        cases[i].fn();
        printf("%s %s\n", (s_failures == before) ? "[ok]   " : "[FALLA]", cases[i].name);
    }

    printf("\n%d verificaciones, %d fallas\n", s_checks, s_failures);
    return (s_failures == 0) ? 0 : 1;
}
