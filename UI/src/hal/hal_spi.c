/**
 * @file hal_spi.c
 * @brief Bus SPI de la pantalla (SPI1) con envío por DMA y timeout.
 */
#include "hal/hal_spi.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

#include "hal/board.h"
#include "ui/ui_config.h"

#define HAL_SPI_BITS_PER_BYTE      8u
#define HAL_SPI_DATA_BITS_8        8u
#define HAL_SPI_DATA_BITS_16       16u
#define HAL_SPI_BYTES_PER_WORD16   2u
#define HAL_SPI_US_PER_S           1000000u

#define HAL_SPI_INST               SPI_INSTANCE(BOARD_LCD_SPI_INDEX)

static bool s_initialized = false;
static uint32_t s_baud_hz = 0u;
static uint s_dma_chan = 0u;
static absolute_time_t s_dma_deadline;
static volatile uint16_t s_fill_value = 0u; /* Fuente del DMA de relleno */

/** Plazo máximo para enviar @p bytes: tiempo teórico × factor + margen. */
static absolute_time_t hal_spi_deadline_for(size_t bytes)
{
    const uint64_t bits = (uint64_t)bytes * HAL_SPI_BITS_PER_BYTE;
    const uint64_t expected_us = (bits * HAL_SPI_US_PER_S) / s_baud_hz;
    const uint64_t timeout_us = expected_us * UI_SPI_TIMEOUT_FACTOR + UI_SPI_TIMEOUT_MARGIN_US;
    return make_timeout_time_us(timeout_us);
}

static void hal_spi_set_data_bits(uint bits)
{
    spi_set_format(HAL_SPI_INST, bits, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

/** Vacía la FIFO de recepción (no se usa) y borra el aviso de desborde. */
static void hal_spi_drain_rx(void)
{
    spi_hw_t *hw = spi_get_hw(HAL_SPI_INST);
    while ((hw->sr & SPI_SSPSR_RNE_BITS) != 0u) {
        (void)hw->dr;
    }
    hw->icr = SPI_SSPICR_RORIC_BITS;
}

/** Espera a que el bus deje de transmitir, hasta @p deadline. */
static hal_spi_status_t hal_spi_wait_idle(absolute_time_t deadline)
{
    while (spi_is_busy(HAL_SPI_INST)) {
        if (time_reached(deadline)) {
            return HAL_SPI_ERR_TIMEOUT;
        }
    }
    hal_spi_drain_rx();
    return HAL_SPI_OK;
}

void hal_spi_init(void)
{
    if (s_initialized) {
        return;
    }

    s_baud_hz = spi_init(HAL_SPI_INST, UI_LCD_SPI_BAUD_HZ);
    hal_spi_set_data_bits(HAL_SPI_DATA_BITS_8);
    gpio_set_function(BOARD_PIN_LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(BOARD_PIN_LCD_MOSI, GPIO_FUNC_SPI);

    gpio_init(BOARD_PIN_LCD_CS);
    gpio_put(BOARD_PIN_LCD_CS, true);
    gpio_set_dir(BOARD_PIN_LCD_CS, GPIO_OUT);

    s_dma_chan = (uint)dma_claim_unused_channel(true);
    s_dma_deadline = get_absolute_time();
    s_initialized = true;
}

uint32_t hal_spi_get_baud_hz(void)
{
    return s_baud_hz;
}

void hal_spi_cs_select(void)
{
    gpio_put(BOARD_PIN_LCD_CS, false);
}

void hal_spi_cs_deselect(void)
{
    gpio_put(BOARD_PIN_LCD_CS, true);
}

hal_spi_status_t hal_spi_write8(const uint8_t *data, size_t len)
{
    if (!s_initialized) {
        return HAL_SPI_ERR_NOT_INIT;
    }
    if (dma_channel_is_busy(s_dma_chan)) {
        return HAL_SPI_ERR_BUSY;
    }

    const absolute_time_t deadline = hal_spi_deadline_for(len);
    hal_spi_status_t st = hal_spi_wait_idle(deadline);
    if (st != HAL_SPI_OK) {
        return st;
    }
    hal_spi_set_data_bits(HAL_SPI_DATA_BITS_8);

    spi_hw_t *hw = spi_get_hw(HAL_SPI_INST);
    for (size_t i = 0u; i < len; i++) {
        while (!spi_is_writable(HAL_SPI_INST)) {
            if (time_reached(deadline)) {
                return HAL_SPI_ERR_TIMEOUT;
            }
        }
        hw->dr = (uint32_t)data[i];
    }
    return hal_spi_wait_idle(deadline);
}

/** Configura y dispara el DMA de 16 bits hacia el registro de datos del SPI. */
static hal_spi_status_t hal_spi_start_dma16(const volatile void *src, bool increment, size_t count)
{
    if (!s_initialized) {
        return HAL_SPI_ERR_NOT_INIT;
    }
    if (dma_channel_is_busy(s_dma_chan)) {
        return HAL_SPI_ERR_BUSY;
    }

    const absolute_time_t deadline = hal_spi_deadline_for(count * HAL_SPI_BYTES_PER_WORD16);
    const hal_spi_status_t st = hal_spi_wait_idle(deadline);
    if (st != HAL_SPI_OK) {
        return st;
    }
    hal_spi_set_data_bits(HAL_SPI_DATA_BITS_16);

    dma_channel_config cfg = dma_channel_get_default_config(s_dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, increment);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, spi_get_dreq(HAL_SPI_INST, true));

    s_dma_deadline = deadline;
    dma_channel_configure(s_dma_chan, &cfg, &spi_get_hw(HAL_SPI_INST)->dr, src,
                          (uint32_t)count, true);
    return HAL_SPI_OK;
}

hal_spi_status_t hal_spi_write16_dma(const uint16_t *data, size_t count)
{
    return hal_spi_start_dma16(data, true, count);
}

hal_spi_status_t hal_spi_fill16_dma(uint16_t value, size_t count)
{
    if (s_initialized && dma_channel_is_busy(s_dma_chan)) {
        return HAL_SPI_ERR_BUSY; /* No pisar la fuente de un relleno en curso */
    }
    s_fill_value = value;
    return hal_spi_start_dma16(&s_fill_value, false, count);
}

bool hal_spi_dma_busy(void)
{
    return s_initialized && (dma_channel_is_busy(s_dma_chan) || spi_is_busy(HAL_SPI_INST));
}

hal_spi_status_t hal_spi_dma_wait(void)
{
    if (!s_initialized) {
        return HAL_SPI_ERR_NOT_INIT;
    }
    while (dma_channel_is_busy(s_dma_chan)) {
        if (time_reached(s_dma_deadline)) {
            dma_channel_abort(s_dma_chan);
            return HAL_SPI_ERR_TIMEOUT;
        }
    }
    return hal_spi_wait_idle(s_dma_deadline);
}
