// hal_rand.h · Fuente de aleatoriedad por hardware de la Pico. Única capa (junto a los
// demás hal_*) que incluye headers del SDK. Se usa para el id único de cada sesión.
#ifndef REDUX_HAL_RAND_H
#define REDUX_HAL_RAND_H

#include <stdint.h>

// Devuelve 64 bits aleatorios del generador por hardware del RP2350 (get_rand_64). No se
// repite tras un reinicio, así el id de envío de una sesión nunca choca con otro previo.
uint64_t hal_rand_u64(void);

#endif // REDUX_HAL_RAND_H
