// hal_wdt.h · Habilitar y alimentar el watchdog.
#ifndef REDUX_HAL_WDT_H
#define REDUX_HAL_WDT_H

#include <stdint.h>

// Habilita el watchdog: si no se lo alimenta en `timeout_ms`, la Pico se reinicia.
// En la v1 se llama recién después de la calibración (Fase 6), no en la Fase 1.
void hal_wdt_enable(uint32_t timeout_ms);

// Alimenta el watchdog. Se llama una vez por ciclo de control mientras está activo.
void hal_wdt_feed(void);

#endif // REDUX_HAL_WDT_H
