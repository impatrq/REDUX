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
 * Versión del contrato: 1 (propuesta, pendiente de confirmar en S1/S2).
 */
#ifndef APP_UI_H
#define APP_UI_H

#include <stdbool.h>
#include <stdint.h>

#define UI_CONTRACT_VERSION 1u

/** Eventos que la UI envía al firmware. */
typedef enum {
    UI_EV_NONE = 0,
    UI_EV_CONFIRM,      /**< "Comenzar" en el menú, "Guardar" un extremo de calibración, "Reintentar" tras falla */
    UI_EV_CANCEL,       /**< "Terminar" la sesión o cancelar la calibración (tecla D) */
    UI_EV_RECALIBRATE   /**< Pedir nueva calibración del rango (hoy ningún botón lo envía) */
} ui_event_t;

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
} ui_status_t;

/** Inicializa la UI (en ui_panel: colas y arranque del core 1). Llamar desde el core 0. */
void ui_app_init(void);

/** Publica el estado actual (~20 Hz). No bloquea: si la cola está llena descarta el más viejo. */
void ui_app_publish_status(const ui_status_t *status);

/** Saca el próximo evento de la UI. Devuelve false si no hay. No bloquea. */
bool ui_app_poll_event(ui_event_t *event);

#endif /* APP_UI_H */
