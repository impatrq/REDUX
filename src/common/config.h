// config.h · Todas las constantes ajustables del firmware (umbrales, límites, PWM),
// con unidad en el nombre (LIM_MAX_DEG, PWM_FREQ_HZ). Único lugar para ajustar.
// No debe tener valores repetidos en otro archivo.
#ifndef REDUX_CONFIG_H
#define REDUX_CONFIG_H

// --- Tiempo / lazo de control -------------------------------------------------
#define CTRL_TICK_US 1000u // período del tick de control (1 ms)

// --- PWM ----------------------------------------------------------------------
#define PWM_FREQ_HZ 20000u     // PWM del motor (BTS7960), Fase 2
#define LED_PWM_FREQ_HZ 1000u  // PWM de brillo del LED de estado, sin parpadeo visible

// --- Driver del motor BTS7960 (Fase 2) ----------------------------------------
#define DUTY_MAX 0.80f // techo de duty que el driver nunca supera (recorte en apply)

// --- I2C (AS5600, Fase 3) -----------------------------------------------------
#define I2C_BUS_FREQ_HZ 400000u // 400 kHz
#define I2C_TIMEOUT_US 2000u    // timeout obligatorio de toda operación de bus

// --- Pulsador -----------------------------------------------------------------
#define BTN_DEBOUNCE_MS 20u // tiempo de estabilidad para el antirrebote por software

// --- LED de estado ------------------------------------------------------------
#define LED_BLINK_TICKS 500u // medio período del parpadeo del firmware, en ticks (1 ms)

// --- Programa de prueba de la Fase 1 (diag) -----------------------------------
#define FASE1_REPORT_US 1000000u   // período de impresión por USB: 1 s
#define FASE1_LED_RAMP_STEP 0.002f // paso de brillo del LED por tick (rampa triangular)

// --- Programa de banco de la Fase 2 (diag) ------------------------------------
#define FASE2_DUTY_MAX_PCT 50u       // tope de % que acepta el programa de banco
#define FASE2_REVERSE_BRAKE_MS 200u  // freno obligatorio antes de invertir el sentido
#define FASE2_DEADMAN_MS 5000u       // sin comando en este tiempo => frenar solo
#define FASE2_CMD_LINE_MAX 32u       // largo máximo de una línea de comando por USB

// --- AS5600 y calibración del rango del paciente (Fase 3) ---------------------
// Convención: 0° = flexión máxima; el ángulo crece al extender; rom_deg = extensión
// máxima. Los límites de trabajo son [ROM_MARGIN_DEG, rom_deg - ROM_MARGIN_DEG].
#define AS5600_FILTER_SAMPLES 32u // muestras promediadas en la lectura estable (1 por tick)
#define ROM_MIN_DEG 20.0f         // recorrido mínimo aceptable del paciente
#define ROM_MAX_DEG 160.0f        // recorrido máximo aceptable del paciente
#define ROM_MARGIN_DEG 5.0f       // margen de software antes de cada tope mecánico
#define ROM_STABLE_MAX_DEG 2.0f   // dispersión máxima de las muestras al calibrar (si no, se rechaza)
#define ANGLE_JUMP_MAX_DEG 30.0f  // salto de ángulo entre ciclos que marca falla (lo usa el supervisor, Fase 6)

// --- Lógica de control pura (Fase 5) ------------------------------------------
// Convención de ángulo (Fase 3): 0° = flexión máxima; el ángulo crece al extender.
#define DUTY_MIN        0.15f  // piso de duty al arrancar el motor (intensidad 0)
#define BRAKE_ZONE_DEG  15.0f  // zona antes del límite donde el factor baja de 1.0 a 0.0
#define RAMP_UP_PER_S   1.5f   // pendiente máxima de subida del duty (1/s)
#define RAMP_DOWN_PER_S 4.0f   // pendiente máxima de bajada y frenado del duty (1/s)

_Static_assert(DUTY_MIN < DUTY_MAX, "DUTY_MIN debe ser menor que DUTY_MAX");

// Nota: los parámetros del prototipo (umbrales EMG, histéresis, rango articular,
// rampas de potencia, watchdog) se vuelcan aquí en sus fases correspondientes
// (Fases 3, 4, 5 y 6), cada uno con su unidad en el nombre.

#endif // REDUX_CONFIG_H
