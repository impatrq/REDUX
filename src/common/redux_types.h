// redux_types.h · Enums y structs compartidos por todas las capas:
// direction_t, intent_t, sys_state_t, fault_t, status_t. No incluye headers del SDK.
#ifndef REDUX_TYPES_H
#define REDUX_TYPES_H

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

// Estado del supervisor (ver app/supervisor).
typedef enum {
    ST_BOOT,
    ST_CHECK_ENC,
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
} fault_t;

// Resultado de toda operación de hardware. El llamador siempre lo revisa.
typedef enum {
    STATUS_OK = 0,
    STATUS_TIMEOUT,
    STATUS_NACK,
    STATUS_BAD_ARG,
} status_t;

#endif // REDUX_TYPES_H
