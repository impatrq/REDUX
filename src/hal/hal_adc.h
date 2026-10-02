// hal_adc.h · Iniciar el ADC y leer un canal en cuentas de 12 bits. No filtra la señal.
#ifndef REDUX_HAL_ADC_H
#define REDUX_HAL_ADC_H

#include <stdint.h>

// Inicializa el periférico ADC y habilita los pines de EMG (GP26/GP27) como
// entrada analógica. No toca el pin reservado de corriente (GP28) en la v1.
void hal_adc_init(void);

// Lee el canal ADC indicado (0..3) y devuelve la cuenta cruda de 12 bits (0..4095).
// No filtra ni promedia. Un canal fuera de rango devuelve 0.
uint16_t hal_adc_read(uint8_t channel);

#endif // REDUX_HAL_ADC_H
