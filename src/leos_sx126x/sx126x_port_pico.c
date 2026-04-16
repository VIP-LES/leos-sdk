#include "sx126x_priv.h"

#include <stdarg.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#ifndef LEOS_SX126X_ENABLE_DEBUG
#define LEOS_SX126X_ENABLE_DEBUG 0
#endif

static bool g_spi_initialized = false;
static spi_inst_t *g_spi_port = NULL;
static uint32_t g_spi_baud_hz = 0u;
static uint32_t g_spi_pin_sck = 0u;
static uint32_t g_spi_pin_mosi = 0u;
static uint32_t g_spi_pin_miso = 0u;

static uint8_t leos_sx126x_spi_hw_init(void)
{
    if (g_spi_initialized)
    {
        return 0;
    }

    if (g_spi_port == NULL)
    {
        return 1;
    }

    spi_init(g_spi_port, g_spi_baud_hz);
    spi_set_format(g_spi_port, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(g_spi_pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(g_spi_pin_mosi, GPIO_FUNC_SPI);
    gpio_set_function(g_spi_pin_miso, GPIO_FUNC_SPI);

    g_spi_initialized = true;
    return 0;
}

static uint8_t leos_sx126x_spi_hw_deinit(void)
{
    return 0;
}

static uint8_t leos_sx126x_gpio_init_ctx(leos_sx126x_ctx_t *ctx)
{
    gpio_init(ctx->hw_config.pin_nss);
    gpio_set_dir(ctx->hw_config.pin_nss, GPIO_OUT);
    gpio_put(ctx->hw_config.pin_nss, 1);

    gpio_init(ctx->hw_config.pin_reset);
    gpio_set_dir(ctx->hw_config.pin_reset, GPIO_OUT);
    gpio_put(ctx->hw_config.pin_reset, 1);

    gpio_init(ctx->hw_config.pin_busy);
    gpio_set_dir(ctx->hw_config.pin_busy, GPIO_IN);

    gpio_init(ctx->hw_config.pin_dio1);
    gpio_set_dir(ctx->hw_config.pin_dio1, GPIO_IN);

    return 0;
}

static uint8_t leos_sx126x_gpio_deinit_ctx(leos_sx126x_ctx_t *ctx)
{
    gpio_deinit(ctx->hw_config.pin_nss);
    gpio_deinit(ctx->hw_config.pin_reset);
    gpio_deinit(ctx->hw_config.pin_busy);
    gpio_deinit(ctx->hw_config.pin_dio1);
    return 0;
}

static uint8_t leos_sx126x_reset_write_ctx(leos_sx126x_ctx_t *ctx, uint8_t value)
{
    gpio_put(ctx->hw_config.pin_reset, value ? 1u : 0u);
    return 0;
}

static uint8_t leos_sx126x_busy_read_ctx(leos_sx126x_ctx_t *ctx, uint8_t *value)
{
    if (value == NULL)
    {
        return 1;
    }

    *value = gpio_get(ctx->hw_config.pin_busy) ? 1u : 0u;
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

    gpio_put(ctx->hw_config.pin_nss, 0);

    if ((in_buf != NULL) && (in_len > 0u))
    {
        if (spi_write_blocking(g_spi_port, in_buf, in_len) < 0)
        {
            gpio_put(ctx->hw_config.pin_nss, 1);
            return 1;
        }
    }

    if ((out_buf != NULL) && (out_len > 0u))
    {
        if (spi_read_blocking(g_spi_port, 0x00, out_buf, out_len) < 0)
        {
            gpio_put(ctx->hw_config.pin_nss, 1);
            return 1;
        }
    }

    gpio_put(ctx->hw_config.pin_nss, 1);
    return 0;
}

static void leos_sx126x_delay_ms(uint32_t ms)
{
    sleep_ms(ms);
}

uint64_t leos_sx126x_platform_now_ms(void)
{
    return (uint64_t)to_ms_since_boot(get_absolute_time());
}

void leos_sx126x_platform_idle(void)
{
    tight_loop_contents();
}

uint8_t leos_sx126x_platform_read_dio1(leos_sx126x_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return 0u;
    }

    return gpio_get(ctx->hw_config.pin_dio1) ? 1u : 0u;
}

uint8_t leos_sx126x_platform_read_busy(leos_sx126x_ctx_t *ctx)
{
    uint8_t value = 0u;

    if (leos_sx126x_busy_read_ctx(ctx, &value) != 0u)
    {
        return 0u;
    }

    return value;
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
    g_spi_port = (spi_inst_t *)ctx->hw_config.platform_spi;
    g_spi_baud_hz = ctx->hw_config.spi_baud_hz;
    g_spi_pin_sck = ctx->hw_config.pin_sck;
    g_spi_pin_mosi = ctx->hw_config.pin_mosi;
    g_spi_pin_miso = ctx->hw_config.pin_miso;

    if (ctx->radio == LEOS_RADIO_SX1262)
    {
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
