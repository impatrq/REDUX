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
// Salto de ángulo entre ciclos (1 ms) que marca falla del encoder. 5° equivale a
// 5000 °/s: el codo humano más rápido ronda 300–600 °/s y el exo va muy por debajo,
// así que un salto así solo puede ser un fallo del sensor (patinada del imán, glitch
// de I2C), no movimiento real; el ruido de una lectura simple del AS5600 es ±0.1–0.3°,
// muy por debajo. Antes 30° (demasiado permisivo). Fase 6.
#define ANGLE_JUMP_MAX_DEG 5.0f

// --- Lógica de control pura (Fase 5) ------------------------------------------
// Convención de ángulo (Fase 3): 0° = flexión máxima; el ángulo crece al extender.
#define DUTY_MIN        0.15f  // piso de duty al arrancar el motor (intensidad 0)
#define BRAKE_ZONE_DEG  15.0f  // zona antes del límite donde el factor baja de 1.0 a 0.0
#define RAMP_UP_PER_S   1.5f   // pendiente máxima de subida del duty (1/s)
#define RAMP_DOWN_PER_S 4.0f   // pendiente máxima de bajada y frenado del duty (1/s)

_Static_assert(DUTY_MIN < DUTY_MAX, "DUTY_MIN debe ser menor que DUTY_MAX");

// --- Canal EMG y calibración (Fase 4) -----------------------------------------
// La señal real llega ya RECTIFICADA (onda completa), sin suavizar, 0–3 V, con 2k2
// en serie. GP26/ADC0 = bíceps, GP27/ADC1 = tríceps. La lectura y el sobremuestreo
// los hace el código que arma el tick (diag o app), no emg.c.
#define EMG_OVERSAMPLE 4u          // lecturas ADC por canal por tick, promediadas
#define EMG_RAW_SIGNAL 0           // 0: señal ya rectificada; 1: resta offset y rectifica
#define EMG_OFFSET_COUNTS 2048     // línea de base cruda (solo si EMG_RAW_SIGNAL == 1)
#define EMG_ENV_CUTOFF_HZ 4.0f     // corte del filtro exponencial de la envolvente
#define EMG_ON_MIN_MS 50u          // tiempo mínimo sobre el umbral antes de activar
#define EMG_SAT_COUNTS 4000u       // muestra cruda por encima de esto = saturación
#define EMG_SAT_MS 100u            // tiempo sostenido saturado para marcar el canal
#define EMG_K_NOISE 4.0f           // factor del desvío de reposo en el umbral de activación
#define EMG_MARGIN_MIN 30.0f       // margen mínimo (cuentas de envolvente) sobre la media
#define EMG_OFF_FRAC 0.6f          // histéresis: umbral de desactivación = media + margen·frac
#define EMG_REST_NOISE_MAX 60.0f   // desvío de reposo máximo; si se supera => "ruido en reposo"
#define EMG_MVC_USE 0.8f           // intensidad 1.0 queda a esta fracción del pico sobre el reposo
#define EMG_MVC_MIN_RATIO 1.5f     // el pico debe superar el umbral por este factor o => "débil"

// Duración de las tomas de calibración (no bloqueante: una muestra por tick de 1 ms).
#define CAL_REST_MS 2000u          // toma de reposo: media y desvío de la envolvente
#define CAL_MVC_MS 2000u           // toma de contracción máxima: pico de la envolvente

// --- Programa de banco de la Fase 4 (diag) ------------------------------------
#define FASE4_REPORT_MS 20u        // período del volcado CSV por USB
#define FASE4_CMD_LINE_MAX 32u     // largo máximo de una línea de comando por USB

// Nota: los parámetros del prototipo (umbrales EMG, histéresis, rango articular,
// rampas de potencia, watchdog) se vuelcan aquí en sus fases correspondientes
// (Fases 3, 4, 5 y 6), cada uno con su unidad en el nombre.

// --- Supervisor, watchdog e integración (Fase 6) -----------------------------
#define WDT_TIMEOUT_MS 500u      // el watchdog reinicia la Pico si el lazo no lo alimenta
#define CTRL_OVERRUN_MAX 3u      // sobrepasos de CTRL_TICK_US seguidos que dan FAULT_OVERRUN
#define REARM_OK_TICKS 500u      // lecturas estables en ST_FAULT antes de rearmar (0.5 s)
#define MAGNET_CHECK_TICKS 50u   // cada cuántos ticks se relee el estado del imán (0x0B)
#define STATUS_REPORT_MS 100u    // período del reporte de estado por USB
#define STATUS_TX_CHUNK 16u      // bytes del reporte que se drenan por vuelta del lazo
#define STATUS_LINE_MAX 160u     // largo máximo de una línea de estado
#define APP_CMD_LINE_MAX 32u     // largo máximo de una línea de comando por USB

// Fuente de la señal EMG. En esta fase (banco, sin electrodos) se usa SERIAL: el ADC
// flotante con los electrodos desconectados podría activar el motor. En la Fase 8, con
// el paciente y los EMG conectados, pasar EMG_SOURCE a EMG_SOURCE_ADC.
#define EMG_SOURCE_ADC 0    // señal real por ADC (GP26/GP27)
#define EMG_SOURCE_SERIAL 1 // señal simulada por comandos de serie ("b N", "t N", "r")
#define EMG_SOURCE EMG_SOURCE_SERIAL

// Modo banco: limita el duty del motor a un techo más seguro (DUTY_MAX_BENCH) mientras
// se prueba sin el brazo. Desactivar (BENCH_MODE 0) para los valores finales.
#define BENCH_MODE 1
#define DUTY_MAX_BENCH 0.5f
_Static_assert(DUTY_MAX_BENCH <= DUTY_MAX, "DUTY_MAX_BENCH no puede superar a DUTY_MAX");
#if BENCH_MODE
#define DUTY_CEIL DUTY_MAX_BENCH // techo efectivo de duty en esta build
#else
#define DUTY_CEIL DUTY_MAX
#endif

#endif // REDUX_CONFIG_H
