// intent.c · Implementación de la tabla XOR de intención (lógica pura, sin estado).

#include "intent.h"

intent_t intent_decide(bool bic_on, bool tri_on) {
    if (bic_on && tri_on) {
        return INTENT_COCONTRACTION;
    }
    if (bic_on) {
        return INTENT_FLEX;
    }
    if (tri_on) {
        return INTENT_EXT;
    }
    return INTENT_REST;
}
