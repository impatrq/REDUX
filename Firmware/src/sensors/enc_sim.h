// enc_sim.h · Encoder simulado para probar sin hardware (opción CMake REDUX_ENC=SIM).
// Implementa TODA la API de as5600.h contra un ángulo interno, así rom_calib y app lo usan
// sin cambios. El ángulo se posiciona a mano para calibrar (comando serie "ang <grados>") y
// en marcha integra el duty aplicado al motor (ver enc_sim_integrate). Convención del
// firmware: 0° = flexión máxima; extender SUBE el ángulo, flexionar lo BAJA (entre 0 y
// ENC_SIM_MAX_DEG). El imán siempre se reporta OK. Nunca se compila por defecto (default
// AS5600); se elige explícitamente con -DREDUX_ENC=SIM.
#ifndef REDUX_ENC_SIM_H
#define REDUX_ENC_SIM_H

#include "redux_types.h" // direction_t

// Deja el ángulo simulado en 0° (flexión máxima) y reinicia el promediador.
void enc_sim_init(void);

// Posiciona el ángulo simulado (se recorta a [0, ENC_SIM_MAX_DEG]). Lo usa el comando serie
// "ang <grados>" para llevar el "brazo" a flexión/extensión durante la calibración del ROM.
void enc_sim_set_angle_deg(float deg);

// Integra un paso de 1 ms del motor: con DIR_EXT el ángulo sube, con DIR_FLEX baja, a una
// velocidad proporcional al duty (ENC_SIM_DEG_PER_S a duty 1.0). DIR_STOP no mueve nada.
void enc_sim_integrate(direction_t dir, float duty);

// Ángulo simulado actual, en grados (para diagnóstico).
float enc_sim_angle_deg(void);

#endif // REDUX_ENC_SIM_H
