// hal_button.h · Pulsador en GP3 con pull-up interno, activo en bajo, y antirrebote
// por software. No bloquea: informa una sola vez cada pulsación.
#ifndef REDUX_HAL_BUTTON_H
#define REDUX_HAL_BUTTON_H

#include <stdbool.h>

// Configura GP3 como entrada con pull-up interno. El estado inicial es "no presionado".
void hal_button_init(void);

// Antirrebote por conteo: pensado para llamarse una vez por tick de 1 ms. Devuelve
// true exactamente una vez por pulsación, en el flanco de "soltado" a "presionado"
// ya estabilizado (BTN_DEBOUNCE_MS). Nunca bloquea.
bool hal_button_poll(void);

#endif // REDUX_HAL_BUTTON_H
