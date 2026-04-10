#define _DEFAULT_SOURCE

#include "sx126x_priv.h"

#include <errno.h>
#include <fcntl.h>
#include <gpiod.h>
#include <linux/spi/spidev.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#ifndef LEOS_SX126X_ENABLE_DEBUG
#define LEOS_SX126X_ENABLE_DEBUG 0
#endif

#ifndef LEOS_SX126X_LINUX_GPIO_CHIP
#define LEOS_SX126X_LINUX_GPIO_CHIP "/dev/gpiochip0"
#endif

typedef struct
{
    struct gpiod_line_request *nss_req;
    struct gpiod_line_request *busy_req;
    struct gpiod_line_request *reset_req;
    unsigned int nss_offset;
    unsigned int busy_offset;
    unsigned int reset_offset;
    bool requested;
} leos_sx126x_linux_gpio_t;

static struct gpiod_chip *g_gpio_chip = NULL;
static leos_sx126x_linux_gpio_t g_sx1262_gpio = {0};
static leos_sx126x_linux_gpio_t g_sx1268_gpio = {0};

static leos_sx126x_linux_gpio_t *leos_sx126x_linux_gpio(leos_sx126x_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    return (ctx->radio == LEOS_RADIO_SX1262) ? &g_sx1262_gpio : &g_sx1268_gpio;
}

static int leos_sx126x_linux_spi_fd(leos_sx126x_ctx_t *ctx)
{
    int *fd_ptr;

    if ((ctx == NULL) || (ctx->hw_config.platform_spi == NULL))
    {
        return -1;
    }

    fd_ptr = (int *)ctx->hw_config.platform_spi;
    return *fd_ptr;
}

static uint8_t leos_sx126x_gpio_chip_open(void)
{
    if (g_gpio_chip != NULL)
    {
        return 0;
    }

    g_gpio_chip = gpiod_chip_open(LEOS_SX126X_LINUX_GPIO_CHIP);
    return (g_gpio_chip != NULL) ? 0u : 1u;
}

static uint8_t leos_sx126x_spi_hw_init(leos_sx126x_ctx_t *ctx)
{
    uint8_t mode = SPI_MODE_0 | SPI_NO_CS;
    uint8_t bits_per_word = 8u;
    uint32_t speed_hz;
    int fd;

    if (ctx == NULL)
    {
        return 1;
    }

    fd = leos_sx126x_linux_spi_fd(ctx);
    if (fd < 0)
    {
        return 1;
    }

    speed_hz = ctx->hw_config.spi_baud_hz;

    if ((ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) ||
        (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0) ||
        (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0))
    {
        return 1;
    }

    return 0;
}

static uint8_t leos_sx126x_spi_hw_deinit(void)
{
    return 0;
}

