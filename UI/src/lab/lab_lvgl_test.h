/**
 * @file lab_lvgl_test.h
 * @brief Pruebas de U2 (solo del laboratorio). Todo corre en el core 1.
 *
 * Prueba 1: etiqueta "REDUX" y un contador que se actualiza.
 * Prueba 2: después de LAB_TEST1_DURATION_MS, la primera pantalla del export
 * de SquareLine (ui_init()).
 * En las dos se mide un refresco completo cada LAB_MEASURE_PERIOD_MS y se
 * imprime por USB.
 */
#ifndef LAB_LVGL_TEST_H
#define LAB_LVGL_TEST_H

/** Arma la prueba 1 y los timers. Se pasa a ui_core1_start(). */
void lab_lvgl_test_setup(void);

#endif /* LAB_LVGL_TEST_H */
