// as5600.h · Driver del encoder AS5600: lectura cruda del ángulo y estado del imán
// (MD, ML, MH), más la lectura promediada no bloqueante que usa la calibración.
// Depende de hal_i2c y config. No aplica límites ni conoce la articulación.
#ifndef REDUX_AS5600_H
#define REDUX_AS5600_H

#include <stdbool.h>
#include <stdint.h>

#include "redux_types.h" // status_t

// Resolución del AS5600: la lectura cruda va de 0 a AS5600_COUNTS-1 y vuelve a 0.
#define AS5600_COUNTS 4096

// Estado del imán decodificado del registro STATUS (0x0B).
typedef struct {
    bool detected;    // MD: hay un imán en rango
    bool too_weak;    // ML: el AGC llegó al máximo (imán demasiado lejos/débil)
    bool too_strong;  // MH: el AGC llegó al mínimo (imán demasiado cerca/fuerte)
} as5600_magnet_t;

// Lee RAW ANGLE (0x0C-0x0D) y lo devuelve en 0..AS5600_COUNTS-1 (int32_t).
// Devuelve STATUS_OK o el error del bus. raw_out no puede ser NULL.
status_t as5600_read_raw(int32_t *raw_out);

// Lee STATUS (0x0B) y decodifica MD/ML/MH. m no puede ser NULL.
status_t as5600_read_magnet(as5600_magnet_t *m);

// --- Lectura promediada no bloqueante ----------------------------------------
// Promedia AS5600_FILTER_SAMPLES muestras tomadas de a una por tick, de forma que
// el lazo nunca se bloquea esperando el filtro. Secuencia de uso:
//   as5600_avg_begin();                       // reinicia el acumulador
//   while (!as5600_avg_ready()) as5600_avg_sample();  // una muestra por tick
//   as5600_avg_result(&raw, &spread);         // promedio y dispersión en cuentas
// El acumulador usa la primera muestra como referencia y promedia diferencias
// envueltas (wrap_diff), así funciona aunque el recorrido cruce el 0 del imán.

// Reinicia el acumulador: la próxima muestra será la referencia.
void as5600_avg_begin(void);

// Toma UNA muestra cruda y la acumula. Pensada para llamarse una vez por tick.
// Devuelve el status del bus; ante un error el llamador aborta la lectura.
status_t as5600_avg_sample(void);

// true cuando ya se acumularon AS5600_FILTER_SAMPLES muestras.
bool as5600_avg_ready(void);

// Promedio (0..AS5600_COUNTS-1) y dispersión (max-min, en cuentas) del lote.
// Válido cuando as5600_avg_ready() es true. Los punteros no pueden ser NULL.
void as5600_avg_result(int32_t *raw_out, int32_t *spread_counts_out);

// --- Funciones puras (se prueban en PC sin hardware) -------------------------
// Diferencia envuelta raw-ref en cuentas, en el rango [-AS5600_COUNTS/2,
// AS5600_COUNTS/2 - 1]. Sin overflow aunque el recorrido cruce el 0 del imán:
//   ((raw - ref + 2048) mod 4096) - 2048.
int32_t as5600_wrap_diff(int32_t raw, int32_t ref);

// Convierte cuentas a grados: counts * 360 / AS5600_COUNTS.
float as5600_counts_to_deg(int32_t counts);

#endif // REDUX_AS5600_H
