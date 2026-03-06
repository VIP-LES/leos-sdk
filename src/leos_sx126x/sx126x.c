#include "sx126x_priv.h"

#include <string.h>

#include "pico/stdlib.h"

leos_sx126x_ctx_t g_sx1262 = {
    .radio = LEOS_RADIO_SX1262,
    .mode = LEOS_RADIO_MODE_UNINIT,
};

leos_sx126x_ctx_t g_sx1268 = {
    .radio = LEOS_RADIO_SX1268,
    .mode = LEOS_RADIO_MODE_UNINIT,
};

leos_sx126x_ctx_t *leos_sx126x_ctx(leos_radio_t radio)
{
    switch (radio)
    {
        case LEOS_RADIO_SX1262: return &g_sx1262;
        case LEOS_RADIO_SX1268: return &g_sx1268;
        default:                return NULL;
    }
}

static bool leos_sx126x_to_vendor_bw(leos_radio_bandwidth_t bw, sx1262_lora_bandwidth_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    switch (bw)
    {
        case LEOS_RADIO_BW_7_KHZ:   *out = SX1262_LORA_BANDWIDTH_7P81_KHZ;  return true;
        case LEOS_RADIO_BW_10_KHZ:  *out = SX1262_LORA_BANDWIDTH_10P42_KHZ; return true;
        case LEOS_RADIO_BW_15_KHZ:  *out = SX1262_LORA_BANDWIDTH_15P63_KHZ; return true;
        case LEOS_RADIO_BW_20_KHZ:  *out = SX1262_LORA_BANDWIDTH_20P83_KHZ; return true;
        case LEOS_RADIO_BW_31_KHZ:  *out = SX1262_LORA_BANDWIDTH_31P25_KHZ; return true;
        case LEOS_RADIO_BW_41_KHZ:  *out = SX1262_LORA_BANDWIDTH_41P67_KHZ; return true;
        case LEOS_RADIO_BW_62_KHZ:  *out = SX1262_LORA_BANDWIDTH_62P50_KHZ; return true;
        case LEOS_RADIO_BW_125_KHZ: *out = SX1262_LORA_BANDWIDTH_125_KHZ;   return true;
        case LEOS_RADIO_BW_250_KHZ: *out = SX1262_LORA_BANDWIDTH_250_KHZ;   return true;
        case LEOS_RADIO_BW_500_KHZ: *out = SX1262_LORA_BANDWIDTH_500_KHZ;   return true;
        default: return false;
    }
}

static bool leos_sx126x_to_vendor_sf(leos_radio_spreading_factor_t sf, sx1262_lora_sf_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    switch (sf)
    {
        case LEOS_RADIO_SF_5:  *out = SX1262_LORA_SF_5;  return true;
        case LEOS_RADIO_SF_6:  *out = SX1262_LORA_SF_6;  return true;
        case LEOS_RADIO_SF_7:  *out = SX1262_LORA_SF_7;  return true;
        case LEOS_RADIO_SF_8:  *out = SX1262_LORA_SF_8;  return true;
        case LEOS_RADIO_SF_9:  *out = SX1262_LORA_SF_9;  return true;
        case LEOS_RADIO_SF_10: *out = SX1262_LORA_SF_10; return true;
        case LEOS_RADIO_SF_11: *out = SX1262_LORA_SF_11; return true;
        case LEOS_RADIO_SF_12: *out = SX1262_LORA_SF_12; return true;
        default: return false;
    }
}

static bool leos_sx126x_to_vendor_cr(leos_radio_coding_rate_t cr, sx1262_lora_cr_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    switch (cr)
    {
        case LEOS_RADIO_CR_4_5: *out = SX1262_LORA_CR_4_5; return true;
        case LEOS_RADIO_CR_4_6: *out = SX1262_LORA_CR_4_6; return true;
        case LEOS_RADIO_CR_4_7: *out = SX1262_LORA_CR_4_7; return true;
        case LEOS_RADIO_CR_4_8: *out = SX1262_LORA_CR_4_8; return true;
        default: return false;
    }
}

static bool leos_sx126x_low_data_rate_optimize(const leos_radio_config_t *cfg)
{
    if (cfg == NULL)
    {
        return false;
    }

    switch (cfg->bandwidth)
    {
        case LEOS_RADIO_BW_125_KHZ:
            return (cfg->spreading_factor == LEOS_RADIO_SF_11 ||
                    cfg->spreading_factor == LEOS_RADIO_SF_12);

        case LEOS_RADIO_BW_250_KHZ:
            return (cfg->spreading_factor == LEOS_RADIO_SF_12);

        default:
            return false;
    }
}

