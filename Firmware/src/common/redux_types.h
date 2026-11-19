// redux_types.h · Enums y structs compartidos por todas las capas:
// direction_t, intent_t, sys_state_t, fault_t, status_t. No incluye headers del SDK.
#ifndef REDUX_TYPES_H
#define REDUX_TYPES_H

// ui_event_t es parte del contrato único con la interfaz (app/ui.h). Vive allá para no
// duplicarlo entre el firmware y la UI; se incluye acá para que las capas inferiores (p. ej.
// sensors/rom_calib, app/supervisor) lo reciban por redux_types.h como hasta ahora. El
// header es autocontenido (solo stdbool/stdint), así que incluirlo no crea dependencia de app.
#include "app/ui.h" // ui_event_t

// Sentido pedido o aplicado al motor.
typedef enum {
    DIR_STOP = 0,
    DIR_FLEX = 1,
    DIR_EXT = -1,
} direction_t;

// Resultado de la lógica XOR de intención (ver control/intent).
typedef enum {
    INTENT_REST,
    INTENT_FLEX,
    INTENT_EXT,
    INTENT_COCONTRACTION,
} intent_t;

// Estado del supervisor (ver app/supervisor). ST_CALIB_ROM calibra el rango del
// paciente (Fase 3) antes de las dos tomas de EMG (reposo y contracción máxima).
typedef enum {
    ST_BOOT,
    ST_CHECK_ENC,
    ST_IDLE,       // encoder OK, sin sesión, motor apagado: listo para "sesion"
    ST_CALIB_ROM,
    ST_CALIB_REST,
    ST_CALIB_MVC,
    ST_ARMING,
    ST_RUN,
    ST_FAULT,
} sys_state_t;

// Causa de una falla. Máscara de bits: se pueden combinar con OR.
typedef enum {
    FAULT_NONE = 0u,
    FAULT_I2C = 1u << 0,
    FAULT_MAGNET = 1u << 1,
    FAULT_ANGLE_JUMP = 1u << 2,
    FAULT_OVERRUN = 1u << 3,
    FAULT_EMG_SAT = 1u << 4, // saturación de EMG en marcha (Fase 6)
} fault_t;

// Fallas del encoder: el rearme desde ST_FAULT con cualquiera de estos bits vuelve a
// ST_CALIB_ROM (el rango ya no es confiable); el resto rearma a ST_ARMING.
#define FAULT_ENC_MASK ((fault_t)(FAULT_I2C | FAULT_MAGNET | FAULT_ANGLE_JUMP))

// Resultado de toda operación de hardware. El llamador siempre lo revisa.
typedef enum {
    STATUS_OK = 0,
    STATUS_TIMEOUT,
    STATUS_NACK,
    STATUS_BAD_ARG,
} status_t;

#endif // REDUX_TYPES_H
