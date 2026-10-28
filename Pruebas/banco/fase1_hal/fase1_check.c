// fase1_check.c · Programa de bring-up de la Fase 1 (no es el firmware). Ejecutable
// aparte (target fase1_check) que ejercita el HAL e imprime por USB: ticks por
// segundo (~1000), "pulsador" en cada pulsación y las lecturas crudas de los dos ADC;
// además varía el brillo del LED (GP2) por PWM. El motor nunca se activa.

#include <inttypes.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "board.h"
#include "config.h"
#include "hal_adc.h"
#include "hal_button.h"
#include "hal_gpio.h"
#include "hal_pwm.h"
#include "hal_time.h"

int main(void) {
    stdio_init_all();

    hal_gpio_init();                      // R_EN y L_EN en bajo: motor apagado
    hal_adc_init();                       // EMG bíceps (ADC0) y tríceps (ADC1)
    hal_pwm_init(PIN_LED, LED_PWM_FREQ_HZ); // el LED de estado se toma por PWM aquí
    hal_button_init();
    hal_time_init();

    printf("REDUX fase1_check: ticks/s, pulsador, ADC crudos y brillo PWM del LED\n");

    uint64_t window_start_us = hal_time_us();
    uint32_t window_start_ticks = hal_time_ticks();

    float brightness = 0.0f;
    float ramp = FASE1_LED_RAMP_STEP; // sentido actual de la rampa triangular

    while (true) {
        // Trabajo por tick de 1 ms: antirrebote del pulsador y rampa del LED.
        if (hal_time_tick_take()) {
            if (hal_button_poll()) {
                printf("pulsador\n");
            }

            brightness += ramp;
            if (brightness >= 1.0f) {
                brightness = 1.0f;
                ramp = -FASE1_LED_RAMP_STEP;
            } else if (brightness <= 0.0f) {
                brightness = 0.0f;
                ramp = FASE1_LED_RAMP_STEP;
            }
            hal_pwm_set_duty(PIN_LED, brightness);
        }

        // Reporte una vez por segundo, con la cuenta de ticks validada contra el
        // tiempo real medido en µs.
        const uint64_t now_us = hal_time_us();
        if (now_us - window_start_us >= FASE1_REPORT_US) {
            const uint32_t now_ticks = hal_time_ticks();
            const uint32_t ticks_per_sec = now_ticks - window_start_ticks;
            const uint16_t adc0 = hal_adc_read(ADC_CH_BICEPS);
            const uint16_t adc1 = hal_adc_read(ADC_CH_TRICEPS);

            printf("ticks/s=%" PRIu32 "  adc0(GP26)=%u  adc1(GP27)=%u\n", ticks_per_sec,
                   adc0, adc1);

            window_start_us += FASE1_REPORT_US; // mantener la fase del reporte
            window_start_ticks = now_ticks;
        }
    }
}
