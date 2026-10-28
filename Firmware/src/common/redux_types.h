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

// Evento de la interfaz de usuario (ver app/ui.h). Vive acá, en common, para que
// las capas inferiores (p. ej. sensors/rom_calib) lo reciban sin depender de app:
// las dependencias solo bajan. La implementación concreta (ui_basic, y a futuro la
// pantalla con teclado) traduce pulsador/serie/teclas a estos eventos.
typedef enum {
    UI_EV_NONE = 0,       // no hay evento pendiente
    UI_EV_CONFIRM,        // el usuario confirma el paso actual
    UI_EV_CANCEL,         // el usuario cancela la operación
    UI_EV_RECALIBRATE,    // el usuario pide repetir la calibración
} ui_event_t;

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
