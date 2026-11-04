/**
 * @file st7735.c
 * @brief Driver del panel ST7735 1,44" 128x128 (RGB565) sobre hal_spi.
 *
 * Secuencia de inicio: la del módulo 1,44" "green tab" (datasheet ST7735R,
 * sección 10 + valores de fábrica de los módulos comunes).
 */
#include "ui/st7735.h"

#include "hardware/gpio.h"
#include "pico/time.h"

#include "hal/board.h"
#include "ui/ui_config.h"

/* ---- Comandos del ST7735 (datasheet, sección 10) ---- */
#define ST7735_CMD_SWRESET   0x01u
#define ST7735_CMD_SLPOUT    0x11u
#define ST7735_CMD_NORON     0x13u
#define ST7735_CMD_INVOFF    0x20u
#define ST7735_CMD_INVON     0x21u
#define ST7735_CMD_DISPON    0x29u
#define ST7735_CMD_CASET     0x2Au
#define ST7735_CMD_RASET     0x2Bu
#define ST7735_CMD_RAMWR     0x2Cu
#define ST7735_CMD_MADCTL    0x36u
#define ST7735_CMD_COLMOD    0x3Au
#define ST7735_CMD_FRMCTR1   0xB1u
#define ST7735_CMD_FRMCTR2   0xB2u
#define ST7735_CMD_FRMCTR3   0xB3u
#define ST7735_CMD_INVCTR    0xB4u
#define ST7735_CMD_PWCTR1    0xC0u
#define ST7735_CMD_PWCTR2    0xC1u
#define ST7735_CMD_PWCTR3    0xC2u
#define ST7735_CMD_PWCTR4    0xC3u
#define ST7735_CMD_PWCTR5    0xC4u
#define ST7735_CMD_VMCTR1    0xC5u
#define ST7735_CMD_GMCTRP1   0xE0u
#define ST7735_CMD_GMCTRN1   0xE1u

/* ---- Bits de MADCTL ---- */
#define ST7735_MADCTL_MY     0x80u
#define ST7735_MADCTL_MX     0x40u
#define ST7735_MADCTL_MV     0x20u
#define ST7735_MADCTL_BGR    0x08u

#define ST7735_COLMOD_16BIT  0x05u

/* ---- Esperas del datasheet ---- */
#define ST7735_SWRESET_WAIT_MS  150u
#define ST7735_SLPOUT_WAIT_MS   255u
#define ST7735_NORON_WAIT_MS    10u
#define ST7735_DISPON_WAIT_MS   100u
#define ST7735_RESET_HIGH_MS    5u

#define ST7735_INIT_MAX_PARAMS  16u
#define ST7735_WINDOW_PARAMS    4u
#define ST7735_BYTE_SHIFT       8u
#define ST7735_BYTE_MASK        0xFFu

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t params[ST7735_INIT_MAX_PARAMS];
    uint16_t wait_ms;
} st7735_init_cmd_t;

/* MADCTL e INVON/INVOFF salen de ui_config.h y se envían aparte. */
static const st7735_init_cmd_t s_init_seq[] = {
    { ST7735_CMD_SWRESET, 0u, { 0u }, ST7735_SWRESET_WAIT_MS },
    { ST7735_CMD_SLPOUT,  0u, { 0u }, ST7735_SLPOUT_WAIT_MS },
    { ST7735_CMD_FRMCTR1, 3u, { 0x01, 0x2C, 0x2D }, 0u },              /* Frecuencia de cuadro, modo normal */
    { ST7735_CMD_FRMCTR2, 3u, { 0x01, 0x2C, 0x2D }, 0u },              /* Modo inactivo */
    { ST7735_CMD_FRMCTR3, 6u, { 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D }, 0u }, /* Modo parcial */
    { ST7735_CMD_INVCTR,  1u, { 0x07 }, 0u },                           /* Sin inversión de líneas */
    { ST7735_CMD_PWCTR1,  3u, { 0xA2, 0x02, 0x84 }, 0u },
    { ST7735_CMD_PWCTR2,  1u, { 0xC5 }, 0u },
    { ST7735_CMD_PWCTR3,  2u, { 0x0A, 0x00 }, 0u },
    { ST7735_CMD_PWCTR4,  2u, { 0x8A, 0x2A }, 0u },
    { ST7735_CMD_PWCTR5,  2u, { 0x8A, 0xEE }, 0u },
    { ST7735_CMD_VMCTR1,  1u, { 0x0E }, 0u },
    { ST7735_CMD_COLMOD,  1u, { ST7735_COLMOD_16BIT }, 0u },
    { ST7735_CMD_GMCTRP1, 16u, { 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                                 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 }, 0u },
    { ST7735_CMD_GMCTRN1, 16u, { 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                                 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 }, 0u },
};

