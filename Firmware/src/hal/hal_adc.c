// hal_adc.c · Implementación de la lectura de canales ADC en cuentas de 12 bits (SDK).

#include "hal_adc.h"

#include "hardware/adc.h"

#include "board.h"

// Cantidad de canales ADC de propósito general del RP2350 (GP26..GP29 -> 0..3).
#define ADC_CHANNEL_COUNT 4u

void hal_adc_init(void) {
    adc_init();
    adc_gpio_init(PIN_EMG_BICEPS);  // GP26 -> ADC0
    adc_gpio_init(PIN_EMG_TRICEPS); // GP27 -> ADC1
}

uint16_t hal_adc_read(uint8_t channel) {
    if (channel >= ADC_CHANNEL_COUNT) {
        return 0;
    }
    adc_select_input(channel);
    return adc_read(); // 12 bits: 0..4095
}
