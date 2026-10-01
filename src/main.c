// main.c · Punto de entrada: inicializa el SDK y los módulos, arranca core 1 y
// corre el lazo principal esperando el tick de 1 ms. No contiene lógica de control.

#include <stdio.h>

#include "pico/stdlib.h"

#define REDUX_FW_VERSION "0.1.0"

int main(void) {
    stdio_init_all();

    while (true) {
        printf("REDUX firmware v%s\n", REDUX_FW_VERSION);
        sleep_ms(1000);
    }
}
