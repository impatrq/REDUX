/**
 * @file ui_core1.h
 * @brief Bucle del core 1: LVGL y nada más.
 *
 * Es el único lugar donde corre LVGL. El core 0 solo llama a ui_core1_start().
 * Desde U6 este bucle también atiende las colas del contrato (app/ui.h).
 */
#ifndef UI_CORE1_H
#define UI_CORE1_H

/** Arma las pantallas. Corre en el core 1, después de iniciar LVGL y el display. */
typedef void (*ui_core1_setup_fn_t)(void);

/**
 * Lanza el core 1 con su propia pila: lv_init(), display, @p setup y luego
 * lv_timer_handler() cada UI_LVGL_HANDLER_PERIOD_MS. Llamar una vez, desde el core 0.
 */
void ui_core1_start(ui_core1_setup_fn_t setup);

#endif /* UI_CORE1_H */
