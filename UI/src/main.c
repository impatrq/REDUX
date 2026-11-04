/**
 * @file main.c
 * @brief Punto de entrada del laboratorio de UI (solo del laboratorio).
 *
 * U5 (pantallas rediseñadas después): el core 0 arranca el Wi-Fi (hal_wifi) y el core 1
 * (ui_core1), donde corre LVGL con las pantallas de SquareLine (ui_panel) y un firmware falso
 * mínimo (lab_screens_test). Después, el core 0 solo atiende el Wi-Fi.
 * El core 0 no llama a LVGL ni al teclado. Las pruebas de U2 (lab_lvgl_test_setup),
 * U3 (lab_keypad_test_setup) y U4 (lab_indev_test_setup) siguen compilándose: para
 * volver a una, pasarla a ui_core1_start(). En U6 el firmware falso (fake_core)
 * pasa a correr acá, en el core 0, y habla con la UI por colas.
 */
#include <stdbool.h>
#include <stdio.h>

#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

#include "hal/hal_wifi.h"
#include "lab/lab_screens_test.h"
#include "ui/ui_config.h"
#include "ui/ui_core1.h"

#define LAB_USB_WAIT_MS       3000u  /* Espera al monitor serie para no perder el arranque */
#define LAB_USB_POLL_MS       10u

static void lab_wait_usb(void)
{
    for (uint32_t waited_ms = 0u; waited_ms < LAB_USB_WAIT_MS; waited_ms += LAB_USB_POLL_MS) {
        if (stdio_usb_connected()) {
            return;
        }
        sleep_ms(LAB_USB_POLL_MS);
    }
}

int main(void)
{
    stdio_init_all();
    lab_wait_usb();

    printf("REDUX UI lab | U5 | LVGL %s | handler %u ms | teclado cada %u ms\n",
           UI_LVGL_VERSION_STR, (unsigned)UI_LVGL_HANDLER_PERIOD_MS,
           (unsigned)UI_KEYPAD_SCAN_PERIOD_MS);

    /* Antes del core 1: las colas del Wi-Fi tienen que existir cuando la UI pide algo. */
    (void)hal_wifi_init();
    ui_core1_start(lab_screens_test_setup);

    for (;;) {
        hal_wifi_poll();
        sleep_ms(UI_WIFI_POLL_MS);
    }
}
