// hal_rand.c · Implementación de hal_rand con el RNG por hardware del SDK (pico_rand).

#include "hal_rand.h"

#include "pico/rand.h"

uint64_t hal_rand_u64(void) { return get_rand_64(); }