leos_radio_status_t leos_sx126x_map_status(uint8_t rc)
{
    switch (rc)
    {
        case 0: return LEOS_RADIO_OK;
        case 4: return LEOS_RADIO_ERR_BUSY;
        case 5:
        case 6: return LEOS_RADIO_ERR_TIMEOUT;
        default: return LEOS_RADIO_ERR_DRIVER;
    }
}

static leos_radio_status_t leos_sx126x_apply_packet_params(leos_sx126x_ctx_t *ctx, uint8_t payload_len)
{
    sx1262_lora_crc_type_t crc_type =
        ctx->config.crc_enabled ? SX1262_LORA_CRC_TYPE_ON : SX1262_LORA_CRC_TYPE_OFF;

    return leos_sx126x_map_status(
        sx1262_set_lora_packet_params(&ctx->handle,
                                      ctx->config.preamble_len_symbols,
                                      SX1262_LORA_HEADER_EXPLICIT,
                                      payload_len,
                                      crc_type,
                                      ctx->config.iq_inverted ? SX1262_BOOL_TRUE : SX1262_BOOL_FALSE));
}

static leos_radio_status_t leos_sx126x_apply_config(leos_sx126x_ctx_t *ctx)
{
    sx1262_lora_sf_t sf;
    sx1262_lora_bandwidth_t bw;
    sx1262_lora_cr_t cr;
    uint32_t freq_reg;
    uint8_t modulation;
    uint8_t clamp;
    leos_radio_status_t status;

    if ((ctx == NULL) ||
        !leos_sx126x_to_vendor_sf(ctx->config.spreading_factor, &sf) ||
        !leos_sx126x_to_vendor_bw(ctx->config.bandwidth, &bw) ||
        !leos_sx126x_to_vendor_cr(ctx->config.coding_rate, &cr))
    {
        return LEOS_RADIO_ERR_ARG;
    }

    status = leos_sx126x_map_status(sx1262_set_standby(&ctx->handle, SX1262_CLOCK_SOURCE_XTAL_32MHZ));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_stop_timer_on_preamble(&ctx->handle, SX1262_BOOL_FALSE));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_regulator_mode(&ctx->handle, SX1262_REGULATOR_MODE_DC_DC_LDO));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_pa_config(&ctx->handle, LEOS_SX126X_PA_DUTY_CYCLE, LEOS_SX126X_PA_HP_MAX));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_rx_tx_fallback_mode(&ctx->handle, SX1262_RX_TX_FALLBACK_MODE_STDBY_XOSC));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_packet_type(&ctx->handle, SX1262_PACKET_TYPE_LORA));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_tx_params(&ctx->handle, ctx->config.tx_power_dbm, SX1262_RAMP_TIME_10US));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(
        sx1262_set_lora_modulation_params(&ctx->handle,
                                          sf,
                                          bw,
                                          cr,
                                          leos_sx126x_low_data_rate_optimize(&ctx->config) ? SX1262_BOOL_TRUE : SX1262_BOOL_FALSE));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_frequency_convert_to_register(&ctx->handle, ctx->config.rf_frequency_hz, &freq_reg));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_rf_frequency(&ctx->handle, freq_reg));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_buffer_base_address(&ctx->handle, 0u, 0u));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_apply_packet_params(ctx, LEOS_SX126X_MAX_PAYLOAD_LEN);
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_lora_symb_num_timeout(&ctx->handle, LEOS_SX126X_SYMB_NUM_TIMEOUT));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_lora_sync_word(
        &ctx->handle,
        (uint16_t)(((uint16_t)ctx->config.sync_word << 8) | ctx->config.sync_word)));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_get_tx_modulation(&ctx->handle, &modulation));
    if (status != LEOS_RADIO_OK) return status;

    modulation |= LEOS_SX126X_TX_MODULATION_DEFAULT;
    status = leos_sx126x_map_status(sx1262_set_tx_modulation(&ctx->handle, modulation));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_rx_gain(&ctx->handle, LEOS_SX126X_RX_GAIN));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_ocp(&ctx->handle, LEOS_SX126X_OCP));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_get_tx_clamp_config(&ctx->handle, &clamp));
    if (status != LEOS_RADIO_OK) return status;

    clamp |= LEOS_SX126X_TX_CLAMP_MASK;
    status = leos_sx126x_map_status(sx1262_set_tx_clamp_config(&ctx->handle, clamp));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_clear_irq_status(&ctx->handle, LEOS_SX126X_IRQ_MASK_ALL));
    if (status != LEOS_RADIO_OK) return status;

    return LEOS_RADIO_OK;
}

