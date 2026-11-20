/**
 * @file ui.h
 * @brief Contrato entre el firmware de REDUX y la interfaz de usuario.
 *
 * Único punto de contacto entre la UI y el firmware (docs/ui_contract.md).
 * Es AUTOCONTENIDO: no incluye archivos del firmware. El supervisor traduce
 * sus propios estados y fallas a los tipos de este archivo.
 *
 * Dos implementaciones posibles, elegidas al compilar:
 *  - ui_basic: pulsador + LED (firmware).
 *  - ui_panel: pantalla ST7735 + teclado 4x4 con LVGL (este laboratorio).
 *
 * Siempre incluir como "app/ui.h": el export de SquareLine tiene su propio
 * ui.h y su propia ui_init(); por eso las funciones de acá usan el prefijo ui_app_.
 *
 * Versión del contrato: 2 (propuesta, pendiente de confirmar en S1/S2).
 */
#ifndef APP_UI_H
#define APP_UI_H

#include <stdbool.h>
#include <stdint.h>

#define UI_CONTRACT_VERSION 2u

/** Largo máximo del ID de paciente (dígitos); el buffer reserva uno más para el '\0'. */
#define UI_PATIENT_ID_MAX_LEN 15u

/** Brazo que se calibra y asiste en la sesión. */
typedef enum {
    UI_ARM_LEFT = 0,    /**< Izquierdo */
    UI_ARM_RIGHT        /**< Derecho */
} ui_arm_t;

/** Eventos que la UI envía al firmware. */
typedef enum {
    UI_EV_NONE = 0,
    UI_EV_CONFIRM,      /**< "Guardar" un extremo de calibración, "Reintentar" tras falla */
    UI_EV_CANCEL,       /**< "Terminar" la sesión o cancelar la calibración (tecla D) */
    UI_EV_RECALIBRATE,  /**< Pedir nueva calibración del rango (hoy ningún botón lo envía) */
    UI_EV_START_SESSION /**< v2: iniciar sesión con brazo + ID (payload en ui_event_msg_t). Al final del enum */
} ui_event_t;

/**
 * Evento con datos (UI → firmware). Tamaño fijo, se copia por valor.
 * Hoy solo lo usa UI_EV_START_SESSION (lleva brazo e ID); el resto de los eventos
 * viajan como ui_event_t por ui_app_poll_event().
 */
typedef struct {
    ui_event_t type;
    ui_arm_t   arm;                                    /**< Válido solo en UI_EV_START_SESSION */
    char       patient_id[UI_PATIENT_ID_MAX_LEN + 1u]; /**< Válido solo en START_SESSION; "" si está vacío */
} ui_event_msg_t;

/** Modo general que la UI debe mostrar. */
typedef enum {
    UI_MODE_BOOT = 0,   /**< Inicio / verificando encoder */
    UI_MODE_CALIB_ROM,  /**< Calibración del rango articular */
    UI_MODE_CALIB_EMG,  /**< Calibración de los EMG */
    UI_MODE_RUN,        /**< En marcha */
    UI_MODE_FAULT,      /**< Falla: esperando rearme */
    UI_MODE_IDLE        /**< Listo, sin sesión: la UI muestra su menú; UI_EV_CONFIRM inicia la sesión */
} ui_mode_t;

/** Indicación que la UI debe pedirle al usuario. */
typedef enum {
    UI_PROMPT_NONE = 0,
    UI_PROMPT_GO_FLEX,          /**< Llevar el brazo a flexión máxima y apretar Guardar */
    UI_PROMPT_GO_EXT,           /**< Llevar el brazo a extensión máxima y apretar Guardar */
    UI_PROMPT_RELAX,            /**< Relajar el brazo */
    UI_PROMPT_CONTRACT_BICEPS,  /**< Contraer el bíceps */
    UI_PROMPT_CONTRACT_TRICEPS  /**< Contraer el tríceps */
} ui_prompt_t;

/** Bits de la máscara de fallas que muestra la UI. */
#define UI_FAULT_NONE         0u
#define UI_FAULT_ENCODER      (1u << 0)  /**< Encoder ausente o lectura inválida */
#define UI_FAULT_OVERCURRENT  (1u << 1)  /**< Sobrecorriente en el puente H */
#define UI_FAULT_ANGLE_LIMIT  (1u << 2)  /**< Fuera del rango calibrado */
#define UI_FAULT_EMG          (1u << 3)  /**< Señal EMG inválida */
#define UI_FAULT_OTHER        (1u << 31) /**< Cualquier otra falla del firmware */

/** Mensaje de estado firmware -> UI. Tamaño fijo, se copia por valor en la cola. */
typedef struct {
    ui_mode_t   mode;
    ui_prompt_t prompt;
    float       angle_deg;  /**< Ángulo actual, 0° = flexión máxima */
    float       rom_deg;    /**< Rango calibrado, 0 si no hay calibración */
    uint32_t    faults;     /**< Máscara UI_FAULT_* */
    float       duty;       /**< Potencia aplicada 0.0–1.0 (solo para mostrar) */
    uint32_t    rep_count;    /**< v2: repeticiones completadas en la sesión */
    float       last_rom_deg; /**< v2: rango (grados) de la última repetición, 0 si todavía no hay */
} ui_status_t;

/** Inicializa la UI (en ui_panel: colas y arranque del core 1). Llamar desde el core 0. */
void ui_app_init(void);

/** Publica el estado actual (~20 Hz). No bloquea: si la cola está llena descarta el más viejo. */
void ui_app_publish_status(const ui_status_t *status);

/** Saca el próximo evento de la UI. Devuelve false si no hay. No bloquea. */
bool ui_app_poll_event(ui_event_t *event);

/**
 * Publica la configuración de la sesión a iniciar (brazo + ID de paciente).
 * Buffer de ÚLTIMO VALOR, no cola: el firmware lee el más reciente con
 * ui_app_take_session() al sacar UI_EV_START_SESSION. La llama la UI (core 1).
 * Protección del buffer (lado firmware): una critical_section de la Pico protege
 * una copia única + un flag "sin leer"; el lock se sostiene solo durante el memcpy.
 */
void ui_app_publish_session(const ui_event_msg_t *msg);

/**
 * Toma la última sesión publicada. Devuelve true y copia en *out si hay una sin
 * leer (y la marca leída); false si no hay novedad. La llama el firmware (core 0).
 */
bool ui_app_take_session(ui_event_msg_t *out);

#endif /* APP_UI_H */
