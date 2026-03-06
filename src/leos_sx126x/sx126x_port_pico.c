#include "sx126x_priv.h"

#include <stdarg.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#ifndef LEOS_SX126X_SPI_PORT
#define LEOS_SX126X_SPI_PORT spi1
#endif

#ifndef LEOS_SX126X_SPI_BAUD_HZ
#define LEOS_SX126X_SPI_BAUD_HZ 4000000u
#endif

#ifndef LEOS_SX126X_PIN_SCK
#define LEOS_SX126X_PIN_SCK 10u
#endif

#ifndef LEOS_SX126X_PIN_MOSI
#define LEOS_SX126X_PIN_MOSI 11u
#endif

#ifndef LEOS_SX126X_PIN_MISO
#define LEOS_SX126X_PIN_MISO 12u
#endif

#ifndef LEOS_SX1262_PIN_NSS
#define LEOS_SX1262_PIN_NSS 13u
#endif

#ifndef LEOS_SX1262_PIN_BUSY
#define LEOS_SX1262_PIN_BUSY 14u
#endif

#ifndef LEOS_SX1262_PIN_RESET
#define LEOS_SX1262_PIN_RESET 15u
#endif

#ifndef LEOS_SX1262_PIN_DIO1
#define LEOS_SX1262_PIN_DIO1 16u
#endif

#ifndef LEOS_SX1268_PIN_NSS
#define LEOS_SX1268_PIN_NSS 17u
#endif

#ifndef LEOS_SX1268_PIN_BUSY
#define LEOS_SX1268_PIN_BUSY 18u
#endif

#ifndef LEOS_SX1268_PIN_RESET
#define LEOS_SX1268_PIN_RESET 19u
#endif

#ifndef LEOS_SX1268_PIN_DIO1
#define LEOS_SX1268_PIN_DIO1 20u
#endif

#ifndef LEOS_SX126X_ENABLE_DEBUG
#define LEOS_SX126X_ENABLE_DEBUG 0
#endif

static bool g_spi_initialized = false;