static void leos_sx126x_latch_rx(leos_sx126x_ctx_t *ctx, uint8_t *buf, uint16_t len)
{
    uint8_t rssi_raw = 0u;
    int8_t snr_raw = 0;
    uint8_t signal_rssi_raw = 0u;
    float rssi_pkt = 0.0f;
    float snr_pkt = 0.0f;
    float signal_rssi_pkt = 0.0f;

    if ((ctx == NULL) || (buf == NULL))
    {
        return;
    }

    if (ctx->rx_pending)
    {
        ctx->rx_overrun = true;
        return;
    }

    if (len > LEOS_SX126X_MAX_PAYLOAD_LEN)
    {
        len = LEOS_SX126X_MAX_PAYLOAD_LEN;
    }

    memcpy(ctx->rx_buf, buf, len);
    ctx->rx_len = (size_t)len;
    ctx->rx_info.crc_ok = true;

    if (sx1262_get_lora_packet_status(&ctx->handle,
                                      &rssi_raw,
                                      &snr_raw,
                                      &signal_rssi_raw,
                                      &rssi_pkt,
                                      &snr_pkt,
                                      &signal_rssi_pkt) == 0)
    {
        ctx->rx_info.rssi_dbm = (int16_t)rssi_pkt;
        ctx->rx_info.snr_db = (int8_t)snr_pkt;
    }
    else
    {
        ctx->rx_info.rssi_dbm = 0;
        ctx->rx_info.snr_db = 0;
    }

    ctx->rx_pending = true;
    ctx->mode = LEOS_RADIO_MODE_STANDBY;
}

void leos_sx126x_receive_callback_impl(leos_sx126x_ctx_t *ctx, uint16_t type, uint8_t *buf, uint16_t len)
{
    if (ctx == NULL)
    {
        return;
    }

    switch (type)
    {
        case SX1262_IRQ_TX_DONE:
            ctx->mode = LEOS_RADIO_MODE_STANDBY;
            break;

        case SX1262_IRQ_RX_DONE:
            leos_sx126x_latch_rx(ctx, buf, len);
            break;

        case SX1262_IRQ_CRC_ERR:
            ctx->mode = LEOS_RADIO_MODE_STANDBY;
            break;

        case SX1262_IRQ_TIMEOUT:
            ctx->mode = LEOS_RADIO_MODE_STANDBY;
            break;

        default:
            break;
    }
}

/**
 * @brief Fill a config struct with sane defaults for the selected radio.
 */
void leos_sx126x_get_default_config(leos_radio_t radio, leos_radio_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->tx_power_dbm = 17;
    cfg->preamble_len_symbols = 12u;
    cfg->bandwidth = LEOS_RADIO_BW_125_KHZ;
    cfg->spreading_factor = LEOS_RADIO_SF_9;
    cfg->coding_rate = LEOS_RADIO_CR_4_5;
    cfg->sync_word = 0x34u;
    cfg->crc_enabled = true;
    cfg->iq_inverted = false;

    switch (radio)
    {
        case LEOS_RADIO_SX1262:
            cfg->rf_frequency_hz = 915000000u;
            break;
        case LEOS_RADIO_SX1268:
            cfg->rf_frequency_hz = 433000000u;
            break;
        default:
            cfg->rf_frequency_hz = 0u;
            break;
    }
}

/**
 * @brief Initialize the selected radio and apply configuration.
 */
