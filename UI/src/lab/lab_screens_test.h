/**
 * @file lab_screens_test.h
 * @brief Prueba de las pantallas (solo del laboratorio). Todo corre en el core 1.
 *
 * Un firmware falso mínimo, sin colas (eso es U6): arma un ui_status_t, se lo
 * pasa a ui_panel cada UI_STATUS_PERIOD_MS y avanza según los eventos de la UI.
 *
 * Recorrido: Inicio (LAB_BOOT_MS) -> menú. Comenzar -> calibración del rango
 * (flexión, extensión, cada una con Guardar) -> marcha (el ángulo sube y baja y
 * la potencia lo sigue de 0 a 100 %). Terminar -> fin de sesión -> menú.
 * A los LAB_RUN_FAULT_MS de marcha, falla simulada (una distinta cada vez);
 * Reintentar la rearma -> menú. Cada evento y cambio de modo se imprime por USB.
 */
#ifndef LAB_SCREENS_TEST_H
#define LAB_SCREENS_TEST_H

/** Arma las pantallas (ui_panel) y el firmware falso. Se pasa a ui_core1_start(). */
void lab_screens_test_setup(void);

#endif /* LAB_SCREENS_TEST_H */
