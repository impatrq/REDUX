// hal_button.c · Implementación del pulsador con antirrebote por conteo (SDK).

#include "hal_button.h"

#include "hardware/gpio.h"

#include "board.h"
#include "config.h"

// Estado estable ya filtrado y conteo de muestras consecutivas que lo contradicen.
static bool g_stable_pressed = false;
static uint32_t g_contra_count = 0;

void hal_button_init(void) {
    gpio_init(PIN_BUTTON);
    gpio_set_dir(PIN_BUTTON, GPIO_IN);
    gpio_pull_up(PIN_BUTTON); // activo en bajo
    g_stable_pressed = false;
    g_contra_count = 0;
}

bool hal_button_poll(void) {
    const bool raw_pressed = (gpio_get(PIN_BUTTON) == 0); // activo en bajo

    if (raw_pressed == g_stable_pressed) {
        g_contra_count = 0; // la lectura coincide con el estado estable
        return false;
    }

    // La lectura difiere del estado estable: hay que sostenerla BTN_DEBOUNCE_MS
    // llamadas (una por ms) antes de aceptarla.
    g_contra_count++;
    if (g_contra_count < BTN_DEBOUNCE_MS) {
        return false;
    }

    g_contra_count = 0;
    g_stable_pressed = raw_pressed;
    return g_stable_pressed; // true solo en el flanco a "presionado"
}