#define ST7735_INIT_SEQ_LEN  (sizeof(s_init_seq) / sizeof(s_init_seq[0]))

static bool s_write_pending = false; /* Envío de st7735_write_pixels_start() sin cerrar */

/** Envía un comando y sus parámetros. Requiere CS ya seleccionado. */
static hal_spi_status_t st7735_send_cmd(uint8_t cmd, const uint8_t *params, size_t len)
{
    gpio_put(BOARD_PIN_LCD_DC, false);
    hal_spi_status_t st = hal_spi_write8(&cmd, 1u);
    gpio_put(BOARD_PIN_LCD_DC, true);
    if ((st == HAL_SPI_OK) && (len > 0u)) {
        st = hal_spi_write8(params, len);
    }
    return st;
}

/** Comando aislado: selecciona CS, envía y libera. */
static hal_spi_status_t st7735_cmd(uint8_t cmd, const uint8_t *params, size_t len)
{
    hal_spi_cs_select();
    const hal_spi_status_t st = st7735_send_cmd(cmd, params, len);
    hal_spi_cs_deselect();
    return st;
}

static uint8_t st7735_madctl_value(void)
{
    uint8_t madctl = 0u;
    if (UI_LCD_MIRROR_Y != 0u) {
        madctl |= ST7735_MADCTL_MY;
    }
    if (UI_LCD_MIRROR_X != 0u) {
        madctl |= ST7735_MADCTL_MX;
    }
    if (UI_LCD_SWAP_XY != 0u) {
        madctl |= ST7735_MADCTL_MV;
    }
    if (UI_LCD_COLOR_BGR != 0u) {
        madctl |= ST7735_MADCTL_BGR;
    }
    return madctl;
}

static void st7735_hw_reset(void)
{
    gpio_put(BOARD_PIN_LCD_RST, true);
    sleep_ms(ST7735_RESET_HIGH_MS);
    gpio_put(BOARD_PIN_LCD_RST, false);
    sleep_ms(UI_LCD_RESET_PULSE_MS);
    gpio_put(BOARD_PIN_LCD_RST, true);
    sleep_ms(UI_LCD_RESET_WAIT_MS);
}

static void st7735_init_out_pin(uint pin, bool level)
{
    gpio_init(pin);
    gpio_put(pin, level);
    gpio_set_dir(pin, GPIO_OUT);
}

st7735_status_t st7735_init(void)
{
    hal_spi_init();
    st7735_init_out_pin(BOARD_PIN_LCD_DC, true);
    st7735_init_out_pin(BOARD_PIN_LCD_RST, true);
    st7735_init_out_pin(BOARD_PIN_LCD_BL, false);

    st7735_hw_reset();

    for (size_t i = 0u; i < ST7735_INIT_SEQ_LEN; i++) {
        const st7735_init_cmd_t *c = &s_init_seq[i];
        if (st7735_cmd(c->cmd, c->params, c->len) != HAL_SPI_OK) {
            return ST7735_ERR_BUS;
        }
        if (c->wait_ms > 0u) {
            sleep_ms(c->wait_ms);
        }
    }

    const uint8_t madctl = st7735_madctl_value();
    const uint8_t inv_cmd = (UI_LCD_INVERT != 0u) ? ST7735_CMD_INVON : ST7735_CMD_INVOFF;
    if ((st7735_cmd(ST7735_CMD_MADCTL, &madctl, 1u) != HAL_SPI_OK) ||
        (st7735_cmd(inv_cmd, NULL, 0u) != HAL_SPI_OK) ||
        (st7735_cmd(ST7735_CMD_NORON, NULL, 0u) != HAL_SPI_OK)) {
        return ST7735_ERR_BUS;
    }
    sleep_ms(ST7735_NORON_WAIT_MS);
    if (st7735_cmd(ST7735_CMD_DISPON, NULL, 0u) != HAL_SPI_OK) {
        return ST7735_ERR_BUS;
    }
    sleep_ms(ST7735_DISPON_WAIT_MS);
    return ST7735_OK;
}

