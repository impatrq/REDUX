// intent.h · Lógica XOR pura: de (bíceps activo, tríceps activo) a FLEX, EXT, REPOSO
// o COCONTRACCIÓN. Depende solo de redux_types. No toca hardware.
#ifndef REDUX_INTENT_H
#define REDUX_INTENT_H

#include <stdbool.h>

#include "redux_types.h"

// Decide la intención a partir del estado de activación de cada músculo (tabla XOR):
//   ninguno activo      -> INTENT_REST
//   solo bíceps         -> INTENT_FLEX
//   solo tríceps        -> INTENT_EXT
//   ambos activos       -> INTENT_COCONTRACTION
// Función pura, sin estado.
intent_t intent_decide(bool bic_on, bool tri_on);

#endif // REDUX_INTENT_H