static uint8_t leos_sx126x_gpio_init_ctx(leos_sx126x_ctx_t *ctx)
{
    leos_sx126x_linux_gpio_t *gpio;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;

    if ((ctx == NULL) || (leos_sx126x_gpio_chip_open() != 0u))
    {
        return 1;
    }

    gpio = leos_sx126x_linux_gpio(ctx);
    if (gpio == NULL)
    {
        return 1;
    }

    if (gpio->requested)
    {
        return 0;
    }

    gpio->nss_offset = (unsigned int)ctx->hw_config.pin_nss;
    gpio->busy_offset = (unsigned int)ctx->hw_config.pin_busy;
    gpio->reset_offset = (unsigned int)ctx->hw_config.pin_reset;

    /* --- NSS: output, default high --- */
    settings = gpiod_line_settings_new();
    if (settings == NULL)
    {
        return 1;
    }
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_ACTIVE);

    line_cfg = gpiod_line_config_new();
    if (line_cfg == NULL)
    {
        gpiod_line_settings_free(settings);
        return 1;
    }
    gpiod_line_config_add_line_settings(line_cfg, &gpio->nss_offset, 1, settings);
    gpiod_line_settings_free(settings);

    req_cfg = gpiod_request_config_new();
    if (req_cfg == NULL)
    {
        gpiod_line_config_free(line_cfg);
        return 1;
    }
    gpiod_request_config_set_consumer(req_cfg, "leos_sx126x_nss");

    gpio->nss_req = gpiod_chip_request_lines(g_gpio_chip, req_cfg, line_cfg);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    if (gpio->nss_req == NULL)
    {
        return 1;
    }

    /* --- RESET: output, default high --- */
    settings = gpiod_line_settings_new();
    if (settings == NULL)
    {
        return 1;
    }
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_ACTIVE);

    line_cfg = gpiod_line_config_new();
    if (line_cfg == NULL)
    {
        gpiod_line_settings_free(settings);
        return 1;
    }
    gpiod_line_config_add_line_settings(line_cfg, &gpio->reset_offset, 1, settings);
    gpiod_line_settings_free(settings);

    req_cfg = gpiod_request_config_new();
    if (req_cfg == NULL)
    {
        gpiod_line_config_free(line_cfg);
        return 1;
    }
    gpiod_request_config_set_consumer(req_cfg, "leos_sx126x_reset");

    gpio->reset_req = gpiod_chip_request_lines(g_gpio_chip, req_cfg, line_cfg);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    if (gpio->reset_req == NULL)
    {
        gpiod_line_request_release(gpio->nss_req);
        gpio->nss_req = NULL;
        return 1;
    }

    /* --- BUSY: input --- */
    settings = gpiod_line_settings_new();
    if (settings == NULL)
    {
        return 1;
    }
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    line_cfg = gpiod_line_config_new();
    if (line_cfg == NULL)
    {
        gpiod_line_settings_free(settings);
        return 1;
    }
    gpiod_line_config_add_line_settings(line_cfg, &gpio->busy_offset, 1, settings);
    gpiod_line_settings_free(settings);

    req_cfg = gpiod_request_config_new();
    if (req_cfg == NULL)
    {
        gpiod_line_config_free(line_cfg);
        return 1;
    }
    gpiod_request_config_set_consumer(req_cfg, "leos_sx126x_busy");

    gpio->busy_req = gpiod_chip_request_lines(g_gpio_chip, req_cfg, line_cfg);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    if (gpio->busy_req == NULL)
    {
        gpiod_line_request_release(gpio->reset_req);
        gpiod_line_request_release(gpio->nss_req);
        gpio->reset_req = NULL;
        gpio->nss_req = NULL;
        return 1;
    }

    gpio->requested = true;
    return 0;
}

static uint8_t leos_sx126x_gpio_deinit_ctx(leos_sx126x_ctx_t *ctx)
{
    leos_sx126x_linux_gpio_t *gpio = leos_sx126x_linux_gpio(ctx);

    if (gpio == NULL)
    {
        return 1;
    }

    if (gpio->busy_req != NULL)
    {
        gpiod_line_request_release(gpio->busy_req);
        gpio->busy_req = NULL;
    }
    if (gpio->reset_req != NULL)
    {
        gpiod_line_request_release(gpio->reset_req);
        gpio->reset_req = NULL;
    }
    if (gpio->nss_req != NULL)
    {
        gpiod_line_request_release(gpio->nss_req);
        gpio->nss_req = NULL;
    }
    gpio->requested = false;
    return 0;
}

static uint8_t leos_sx126x_reset_write_ctx(leos_sx126x_ctx_t *ctx, uint8_t value)
{
    leos_sx126x_linux_gpio_t *gpio = leos_sx126x_linux_gpio(ctx);
    enum gpiod_line_value v;

    if ((gpio == NULL) || (gpio->reset_req == NULL))
    {
        return 1;
    }

    v = value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    return (gpiod_line_request_set_value(gpio->reset_req, gpio->reset_offset, v) == 0) ? 0u : 1u;
}

