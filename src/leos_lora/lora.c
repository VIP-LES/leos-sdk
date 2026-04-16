#include "leos/lora.h"

#include <stdbool.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "sx126x.h"

#define LEOS_LORA_MIN_TX_POWER_DBM (-9)
#define LEOS_LORA_MAX_TX_POWER_DBM 22
#define LEOS_LORA_GPIO_LOOKUP_SIZE 48u

static leos_lora_handle_t *s_handle_by_gpio[LEOS_LORA_GPIO_LOOKUP_SIZE] = {0};

static leos_lora_result_t leos_lora_complete_tx(leos_lora_handle_t *handle,
                                                 leos_lora_result_t result)
{
    leos_lora_tx_callback_t callback;
    void *user_reference;

    callback = handle->callbacks.tx_done;
    user_reference = handle->callbacks_user_reference;

    handle->tx_in_progress = false;
    handle->tx_deadline_us = 0u;
    handle->tx_payload_len = 0u;

    if (callback != NULL) {
        callback(handle, result, user_reference);
    }

    return result;
}

static leos_lora_result_t leos_lora_set_packet_payload_len(leos_lora_handle_t *handle,
                                                            uint8_t payload_len)
{
    leos_lora_result_t status;
    sx126x_pkt_params_lora_t pkt_params;

    memset(&pkt_params, 0, sizeof(pkt_params));
    pkt_params.preamble_len_in_symb = handle->config.preamble_symbols;
    pkt_params.header_type = SX126X_LORA_PKT_EXPLICIT;
    pkt_params.pld_len_in_bytes = payload_len;
    pkt_params.crc_is_on = handle->config.crc_enabled;
    pkt_params.invert_iq_is_on = handle->config.invert_iq;

    status = (leos_lora_result_t)sx126x_set_standby(handle, SX126X_STANDBY_CFG_XOSC);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_lora_pkt_params(handle, &pkt_params);
    return status;
}

static leos_lora_result_t leos_lora_apply_config(leos_lora_handle_t *handle) {
    leos_lora_result_t status;
    sx126x_mod_params_lora_t mod_params;
    sx126x_pkt_params_lora_t pkt_params;
    sx126x_pa_cfg_params_t pa_cfg;

    memset(&mod_params, 0, sizeof(mod_params));
    memset(&pkt_params, 0, sizeof(pkt_params));

    mod_params.sf = handle->config.spreading_factor;
    mod_params.bw = handle->config.bandwidth;
    mod_params.cr = handle->config.coding_rate;
    mod_params.ldro = handle->config.low_data_rate_optimize ? 0x01u : 0x00u;

    pkt_params.preamble_len_in_symb = handle->config.preamble_symbols;
    pkt_params.header_type = SX126X_LORA_PKT_EXPLICIT;
    pkt_params.pld_len_in_bytes = 0xFFu;
    pkt_params.crc_is_on = handle->config.crc_enabled;
    pkt_params.invert_iq_is_on = handle->config.invert_iq;

    pa_cfg.pa_duty_cycle = 0x02u;
    pa_cfg.hp_max = 0x03u;
    pa_cfg.device_sel = 0x00u;
    pa_cfg.pa_lut = 0x01u;

    status = (leos_lora_result_t)sx126x_set_standby(handle, SX126X_STANDBY_CFG_RC);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_dio2_as_rf_sw_ctrl(handle, true);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_dio3_as_tcxo_ctrl(
        handle,
        LEOS_LORA_DEFAULT_TCXO_VOLTAGE,
        sx126x_convert_timeout_in_ms_to_rtc_step(LEOS_LORA_DEFAULT_TCXO_STARTUP_TIME_MS));
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_reg_mode(handle, SX126X_REG_MODE_DCDC);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_pkt_type(handle, SX126X_PKT_TYPE_LORA);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_rf_freq(handle, handle->config.frequency_hz);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_pa_cfg(handle, &pa_cfg);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_tx_params(handle, handle->config.tx_power_dbm, SX126X_RAMP_10_US);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_lora_mod_params(handle, &mod_params);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_lora_pkt_params(handle, &pkt_params);
    if (status != LEOS_LORA_OK)
    {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_lora_sync_word(handle, handle->config.sync_word);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_buffer_base_address(handle, 0x00u, 0x00u);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)
        sx126x_set_dio_irq_params(handle,
                                  (sx126x_irq_mask_t)(SX126X_IRQ_TX_DONE | SX126X_IRQ_RX_DONE |
                                                      SX126X_IRQ_TIMEOUT | SX126X_IRQ_CRC_ERROR),
                                  (sx126x_irq_mask_t)(SX126X_IRQ_TX_DONE | SX126X_IRQ_RX_DONE |
                                                      SX126X_IRQ_TIMEOUT | SX126X_IRQ_CRC_ERROR),
                                  SX126X_IRQ_NONE,
                                  SX126X_IRQ_NONE);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_standby(handle, SX126X_STANDBY_CFG_XOSC);
    return status;
}