leos_radio_status_t leos_sx126x_init(leos_radio_t radio, const leos_radio_config_t *cfg)
{
    leos_sx126x_ctx_t *ctx;
    leos_radio_status_t status;
    uint8_t rc;

    if (cfg == NULL)
    {
        return LEOS_RADIO_ERR_ARG;
    }

    ctx = leos_sx126x_ctx(radio);
    if (ctx == NULL)
    {
        return LEOS_RADIO_ERR_ARG;
    }

    if (ctx->initialized)
    {
        return LEOS_RADIO_ERR_STATE;
    }

    memset(ctx->rx_buf, 0, sizeof(ctx->rx_buf));
    ctx->config = *cfg;
    ctx->irq_pending = false;
    ctx->rx_pending = false;
    ctx->rx_overrun = false;
    ctx->rx_len = 0u;
    ctx->rx_info.rssi_dbm = 0;
    ctx->rx_info.snr_db = 0;
    ctx->rx_info.crc_ok = false;
    ctx->mode = LEOS_RADIO_MODE_UNINIT;

    leos_sx126x_link_handle(ctx);

    rc = sx1262_init(&ctx->handle);
    status = leos_sx126x_map_status(rc);
    if (status != LEOS_RADIO_OK)
    {
        return status;
    }

    ctx->initialized = true;

    status = leos_sx126x_apply_config(ctx);
    if (status != LEOS_RADIO_OK)
    {
        (void)sx1262_deinit(&ctx->handle);
        ctx->initialized = false;
        ctx->mode = LEOS_RADIO_MODE_UNINIT;
        return status;
    }

    ctx->mode = LEOS_RADIO_MODE_STANDBY;
    return LEOS_RADIO_OK;
}

/**
 * @brief Transmit one packet and wait for completion.
 */
leos_radio_status_t leos_sx126x_send(leos_radio_t radio, const uint8_t *data, size_t len)
{
    leos_sx126x_ctx_t *ctx;
    leos_radio_status_t status;
    absolute_time_t deadline;
    uint8_t rc;

    if ((data == NULL) || (len == 0u) || (len > LEOS_SX126X_MAX_PAYLOAD_LEN))
    {
        return LEOS_RADIO_ERR_ARG;
    }

    ctx = leos_sx126x_ctx(radio);
    if ((ctx == NULL) || !ctx->initialized)
    {
        return LEOS_RADIO_ERR_STATE;
    }

    if (ctx->mode == LEOS_RADIO_MODE_TX)
    {
        return LEOS_RADIO_ERR_BUSY;
    }

    status = leos_sx126x_map_status(
        sx1262_set_dio_irq_params(&ctx->handle, LEOS_SX126X_IRQ_MASK_TX, LEOS_SX126X_IRQ_MASK_TX, 0u, 0u));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_clear_irq_status(&ctx->handle, LEOS_SX126X_IRQ_MASK_ALL));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_set_standby(&ctx->handle, SX1262_CLOCK_SOURCE_XTAL_32MHZ));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_apply_packet_params(ctx, (uint8_t)len);
    if (status != LEOS_RADIO_OK) return status;

    rc = sx1262_write_buffer(&ctx->handle, 0u, (uint8_t *)data, (uint16_t)len);
    status = leos_sx126x_map_status(rc);
    if (status != LEOS_RADIO_OK) return status;

    ctx->handle.tx_done = 0u;
    ctx->handle.timeout = 0u;
    ctx->irq_pending = false;
    ctx->mode = LEOS_RADIO_MODE_TX;

    rc = sx1262_set_tx(&ctx->handle, LEOS_SX126X_TX_TIMEOUT_MS * 1000u);
    status = leos_sx126x_map_status(rc);
    if (status != LEOS_RADIO_OK)
    {
        ctx->mode = LEOS_RADIO_MODE_STANDBY;
        return status;
    }

    deadline = make_timeout_time_ms(LEOS_SX126X_TX_TIMEOUT_MS);
    while (!time_reached(deadline))
    {
        if (ctx->irq_pending)
        {
            status = leos_sx126x_process_irq(radio);
            if (status != LEOS_RADIO_OK)
            {
                ctx->mode = LEOS_RADIO_MODE_STANDBY;
                return status;
            }
        }

        if (ctx->handle.tx_done)
        {
            ctx->mode = LEOS_RADIO_MODE_STANDBY;
            return LEOS_RADIO_OK;
        }

        if (ctx->handle.timeout)
        {
            ctx->mode = LEOS_RADIO_MODE_STANDBY;
            return LEOS_RADIO_ERR_TIMEOUT;
        }

        tight_loop_contents();
    }

    ctx->mode = LEOS_RADIO_MODE_STANDBY;
    return LEOS_RADIO_ERR_TIMEOUT;
}

/**
 * @brief Place the selected radio into RX mode.
 */