static uint8_t leos_sx126x_busy_read_ctx(leos_sx126x_ctx_t *ctx, uint8_t *value)
{
    enum gpiod_line_value rc;
    leos_sx126x_linux_gpio_t *gpio = leos_sx126x_linux_gpio(ctx);

    if ((gpio == NULL) || (gpio->busy_req == NULL) || (value == NULL))
    {
        return 1;
    }

    rc = gpiod_line_request_get_value(gpio->busy_req, gpio->busy_offset);
    if (rc < 0)
    {
        return 1;
    }

    *value = (rc == GPIOD_LINE_VALUE_ACTIVE) ? 1u : 0u;
    return 0;
}

static uint8_t leos_sx126x_spi_write_read_ctx(leos_sx126x_ctx_t *ctx,
                                              uint8_t *in_buf,
                                              uint32_t in_len,
                                              uint8_t *out_buf,
                                              uint32_t out_len)
{
    struct spi_ioc_transfer transfers[2];
    unsigned int transfer_count = 0u;
    int fd;
    uint32_t speed_hz;
    leos_sx126x_linux_gpio_t *gpio = leos_sx126x_linux_gpio(ctx);

    if ((ctx == NULL) || (gpio == NULL) || (gpio->nss_req == NULL))
    {
        return 1;
    }

    fd = leos_sx126x_linux_spi_fd(ctx);
    speed_hz = ctx->hw_config.spi_baud_hz;
    if (fd < 0)
    {
        return 1;
    }

    memset(transfers, 0, sizeof(transfers));

    if ((in_buf != NULL) && (in_len > 0u))
    {
        transfers[transfer_count].tx_buf = (uintptr_t)in_buf;
        transfers[transfer_count].len = in_len;
        transfers[transfer_count].speed_hz = speed_hz;
        transfers[transfer_count].bits_per_word = 8u;
        transfer_count++;
    }

    if ((out_buf != NULL) && (out_len > 0u))
    {
        transfers[transfer_count].rx_buf = (uintptr_t)out_buf;
        transfers[transfer_count].len = out_len;
        transfers[transfer_count].speed_hz = speed_hz;
        transfers[transfer_count].bits_per_word = 8u;
        transfer_count++;
    }

    if (transfer_count == 0u)
    {
        return 0;
    }

    if (gpiod_line_request_set_value(gpio->nss_req, gpio->nss_offset, GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        return 1;
    }

    if (ioctl(fd, SPI_IOC_MESSAGE(transfer_count), transfers) < 0)
    {
        (void)gpiod_line_request_set_value(gpio->nss_req, gpio->nss_offset, GPIOD_LINE_VALUE_ACTIVE);
        return 1;
    }

    if (gpiod_line_request_set_value(gpio->nss_req, gpio->nss_offset, GPIOD_LINE_VALUE_ACTIVE) < 0)
    {
        return 1;
    }

    return 0;
}

static void leos_sx126x_delay_ms(uint32_t ms)
{
    (void)usleep((useconds_t)(ms * 1000u));
}

uint64_t leos_sx126x_platform_now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0u;
    }

    return ((uint64_t)ts.tv_sec * 1000u) + ((uint64_t)ts.tv_nsec / 1000000u);
}

void leos_sx126x_platform_idle(void)
{
    (void)usleep(1000u);
}

static void leos_sx126x_debug_print(const char *const fmt, ...)
{
#if LEOS_SX126X_ENABLE_DEBUG
    va_list args;
    va_start(args, fmt);
    (void)vfprintf(stderr, fmt, args);
    va_end(args);
#else
    (void)fmt;
#endif
}

static uint8_t leos_sx1262_spi_init(void) { return leos_sx126x_spi_hw_init(&g_sx1262); }
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

static uint8_t leos_sx1268_spi_init(void) { return leos_sx126x_spi_hw_init(&g_sx1268); }
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
    if (ctx == NULL)
    {
        return;
    }

    DRIVER_SX1262_LINK_INIT(&ctx->handle, sx1262_handle_t);

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
