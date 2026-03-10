#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    LEOS_RADIO_SX1262,
    LEOS_RADIO_SX1268,
} leos_radio_t;

typedef enum {
    LEOS_RADIO_OK = 0,
    LEOS_RADIO_ERR_ARG,
    LEOS_RADIO_ERR_STATE,
    LEOS_RADIO_ERR_IO,
    LEOS_RADIO_ERR_TIMEOUT,
    LEOS_RADIO_ERR_BUSY,
    LEOS_RADIO_ERR_DRIVER,
} leos_radio_status_t;

/**
 * @brief Supported LoRa bandwidth settings.
 */
typedef enum {
    LEOS_RADIO_BW_7_KHZ,
    LEOS_RADIO_BW_10_KHZ,
    LEOS_RADIO_BW_15_KHZ,
    LEOS_RADIO_BW_20_KHZ,
    LEOS_RADIO_BW_31_KHZ,
    LEOS_RADIO_BW_41_KHZ,
    LEOS_RADIO_BW_62_KHZ,
    LEOS_RADIO_BW_125_KHZ,
    LEOS_RADIO_BW_250_KHZ,
    LEOS_RADIO_BW_500_KHZ,
} leos_radio_bandwidth_t;

/**
 * @brief Supported LoRa spreading factor settings.
 */
typedef enum {
    LEOS_RADIO_SF_5 = 5,
    LEOS_RADIO_SF_6 = 6,
    LEOS_RADIO_SF_7 = 7,
    LEOS_RADIO_SF_8 = 8,
    LEOS_RADIO_SF_9 = 9,
    LEOS_RADIO_SF_10 = 10,
    LEOS_RADIO_SF_11 = 11,
    LEOS_RADIO_SF_12 = 12,
} leos_radio_spreading_factor_t;

/**
 * @brief Supported LoRa coding rate settings.
 */
typedef enum {
    LEOS_RADIO_CR_4_5,
    LEOS_RADIO_CR_4_6,
    LEOS_RADIO_CR_4_7,
    LEOS_RADIO_CR_4_8,
} leos_radio_coding_rate_t;

/**
 * @brief Public radio mode.
 */
typedef enum {
    LEOS_RADIO_MODE_UNINIT,
    LEOS_RADIO_MODE_STANDBY,
    LEOS_RADIO_MODE_RX,
    LEOS_RADIO_MODE_TX,
} leos_radio_mode_t;

/**
 * @brief Basic SX126x LoRa configuration for one radio instance.
 */
typedef struct {
    uint32_t rf_frequency_hz;                       /**< RF center frequency in Hz. */
    int8_t tx_power_dbm;                            /**< Requested TX power in dBm. */
    uint16_t preamble_len_symbols;                  /**< LoRa preamble length in symbols. */
    leos_radio_bandwidth_t bandwidth;               /**< LoRa bandwidth setting. */
    leos_radio_spreading_factor_t spreading_factor; /**< LoRa spreading factor. */
    leos_radio_coding_rate_t coding_rate;           /**< LoRa coding rate. */
    uint16_t sync_word;                              /**< LoRa sync word. */
    bool crc_enabled;                               /**< Enable CRC on TX/RX packets. */
    bool iq_inverted;                               /**< Enable IQ inversion if required. */
} leos_radio_config_t;

/**
 * @brief Metadata for the most recently read packet.
 */
typedef struct {
    int16_t rssi_dbm; /**< Packet RSSI in dBm. */
    int8_t snr_db;    /**< Packet SNR in dB. */
    bool crc_ok;      /**< True if the packet passed CRC checks. */
} leos_radio_packet_info_t;

/**
 * @brief Fill a config struct with sane defaults for the selected radio.
 */
void leos_sx126x_get_default_config(leos_radio_t radio,
                                    leos_radio_config_t *cfg);

/**
 * @brief Initialize the selected radio and apply configuration.
 */
leos_radio_status_t leos_sx126x_init(leos_radio_t radio,
                                     const leos_radio_config_t *cfg);

/**
 * @brief Transmit one packet and wait for completion.
 */
leos_radio_status_t leos_sx126x_send(leos_radio_t radio,
                                     const uint8_t *data,
                                     size_t len);

/**
 * @brief Place the selected radio into RX mode.
 *
 * @note RX packet bytes are not copied out of the radio until
 *       leos_sx126x_process_irq() services a latched DIO1 interrupt.
 *       Downstream code must therefore arrange prompt deferred IRQ
 *       servicing after DIO1 asserts; slow polling can lose packets
 *       before the SDK reads them from the radio.
 */
leos_radio_status_t leos_sx126x_start_rx(leos_radio_t radio);

/**
 * @brief Return whether one unread packet is available.
 */
bool leos_sx126x_packet_available(leos_radio_t radio);

/**
 * @brief Read the pending packet and optional metadata.
 */
leos_radio_status_t leos_sx126x_read(leos_radio_t radio,
                                     uint8_t *buf,
                                     size_t buf_size,
                                     size_t *out_len,
                                     leos_radio_packet_info_t *info);

/**
 * @brief Place the selected radio into standby mode.
 */
leos_radio_status_t leos_sx126x_standby(leos_radio_t radio);

/**
 * @brief Flag deferred IRQ work for the selected radio.
 *
 * @note This function is ISR-safe because it only latches that DIO1
 *       asserted. It does not read packet bytes from the radio.
 *       leos_sx126x_process_irq() must be called promptly afterward in
 *       non-ISR context to drain the radio before a later receive can
 *       overwrite the unread packet.
 */
void leos_sx126x_handle_dio1_irq(leos_radio_t radio);

/**
 * @brief Process deferred IRQ work outside ISR context.
 *
 * @note This function performs the SPI transactions that read received
 *       packet data out of the SX126x. It is intended to run as soon as
 *       possible after leos_sx126x_handle_dio1_irq() latches DIO1.
 *       Treating this as a low-rate polling hook rather than prompt
 *       deferred IRQ servicing risks packet loss.
 */
leos_radio_status_t leos_sx126x_process_irq(leos_radio_t radio);

/**
 * @brief Return the current mode of the selected radio.
 */
leos_radio_mode_t leos_sx126x_mode(leos_radio_t radio);
