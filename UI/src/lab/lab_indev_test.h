/**
 * @file lab_indev_test.h
 * @brief Prueba de U4 (solo del laboratorio). Todo corre en el core 1.
 *
 * Dos botones en un grupo y una etiqueta. Con el teclado como entrada de LVGL:
 * A y C mueven el foco, B aprieta el botón con foco y D muestra "Volver".
 * Cada acción se imprime por USB; cada LAB_INDEV_STATS_PERIOD_MS, los contadores.
 */
#ifndef LAB_INDEV_TEST_H
#define LAB_INDEV_TEST_H

/** Inicia el teclado como entrada de LVGL y arma la pantalla. Se pasa a ui_core1_start(). */
void lab_indev_test_setup(void);

#endif /* LAB_INDEV_TEST_H */
