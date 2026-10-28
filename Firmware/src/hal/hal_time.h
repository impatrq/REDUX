// hal_time.h · Tick de 1 ms por repeating_timer (la ISR solo levanta una bandera
// volatile), tiempo en µs (hal_time_us) y delays. Nunca ejecuta lógica en la ISR.
#ifndef REDUX_HAL_TIME_H
#define REDUX_HAL_TIME_H

#include <stdbool.h>
#include <stdint.h>

// Arranca el repeating_timer de 1 ms. Idempotente.
void hal_time_init(void);

// Contador monotónico de ticks de 1 ms desde hal_time_init(). No se pierde ningún
// tick aunque el lazo tarde más de 1 ms: la ISR lo incrementa siempre.
uint32_t hal_time_ticks(void);

// Si hubo al menos un tick pendiente desde la última llamada, lo consume y devuelve
// true; si no, devuelve false. Para marcar la cadencia del lazo principal.
bool hal_time_tick_take(void);

// Tiempo transcurrido en microsegundos desde el arranque del sistema.
uint64_t hal_time_us(void);

#endif // REDUX_HAL_TIME_H
