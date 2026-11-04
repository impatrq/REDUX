/**
 * @file lab_keypad_test.h
 * @brief Prueba de U3 (solo del laboratorio). Todo corre en el core 1.
 *
 * Barre el teclado cada UI_KEYPAD_SCAN_PERIOD_MS desde un timer de LVGL e
 * imprime por USB cada evento presionar/soltar. En la pantalla muestra la
 * última tecla y cuántas pulsaciones lleva. Cada LAB_KEYPAD_STATS_PERIOD_MS
 * imprime los contadores (barridos, fantasmas, eventos perdidos).
 */
#ifndef LAB_KEYPAD_TEST_H
#define LAB_KEYPAD_TEST_H

/** Inicia el teclado, arma la pantalla y los timers. Se pasa a ui_core1_start(). */
void lab_keypad_test_setup(void);

#endif /* LAB_KEYPAD_TEST_H */
