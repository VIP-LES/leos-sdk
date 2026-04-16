#include "sx126x_hal.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "leos/lora.h"

#define SX126X_CMD_GET_STATUS 0xC0U
#define LEOS_LORA_RESET_PULSE_MS 2U
#define LEOS_LORA_RESET_SETTLE_MS 10U
#define LEOS_LORA_WAKEUP_SETTLE_US 200U
#define LEOS_LORA_BUSY_TIMEOUT_US 1000000U

static sx126x_hal_status_t leos_lora_hal_wait_while_busy(const leos_lora_handle_t *handle) {
    uint64_t start_us;

    start_us = time_us_64();
    while (gpio_get(handle->hw.pin_busy)) {
        if ((time_us_64() - start_us) >= LEOS_LORA_BUSY_TIMEOUT_US) {
            return SX126X_HAL_STATUS_ERROR;
        }
        tight_loop_contents();
    }

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_write(const void *context,
                                     const uint8_t *command,
                                     const uint16_t command_length,
                                     const uint8_t *data,
                                     const uint16_t data_length)
{
    const leos_lora_handle_t *handle = (const leos_lora_handle_t *)context;
    spi_inst_t *spi;
    int transferred;

    if ((handle == NULL) || (command == NULL) || (command_length == 0u)) {
        return SX126X_HAL_STATUS_ERROR;
    }

    if ((data_length > 0u) && (data == NULL)) {
        return SX126X_HAL_STATUS_ERROR;
    }

    if (leos_lora_hal_wait_while_busy(handle) != SX126X_HAL_STATUS_OK) {
        return SX126X_HAL_STATUS_ERROR;
    }

    spi = (spi_inst_t *)handle->hw.platform_spi;
    gpio_put(handle->hw.pin_nss, false);

    transferred = spi_write_blocking(spi, command, command_length);
    if (transferred != (int)command_length) {
        gpio_put(handle->hw.pin_nss, true);
        return SX126X_HAL_STATUS_ERROR;
    }

    if (data_length > 0u) {
        transferred = spi_write_blocking(spi, data, data_length);
        if (transferred != (int)data_length) {
            gpio_put(handle->hw.pin_nss, true);
            return SX126X_HAL_STATUS_ERROR;
        }
    }

    gpio_put(handle->hw.pin_nss, true);
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read(const void *context,
                                    const uint8_t *command,
                                    const uint16_t command_length,
                                    uint8_t *data,
                                    const uint16_t data_length)
{
    const leos_lora_handle_t *handle = (const leos_lora_handle_t *)context;
    spi_inst_t *spi;
    int transferred;

    if ((handle == NULL) || (command == NULL) || (command_length == 0u)) {
        return SX126X_HAL_STATUS_ERROR;
    }

    if ((data_length > 0u) && (data == NULL)) {
        return SX126X_HAL_STATUS_ERROR;
    }

    if (leos_lora_hal_wait_while_busy(handle) != SX126X_HAL_STATUS_OK) {
        return SX126X_HAL_STATUS_ERROR;
    }

    spi = (spi_inst_t *)handle->hw.platform_spi;
    gpio_put(handle->hw.pin_nss, false);

    transferred = spi_write_blocking(spi, command, command_length);
    if (transferred != (int)command_length) {
        gpio_put(handle->hw.pin_nss, true);
        return SX126X_HAL_STATUS_ERROR;
    }

    if (data_length > 0u) {
        transferred = spi_read_blocking(spi, SX126X_NOP, data, data_length);
        if (transferred != (int)data_length) {
            gpio_put(handle->hw.pin_nss, true);
            return SX126X_HAL_STATUS_ERROR;
        }
    }

    gpio_put(handle->hw.pin_nss, true);
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_reset(const void *context) {
    const leos_lora_handle_t *handle = (const leos_lora_handle_t *)context;

    gpio_put(handle->hw.pin_reset, false);
    sleep_ms(LEOS_LORA_RESET_PULSE_MS);
    gpio_put(handle->hw.pin_reset, true);
    sleep_ms(LEOS_LORA_RESET_SETTLE_MS);

    return leos_lora_hal_wait_while_busy(handle);
}

sx126x_hal_status_t sx126x_hal_wakeup(const void *context) {
    const leos_lora_handle_t *handle = (const leos_lora_handle_t *)context;
    spi_inst_t *spi;
    uint8_t wakeup_cmd[2] = {SX126X_CMD_GET_STATUS, SX126X_NOP};
    int transferred;

    spi = (spi_inst_t *)handle->hw.platform_spi;
    gpio_put(handle->hw.pin_nss, false);

    transferred = spi_write_blocking(spi, wakeup_cmd, sizeof(wakeup_cmd));
    gpio_put(handle->hw.pin_nss, true);
    if (transferred != (int)sizeof(wakeup_cmd)) {
        return SX126X_HAL_STATUS_ERROR;
    }

    sleep_us(LEOS_LORA_WAKEUP_SETTLE_US);
    return leos_lora_hal_wait_while_busy(handle);
}
