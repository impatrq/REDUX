// angle_watch.h · Detector de saltos de ángulo entre ciclos. Lógica pura (depende solo
// de config): guarda el ángulo del tick anterior y marca falla si el salto supera
// ANGLE_JUMP_MAX_DEG. app lo usa solo en los estados armados (ARMING, RUN) y reinicia
// su referencia al entrar a cada uno, para que los movimientos hechos mientras estaba
// en FAULT o calibrando no disparen un falso FAULT_ANGLE_JUMP.
#ifndef REDUX_ANGLE_WATCH_H
#define REDUX_ANGLE_WATCH_H

#include <stdbool.h>

// Estado del detector. Un struct en cero equivale a "sin referencia": usar _reset.
typedef struct {
    bool have_prev; // ya hay un ángulo de referencia válido
    float prev_deg; // ángulo del tick anterior
} angle_watch_t;

// Invalida la referencia: la próxima muestra solo la fija y nunca marca salto.
void angle_watch_reset(angle_watch_t *w);

// Procesa el ángulo de este tick y actualiza la referencia. Devuelve true si el salto
// respecto del tick anterior superó ANGLE_JUMP_MAX_DEG. La primera muestra tras un
// reset (o tras _reset) nunca marca salto: solo establece la referencia.
bool angle_watch_jump(angle_watch_t *w, float angle_deg);

#endif // REDUX_ANGLE_WATCH_H