void st7735_backlight_set(bool on)
{
    gpio_put(BOARD_PIN_LCD_BL, on);
}

static bool st7735_rect_valid(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    return (w > 0u) && (h > 0u) &&
           ((uint32_t)x + w <= UI_LCD_WIDTH_PX) &&
           ((uint32_t)y + h <= UI_LCD_HEIGHT_PX);
}

/** Envía un par CASET/RASET: inicio y fin (inclusive) en 16 bits, MSB primero. */
static hal_spi_status_t st7735_send_range(uint8_t cmd, uint32_t start, uint32_t end)
{
    const uint8_t params[ST7735_WINDOW_PARAMS] = {
        (uint8_t)((start >> ST7735_BYTE_SHIFT) & ST7735_BYTE_MASK),
        (uint8_t)(start & ST7735_BYTE_MASK),
        (uint8_t)((end >> ST7735_BYTE_SHIFT) & ST7735_BYTE_MASK),
        (uint8_t)(end & ST7735_BYTE_MASK),
    };
    return st7735_send_cmd(cmd, params, ST7735_WINDOW_PARAMS);
}

/** Selecciona CS, fija la ventana y deja el panel esperando píxeles (DC = datos). */
static hal_spi_status_t st7735_begin_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    const uint32_t x0 = (uint32_t)x + UI_LCD_COL_OFFSET_PX;
    const uint32_t y0 = (uint32_t)y + UI_LCD_ROW_OFFSET_PX;

    hal_spi_cs_select();
    hal_spi_status_t st = st7735_send_range(ST7735_CMD_CASET, x0, x0 + w - 1u);
    if (st == HAL_SPI_OK) {
        st = st7735_send_range(ST7735_CMD_RASET, y0, y0 + h - 1u);
    }
    if (st == HAL_SPI_OK) {
        st = st7735_send_cmd(ST7735_CMD_RAMWR, NULL, 0u);
    }
    if (st != HAL_SPI_OK) {
        hal_spi_cs_deselect();
    }
    return st;
}

/** Espera el DMA de píxeles y libera CS. */
static st7735_status_t st7735_end_window(hal_spi_status_t start_status)
{
    hal_spi_status_t st = start_status;
    if (st == HAL_SPI_OK) {
        st = hal_spi_dma_wait();
    }
    hal_spi_cs_deselect();
    return (st == HAL_SPI_OK) ? ST7735_OK : ST7735_ERR_BUS;
}

st7735_status_t st7735_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (s_write_pending) {
        return ST7735_ERR_BUSY;
    }
    if (!st7735_rect_valid(x, y, w, h)) {
        return ST7735_ERR_RANGE;
    }
    if (st7735_begin_window(x, y, w, h) != HAL_SPI_OK) {
        return ST7735_ERR_BUS;
    }
    return st7735_end_window(hal_spi_fill16_dma(color, (size_t)w * h));
}

st7735_status_t st7735_fill_screen(uint16_t color)
{
    return st7735_fill_rect(0u, 0u, UI_LCD_WIDTH_PX, UI_LCD_HEIGHT_PX, color);
}

st7735_status_t st7735_write_pixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                    const uint16_t *pixels)
{
    const st7735_status_t st = st7735_write_pixels_start(x, y, w, h, pixels);
    return (st == ST7735_OK) ? st7735_write_finish() : st;
}

st7735_status_t st7735_write_pixels_start(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                          const uint16_t *pixels)
{
    if (s_write_pending) {
        return ST7735_ERR_BUSY;
    }
    if ((pixels == NULL) || !st7735_rect_valid(x, y, w, h)) {
        return ST7735_ERR_RANGE;
    }
    if (st7735_begin_window(x, y, w, h) != HAL_SPI_OK) {
        return ST7735_ERR_BUS;
    }
    if (hal_spi_write16_dma(pixels, (size_t)w * h) != HAL_SPI_OK) {
        hal_spi_cs_deselect();
        return ST7735_ERR_BUS;
    }
    s_write_pending = true;
    return ST7735_OK;
}

bool st7735_write_busy(void)
{
    return s_write_pending && hal_spi_dma_busy();
}

st7735_status_t st7735_write_finish(void)
{
    if (!s_write_pending) {
        return ST7735_OK;
    }
    s_write_pending = false;
    return st7735_end_window(HAL_SPI_OK);
}