static uint8_t leos_sx126x_spi_hw_init(void)
{
    if (g_spi_initialized)
    {
        return 0;
    }

    spi_init(LEOS_SX126X_SPI_PORT, LEOS_SX126X_SPI_BAUD_HZ);
    spi_set_format(LEOS_SX126X_SPI_PORT, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(LEOS_SX126X_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LEOS_SX126X_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LEOS_SX126X_PIN_MISO, GPIO_FUNC_SPI);

    g_spi_initialized = true;
    return 0;
}

static uint8_t leos_sx126x_spi_hw_deinit(void)
{
    return 0;
}

static uint8_t leos_sx126x_gpio_init_ctx(leos_sx126x_ctx_t *ctx)
{
    gpio_init(ctx->pins.nss);
    gpio_set_dir(ctx->pins.nss, GPIO_OUT);
    gpio_put(ctx->pins.nss, 1);

    gpio_init(ctx->pins.reset);
    gpio_set_dir(ctx->pins.reset, GPIO_OUT);
    gpio_put(ctx->pins.reset, 1);

    gpio_init(ctx->pins.busy);
    gpio_set_dir(ctx->pins.busy, GPIO_IN);

    gpio_init(ctx->pins.dio1);
    gpio_set_dir(ctx->pins.dio1, GPIO_IN);

    return 0;
}

static uint8_t leos_sx126x_gpio_deinit_ctx(leos_sx126x_ctx_t *ctx)
{
    gpio_deinit(ctx->pins.nss);
    gpio_deinit(ctx->pins.reset);
    gpio_deinit(ctx->pins.busy);
    gpio_deinit(ctx->pins.dio1);
    return 0;
}

static uint8_t leos_sx126x_reset_write_ctx(leos_sx126x_ctx_t *ctx, uint8_t value)
{
    gpio_put(ctx->pins.reset, value ? 1u : 0u);
    return 0;
}

static uint8_t leos_sx126x_busy_read_ctx(leos_sx126x_ctx_t *ctx, uint8_t *value)
{
    if (value == NULL)
    {
        return 1;
    }

    *value = gpio_get(ctx->pins.busy) ? 1u : 0u;
    return 0;
}

static uint8_t leos_sx126x_spi_write_read_ctx(leos_sx126x_ctx_t *ctx,
                                              uint8_t *in_buf,
                                              uint32_t in_len,
                                              uint8_t *out_buf,
                                              uint32_t out_len)
{
    if (ctx == NULL)
    {
        return 1;
    }

    gpio_put(ctx->pins.nss, 0);

    if ((in_buf != NULL) && (in_len > 0u))
    {
        if (spi_write_blocking(LEOS_SX126X_SPI_PORT, in_buf, in_len) < 0)
        {
            gpio_put(ctx->pins.nss, 1);
            return 1;
        }
    }

    if ((out_buf != NULL) && (out_len > 0u))
    {
        if (spi_read_blocking(LEOS_SX126X_SPI_PORT, 0x00, out_buf, out_len) < 0)
        {
            gpio_put(ctx->pins.nss, 1);
            return 1;
        }
    }

    gpio_put(ctx->pins.nss, 1);
    return 0;
}

static void leos_sx126x_delay_ms(uint32_t ms)
{
    sleep_ms(ms);
}

static void leos_sx126x_debug_print(const char *const fmt, ...)
{
#if LEOS_SX126X_ENABLE_DEBUG
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#else
    (void)fmt;
#endif
}

static uint8_t leos_sx1262_spi_init(void) { return leos_sx126x_spi_hw_init(); }
static uint8_t leos_sx1262_spi_deinit(void) { return leos_sx126x_spi_hw_deinit(); }
static uint8_t leos_sx1262_spi_write_read(uint8_t *in_buf, uint32_t in_len, uint8_t *out_buf, uint32_t out_len)
{
    return leos_sx126x_spi_write_read_ctx(&g_sx1262, in_buf, in_len, out_buf, out_len);
}
static uint8_t leos_sx1262_reset_gpio_init(void) { return leos_sx126x_gpio_init_ctx(&g_sx1262); }
static uint8_t leos_sx1262_reset_gpio_deinit(void) { return leos_sx126x_gpio_deinit_ctx(&g_sx1262); }
static uint8_t leos_sx1262_reset_gpio_write(uint8_t value) { return leos_sx126x_reset_write_ctx(&g_sx1262, value); }
static uint8_t leos_sx1262_busy_gpio_init(void) { return 0; }
static uint8_t leos_sx1262_busy_gpio_deinit(void) { return 0; }
static uint8_t leos_sx1262_busy_gpio_read(uint8_t *value) { return leos_sx126x_busy_read_ctx(&g_sx1262, value); }
static void leos_sx1262_receive_callback(uint16_t type, uint8_t *buf, uint16_t len)
{
    leos_sx126x_receive_callback_impl(&g_sx1262, type, buf, len);
}

static uint8_t leos_sx1268_spi_init(void) { return leos_sx126x_spi_hw_init(); }
static uint8_t leos_sx1268_spi_deinit(void) { return leos_sx126x_spi_hw_deinit(); }
static uint8_t leos_sx1268_spi_write_read(uint8_t *in_buf, uint32_t in_len, uint8_t *out_buf, uint32_t out_len)
{
    return leos_sx126x_spi_write_read_ctx(&g_sx1268, in_buf, in_len, out_buf, out_len);
}
static uint8_t leos_sx1268_reset_gpio_init(void) { return leos_sx126x_gpio_init_ctx(&g_sx1268); }
static uint8_t leos_sx1268_reset_gpio_deinit(void) { return leos_sx126x_gpio_deinit_ctx(&g_sx1268); }
static uint8_t leos_sx1268_reset_gpio_write(uint8_t value) { return leos_sx126x_reset_write_ctx(&g_sx1268, value); }
static uint8_t leos_sx1268_busy_gpio_init(void) { return 0; }
static uint8_t leos_sx1268_busy_gpio_deinit(void) { return 0; }
static uint8_t leos_sx1268_busy_gpio_read(uint8_t *value) { return leos_sx126x_busy_read_ctx(&g_sx1268, value); }
static void leos_sx1268_receive_callback(uint16_t type, uint8_t *buf, uint16_t len)
{
    leos_sx126x_receive_callback_impl(&g_sx1268, type, buf, len);
}

void leos_sx126x_link_handle(leos_sx126x_ctx_t *ctx)
{
    DRIVER_SX1262_LINK_INIT(&ctx->handle, sx1262_handle_t);

    if (ctx->radio == LEOS_RADIO_SX1262)
    {
        ctx->pins.nss = LEOS_SX1262_PIN_NSS;
        ctx->pins.busy = LEOS_SX1262_PIN_BUSY;
        ctx->pins.reset = LEOS_SX1262_PIN_RESET;
        ctx->pins.dio1 = LEOS_SX1262_PIN_DIO1;

        DRIVER_SX1262_LINK_SPI_INIT(&ctx->handle, leos_sx1262_spi_init);
        DRIVER_SX1262_LINK_SPI_DEINIT(&ctx->handle, leos_sx1262_spi_deinit);
        DRIVER_SX1262_LINK_SPI_WRITE_READ(&ctx->handle, leos_sx1262_spi_write_read);
        DRIVER_SX1262_LINK_RESET_GPIO_INIT(&ctx->handle, leos_sx1262_reset_gpio_init);
        DRIVER_SX1262_LINK_RESET_GPIO_DEINIT(&ctx->handle, leos_sx1262_reset_gpio_deinit);
        DRIVER_SX1262_LINK_RESET_GPIO_WRITE(&ctx->handle, leos_sx1262_reset_gpio_write);
        DRIVER_SX1262_LINK_BUSY_GPIO_INIT(&ctx->handle, leos_sx1262_busy_gpio_init);
        DRIVER_SX1262_LINK_BUSY_GPIO_DEINIT(&ctx->handle, leos_sx1262_busy_gpio_deinit);
        DRIVER_SX1262_LINK_BUSY_GPIO_READ(&ctx->handle, leos_sx1262_busy_gpio_read);
        DRIVER_SX1262_LINK_DELAY_MS(&ctx->handle, leos_sx126x_delay_ms);
        DRIVER_SX1262_LINK_DEBUG_PRINT(&ctx->handle, leos_sx126x_debug_print);
        DRIVER_SX1262_LINK_RECEIVE_CALLBACK(&ctx->handle, leos_sx1262_receive_callback);
    }
    else
    {
        ctx->pins.nss = LEOS_SX1268_PIN_NSS;
        ctx->pins.busy = LEOS_SX1268_PIN_BUSY;
        ctx->pins.reset = LEOS_SX1268_PIN_RESET;
        ctx->pins.dio1 = LEOS_SX1268_PIN_DIO1;

        DRIVER_SX1262_LINK_SPI_INIT(&ctx->handle, leos_sx1268_spi_init);
        DRIVER_SX1262_LINK_SPI_DEINIT(&ctx->handle, leos_sx1268_spi_deinit);
        DRIVER_SX1262_LINK_SPI_WRITE_READ(&ctx->handle, leos_sx1268_spi_write_read);
        DRIVER_SX1262_LINK_RESET_GPIO_INIT(&ctx->handle, leos_sx1268_reset_gpio_init);
        DRIVER_SX1262_LINK_RESET_GPIO_DEINIT(&ctx->handle, leos_sx1268_reset_gpio_deinit);
        DRIVER_SX1262_LINK_RESET_GPIO_WRITE(&ctx->handle, leos_sx1268_reset_gpio_write);
        DRIVER_SX1262_LINK_BUSY_GPIO_INIT(&ctx->handle, leos_sx1268_busy_gpio_init);
        DRIVER_SX1262_LINK_BUSY_GPIO_DEINIT(&ctx->handle, leos_sx1268_busy_gpio_deinit);
        DRIVER_SX1262_LINK_BUSY_GPIO_READ(&ctx->handle, leos_sx1268_busy_gpio_read);
        DRIVER_SX1262_LINK_DELAY_MS(&ctx->handle, leos_sx126x_delay_ms);
        DRIVER_SX1262_LINK_DEBUG_PRINT(&ctx->handle, leos_sx126x_debug_print);
        DRIVER_SX1262_LINK_RECEIVE_CALLBACK(&ctx->handle, leos_sx1268_receive_callback);
    }
}