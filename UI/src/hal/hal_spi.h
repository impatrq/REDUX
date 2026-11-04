/**
 * @file hal_spi.h
 * @brief Bus SPI de la pantalla (SPI1) con envío por DMA y timeout.
 *
 * Solo maneja el bus: SCK, MOSI, CS y un canal DMA. No sabe nada del panel
 * (DC, RST y BL los maneja ui/st7735.c).
 *
 * Toda espera tiene timeout: el plazo se calcula a partir del tiempo teórico
 * de la transferencia (ver UI_SPI_TIMEOUT_* en ui/ui_config.h).
 *
 * No es seguro entre cores: lo usa un solo core (el de la UI).
 */
#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HAL_SPI_OK = 0,
    HAL_SPI_ERR_TIMEOUT,   /* La transferencia no terminó en el plazo */
    HAL_SPI_ERR_BUSY,      /* Se pidió un envío con un DMA todavía en curso */
    HAL_SPI_ERR_NOT_INIT,  /* Falta llamar a hal_spi_init() */
} hal_spi_status_t;

/** Configura SPI1, los pines del bus y reserva un canal DMA. Idempotente. */
void hal_spi_init(void);

/** Baudios reales que quedaron configurados (pueden diferir de los pedidos). */
uint32_t hal_spi_get_baud_hz(void);

/** CS activo (bajo) / inactivo (alto). */
void hal_spi_cs_select(void);
void hal_spi_cs_deselect(void);

/** Envía bytes en modo 8 bits por polling y espera a que el bus quede libre. */
hal_spi_status_t hal_spi_write8(const uint8_t *data, size_t len);

/**
 * Inicia el envío de @p count palabras de 16 bits por DMA (MSB primero).
 * No bloquea: el buffer debe seguir vivo hasta hal_spi_dma_wait().
 */
hal_spi_status_t hal_spi_write16_dma(const uint16_t *data, size_t count);

/** Inicia el envío de @p count copias de @p value (16 bits) por DMA. No bloquea. */
hal_spi_status_t hal_spi_fill16_dma(uint16_t value, size_t count);

/** true mientras haya un envío por DMA en curso o el bus siga transmitiendo. */
bool hal_spi_dma_busy(void);

/**
 * Espera a que termine el envío por DMA y el bus quede libre.
 * Si vence el plazo, aborta el DMA y devuelve HAL_SPI_ERR_TIMEOUT.
 */
hal_spi_status_t hal_spi_dma_wait(void);

#endif /* HAL_SPI_H */