leos_radio_status_t leos_sx126x_start_rx(leos_radio_t radio)
{
    leos_sx126x_ctx_t *ctx;
    leos_radio_status_t status;

    ctx = leos_sx126x_ctx(radio);
    if ((ctx == NULL) || !ctx->initialized)
    {
        return LEOS_RADIO_ERR_STATE;
    }

    if (ctx->mode == LEOS_RADIO_MODE_TX)
    {
        return LEOS_RADIO_ERR_BUSY;
    }

    status = leos_sx126x_map_status(
        sx1262_set_dio_irq_params(&ctx->handle, LEOS_SX126X_IRQ_MASK_RX, LEOS_SX126X_IRQ_MASK_RX, 0u, 0u));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_clear_irq_status(&ctx->handle, LEOS_SX126X_IRQ_MASK_ALL));
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_apply_packet_params(ctx, LEOS_SX126X_MAX_PAYLOAD_LEN);
    if (status != LEOS_RADIO_OK) return status;

    status = leos_sx126x_map_status(sx1262_continuous_receive(&ctx->handle));
    if (status != LEOS_RADIO_OK) return status;

    ctx->irq_pending = false;
    ctx->mode = LEOS_RADIO_MODE_RX;
    return LEOS_RADIO_OK;
}

/**
 * @brief Return whether one unread packet is available.
 */
bool leos_sx126x_packet_available(leos_radio_t radio)
{
    leos_sx126x_ctx_t *ctx = leos_sx126x_ctx(radio);
    if (ctx == NULL)
    {
        return false;
    }

    return ctx->rx_pending;
}

/**
 * @brief Read the pending packet and optional metadata.
 */
leos_radio_status_t leos_sx126x_read(leos_radio_t radio,
                                     uint8_t *buf,
                                     size_t buf_size,
                                     size_t *out_len,
                                     leos_radio_packet_info_t *info)
{
    leos_sx126x_ctx_t *ctx;

    if ((buf == NULL) || (out_len == NULL))
    {
        return LEOS_RADIO_ERR_ARG;
    }

    ctx = leos_sx126x_ctx(radio);
    if ((ctx == NULL) || !ctx->initialized)
    {
        return LEOS_RADIO_ERR_STATE;
    }

    if (!ctx->rx_pending)
    {
        return LEOS_RADIO_ERR_STATE;
    }

    if (buf_size < ctx->rx_len)
    {
        return LEOS_RADIO_ERR_ARG;
    }

    memcpy(buf, ctx->rx_buf, ctx->rx_len);
    *out_len = ctx->rx_len;

    if (info != NULL)
    {
        *info = ctx->rx_info;
    }

    ctx->rx_pending = false;
    ctx->rx_len = 0u;
    return LEOS_RADIO_OK;
}

/**
 * @brief Place the selected radio into standby mode.
 */
leos_radio_status_t leos_sx126x_standby(leos_radio_t radio)
{
    leos_sx126x_ctx_t *ctx;
    leos_radio_status_t status;

    ctx = leos_sx126x_ctx(radio);
    if ((ctx == NULL) || !ctx->initialized)
    {
        return LEOS_RADIO_ERR_STATE;
    }

    status = leos_sx126x_map_status(sx1262_set_standby(&ctx->handle, SX1262_CLOCK_SOURCE_XTAL_32MHZ));
    if (status != LEOS_RADIO_OK)
    {
        return status;
    }

    ctx->mode = LEOS_RADIO_MODE_STANDBY;
    return LEOS_RADIO_OK;
}

/**
 * @brief Flag deferred IRQ work for the selected radio.
 */
void leos_sx126x_handle_dio1_irq(leos_radio_t radio)
{
    leos_sx126x_ctx_t *ctx = leos_sx126x_ctx(radio);
    if ((ctx == NULL) || !ctx->initialized)
    {
        return;
    }

    ctx->irq_pending = true;
}

/**
 * @brief Process deferred IRQ work outside ISR context.
 */
leos_radio_status_t leos_sx126x_process_irq(leos_radio_t radio)
{
    leos_sx126x_ctx_t *ctx;
    leos_radio_status_t status;

    ctx = leos_sx126x_ctx(radio);
    if ((ctx == NULL) || !ctx->initialized)
    {
        return LEOS_RADIO_ERR_STATE;
    }

    if (!ctx->irq_pending)
    {
        return LEOS_RADIO_OK;
    }

    ctx->irq_pending = false;
    status = leos_sx126x_map_status(sx1262_irq_handler(&ctx->handle));
    return status;
}

/**
 * @brief Return the current mode of the selected radio.
 */
leos_radio_mode_t leos_sx126x_mode(leos_radio_t radio)
{
    leos_sx126x_ctx_t *ctx = leos_sx126x_ctx(radio);
    if (ctx == NULL)
    {
        return LEOS_RADIO_MODE_UNINIT;
    }

    return ctx->mode;
}