// hal_wdt.c · Implementación del watchdog: habilitar y alimentar (SDK).

#include "hal_wdt.h"

#include "hardware/watchdog.h"

void hal_wdt_enable(uint32_t timeout_ms) {
    // pause_on_debug=true: el watchdog no reinicia mientras el depurador está detenido.
    watchdog_enable(timeout_ms, true);
}

void hal_wdt_feed(void) {
    watchdog_update();
}