void leos_lora_get_default_config(leos_lora_config_t *config) {
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->frequency_hz = LEOS_LORA_DEFAULT_FREQUENCY_HZ;
    config->tx_power_dbm = LEOS_LORA_DEFAULT_TX_POWER_DBM;
    config->preamble_symbols = LEOS_LORA_DEFAULT_PREAMBLE_SYMBOLS;
    config->bandwidth = SX126X_LORA_BW_125;
    config->spreading_factor = SX126X_LORA_SF9;
    config->coding_rate = SX126X_LORA_CR_4_5;
    config->sync_word = LEOS_LORA_DEFAULT_SYNC_WORD;
    config->crc_enabled = true;
    config->invert_iq = false;
    config->low_data_rate_optimize = false;
}

void leos_lora_set_callbacks(leos_lora_handle_t *handle,
                             const leos_lora_callbacks_t *callbacks,
                             void *user_reference)
{
    if (handle == NULL) {
        return;
    }

    if (callbacks == NULL) {
        memset(&handle->callbacks, 0, sizeof(handle->callbacks));
        handle->callbacks_user_reference = NULL;
        return;
    }

    handle->callbacks = *callbacks;
    handle->callbacks_user_reference = user_reference;
}

leos_lora_result_t leos_lora_transmit(leos_lora_handle_t *handle,
                                      const uint8_t *payload,
                                      uint8_t payload_len)
{
    leos_lora_result_t status;

    if ((handle == NULL) || (payload == NULL) || (payload_len == 0u)) {
        return LEOS_LORA_ERR_ARG;
    }

    if (payload_len > LEOS_LORA_MAX_PAYLOAD_LEN) {
        return LEOS_LORA_ERR_ARG;
    }

    if (!handle->initialized) {
        return LEOS_LORA_ERR_STATE;
    }

    if (handle->tx_in_progress) {
        return LEOS_LORA_ERR_STATE;
    }

    if (handle->rx_in_progress) {
        handle->rx_in_progress = false;
    }

    memcpy(handle->tx_payload, payload, payload_len);
    handle->tx_payload_len = payload_len;

    status = leos_lora_set_packet_payload_len(handle, payload_len);
    if (status != LEOS_LORA_OK) {
        handle->tx_payload_len = 0u;
        return status;
    }

    status = (leos_lora_result_t)sx126x_clear_irq_status(handle, SX126X_IRQ_ALL);
    if (status != LEOS_LORA_OK) {
        handle->tx_payload_len = 0u;
        return status;
    }

    status = (leos_lora_result_t)sx126x_write_buffer(handle, 0u, handle->tx_payload, payload_len);
    if (status != LEOS_LORA_OK) {
        handle->tx_payload_len = 0u;
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_tx(handle, LEOS_LORA_DEFAULT_TX_TIMEOUT_MS);
    if (status != LEOS_LORA_OK) {
        handle->tx_payload_len = 0u;
        return status;
    }

    handle->irq_pending = false;
    handle->irq_events_mask = 0u;
    handle->tx_in_progress = true;
    handle->tx_deadline_us = time_us_64() + ((uint64_t)LEOS_LORA_DEFAULT_TX_TIMEOUT_MS * 1000ULL);
    return LEOS_LORA_OK;
}

leos_lora_result_t leos_lora_start_rx(leos_lora_handle_t *handle)
{
    leos_lora_result_t status;

    if (handle == NULL) {
        return LEOS_LORA_ERR_ARG;
    }

    if (!handle->initialized) {
        return LEOS_LORA_ERR_STATE;
    }

    if (handle->tx_in_progress) {
        return LEOS_LORA_ERR_STATE;
    }

    status = leos_lora_set_packet_payload_len(handle, LEOS_LORA_MAX_PAYLOAD_LEN);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_clear_irq_status(handle, SX126X_IRQ_ALL);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    status = (leos_lora_result_t)sx126x_set_rx_with_timeout_in_rtc_step(handle, SX126X_RX_CONTINUOUS);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    handle->irq_pending = false;
    handle->irq_events_mask = 0u;
    handle->rx_in_progress = true;
    return LEOS_LORA_OK;
}

leos_lora_result_t leos_lora_stop_rx(leos_lora_handle_t *handle)
{
    leos_lora_result_t status;

    if (handle == NULL) {
        return LEOS_LORA_ERR_ARG;
    }

    if (!handle->initialized) {
        return LEOS_LORA_ERR_STATE;
    }

    status = (leos_lora_result_t)sx126x_set_standby(handle, SX126X_STANDBY_CFG_XOSC);
    if (status != LEOS_LORA_OK) {
        return status;
    }

    handle->rx_in_progress = false;
    return LEOS_LORA_OK;
}

leos_lora_result_t leos_lora_task(leos_lora_handle_t *handle)
{
    leos_lora_result_t status;
    sx126x_irq_mask_t irq = SX126X_IRQ_NONE;
    uint64_t now_us;

    if (handle == NULL) {
        return LEOS_LORA_ERR_ARG;
    }

    if (!handle->initialized) {
        return LEOS_LORA_ERR_STATE;
    }

    if (!handle->tx_in_progress && !handle->irq_pending) {
        return LEOS_LORA_OK;
    }

    now_us = time_us_64();
    if (!handle->irq_pending && handle->tx_in_progress && (now_us < handle->tx_deadline_us)) {
        return LEOS_LORA_OK;
    }

    handle->irq_pending = false;
    handle->irq_events_mask = 0u;

    status = (leos_lora_result_t)sx126x_get_and_clear_irq_status(handle, &irq);
    if (status != LEOS_LORA_OK) {
        if (handle->tx_in_progress) {
            return leos_lora_complete_tx(handle, status);
        }
        return status;
    }

    if ((irq & SX126X_IRQ_RX_DONE) != 0u) {
        sx126x_rx_buffer_status_t rx_buffer_status;
        sx126x_pkt_status_lora_t pkt_status;
        uint8_t rx_payload[LEOS_LORA_MAX_PAYLOAD_LEN];
        uint8_t rx_len;

        status = (leos_lora_result_t)sx126x_get_rx_buffer_status(handle, &rx_buffer_status);
        if (status != LEOS_LORA_OK) {
            if (handle->tx_in_progress) {
                return leos_lora_complete_tx(handle, status);
            }
            return status;
        }

        rx_len = rx_buffer_status.pld_len_in_bytes;
        if (rx_len > LEOS_LORA_MAX_PAYLOAD_LEN) {
            rx_len = LEOS_LORA_MAX_PAYLOAD_LEN;
        }

        status = (leos_lora_result_t)sx126x_read_buffer(handle,
                                                        rx_buffer_status.buffer_start_pointer,
                                                        rx_payload,
                                                        rx_len);
        if (status != LEOS_LORA_OK) {
            if (handle->tx_in_progress) {
                return leos_lora_complete_tx(handle, status);
            }
            return status;
        }

        memset(&pkt_status, 0, sizeof(pkt_status));
        status = (leos_lora_result_t)sx126x_get_lora_pkt_status(handle, &pkt_status);
        if (status != LEOS_LORA_OK) {
            if (handle->tx_in_progress) {
                return leos_lora_complete_tx(handle, status);
            }
            return status;
        }

        if (handle->callbacks.rx_done != NULL) {
            handle->callbacks.rx_done(handle,
                                      rx_payload,
                                      rx_len,
                                      &pkt_status,
                                      handle->callbacks_user_reference);
        }
    }

    if ((irq & SX126X_IRQ_TX_DONE) != 0u) {
        if (handle->tx_in_progress) {
            return leos_lora_complete_tx(handle, LEOS_LORA_OK);
        }
        return LEOS_LORA_OK;
    }

    if ((irq & SX126X_IRQ_TIMEOUT) != 0u) {
        if (handle->rx_in_progress && !handle->tx_in_progress) {
            handle->rx_in_progress = false;
            return LEOS_LORA_OK;
        }

        if (!handle->tx_in_progress) {
            return LEOS_LORA_OK;
        }
        return leos_lora_complete_tx(handle, LEOS_LORA_ERR_STATE);
    }

    if (handle->tx_in_progress && (now_us >= handle->tx_deadline_us)) {
        (void)sx126x_set_standby(handle, SX126X_STANDBY_CFG_XOSC);
        return leos_lora_complete_tx(handle, LEOS_LORA_ERR_STATE);
    }

    return LEOS_LORA_OK;
}

leos_lora_result_t leos_lora_process(leos_lora_handle_t *handle)
{
    return leos_lora_task(handle);
}

bool leos_lora_tx_busy(const leos_lora_handle_t *handle)
{
    if ((handle == NULL) || !handle->initialized) {
        return false;
    }

    return handle->tx_in_progress;
}

leos_lora_result_t leos_lora_init(leos_lora_handle_t *handle,
                                  const leos_lora_hw_t *hw,
                                  const leos_lora_config_t *config)
{
    spi_inst_t *spi;

    if ((handle == NULL) || (hw == NULL) || (config == NULL)) {
        return LEOS_LORA_ERR_ARG;
    }

    if (hw->platform_spi == NULL) {
        return LEOS_LORA_ERR_ARG;
    }

    if ((config->frequency_hz == 0u) || (config->preamble_symbols == 0u)) {
        return LEOS_LORA_ERR_ARG;
    }

    if ((config->tx_power_dbm < LEOS_LORA_MIN_TX_POWER_DBM) ||
        (config->tx_power_dbm > LEOS_LORA_MAX_TX_POWER_DBM))
    {
        return LEOS_LORA_ERR_ARG;
    }

    if (hw->pin_dio1 >= LEOS_LORA_GPIO_LOOKUP_SIZE) {
        return LEOS_LORA_ERR_ARG;
    }

    memset(handle, 0, sizeof(*handle));
    handle->hw = *hw;
    handle->config = *config;

    if (handle->hw.spi_baudrate_hz == 0u) {
        handle->hw.spi_baudrate_hz = LEOS_LORA_DEFAULT_SPI_BAUDRATE_HZ;
    }

    spi = (spi_inst_t*) handle->hw.platform_spi;
    if (spi == NULL) {
        return LEOS_LORA_ERR_ARG;
    }

    gpio_set_function(handle->hw.pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(handle->hw.pin_mosi, GPIO_FUNC_SPI);
    gpio_set_function(handle->hw.pin_miso, GPIO_FUNC_SPI);

    spi_init(spi, handle->hw.spi_baudrate_hz);
    spi_set_format(spi, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_init(handle->hw.pin_nss);
    gpio_set_dir(handle->hw.pin_nss, GPIO_OUT);
    gpio_put(handle->hw.pin_nss, true);

    gpio_init(handle->hw.pin_busy);
    gpio_set_dir(handle->hw.pin_busy, GPIO_IN);

    gpio_init(handle->hw.pin_dio1);
    gpio_set_dir(handle->hw.pin_dio1, GPIO_IN);

    gpio_init(handle->hw.pin_reset);
    gpio_set_dir(handle->hw.pin_reset, GPIO_OUT);
    gpio_put(handle->hw.pin_reset, true);

    sleep_ms(10u);

    if ((leos_lora_result_t)sx126x_reset(handle) != LEOS_LORA_OK) {
        return LEOS_LORA_ERR_HAL;
    }

    if ((leos_lora_result_t)sx126x_wakeup(handle) != LEOS_LORA_OK) {
        return LEOS_LORA_ERR_HAL;
    }

    {
        leos_lora_result_t status = leos_lora_apply_config(handle);
        if (status != LEOS_LORA_OK)
        {
            return status;
        }
    }

    handle->initialized = true;
    handle->irq_pending = false;
    handle->irq_events_mask = 0u;
    handle->tx_in_progress = false;
    handle->rx_in_progress = false;
    handle->tx_deadline_us = 0u;
    memset(&handle->callbacks, 0, sizeof(handle->callbacks));
    handle->callbacks_user_reference = NULL;
    handle->tx_payload_len = 0u;
    s_handle_by_gpio[handle->hw.pin_dio1] = handle;
    gpio_set_irq_enabled(handle->hw.pin_dio1, GPIO_IRQ_EDGE_RISE, true);
    return LEOS_LORA_OK;
}

void leos_lora_irq_handler(unsigned int gpio, uint32_t events_mask) {
    leos_lora_handle_t *handle;

    if ((events_mask & GPIO_IRQ_EDGE_RISE) == 0u) {
        return;
    }

    if (gpio >= LEOS_LORA_GPIO_LOOKUP_SIZE) {
        return;
    }

    handle = s_handle_by_gpio[gpio];
    if ((handle == NULL) || !handle->initialized) {
        return;
    }

    handle->irq_pending = true;
    handle->irq_events_mask = events_mask;
}