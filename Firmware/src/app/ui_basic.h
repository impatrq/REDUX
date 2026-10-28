// ui_basic.h · Implementación de ui.h con pulsador (GP3), LED de estado (GP2) y
// monitor serie por USB. Expone, además del contrato ui.h, la entrada de comandos
// por línea serie: eso es propio de esta implementación, no del contrato general.
#ifndef REDUX_UI_BASIC_H
#define REDUX_UI_BASIC_H

#include "ui.h" // implementa este contrato (ui_init, ui_tick, ui_poll, ...)

// Traduce una línea recibida por el monitor serie a un evento de ui:
//   "ok"    -> UI_EV_CONFIRM
//   "cancel"-> UI_EV_CANCEL
//   "recal" -> UI_EV_RECALIBRATE
// Cualquier otra línea se ignora. El evento queda en la cola de ui_poll().
void ui_basic_feed_line(const char *line);

#endif // REDUX_UI_BASIC_H
