#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sx126x.h"

#define LEOS_LORA_DEFAULT_SPI_BAUDRATE_HZ 8000000U
#define LEOS_LORA_DEFAULT_FREQUENCY_HZ 480000000U
#define LEOS_LORA_DEFAULT_TX_POWER_DBM 17
#define LEOS_LORA_DEFAULT_PREAMBLE_SYMBOLS 12U
#define LEOS_LORA_DEFAULT_SYNC_WORD 0x34U
#define LEOS_LORA_DEFAULT_TX_TIMEOUT_MS 5000U
#define LEOS_LORA_MAX_PAYLOAD_LEN 255U
#define LEOS_LORA_DEFAULT_TCXO_VOLTAGE SX126X_TCXO_CTRL_2_2V
#define LEOS_LORA_DEFAULT_TCXO_STARTUP_TIME_MS 5U

typedef enum {
	LEOS_LORA_OK = SX126X_STATUS_OK,
	LEOS_LORA_ERR_DRIVER_UNSUPPORTED_FEATURE = SX126X_STATUS_UNSUPPORTED_FEATURE,
	LEOS_LORA_ERR_DRIVER_UNKNOWN_VALUE = SX126X_STATUS_UNKNOWN_VALUE,
	LEOS_LORA_ERR_DRIVER = SX126X_STATUS_ERROR,
	LEOS_LORA_ERR_ARG = 0x80,
	LEOS_LORA_ERR_STATE,
	LEOS_LORA_ERR_HAL,
} leos_lora_result_t;

typedef struct {
	void *platform_spi;
	uint32_t spi_baudrate_hz;
	uint8_t pin_sck;
	uint8_t pin_mosi;
	uint8_t pin_miso;
	uint8_t pin_nss;
	uint8_t pin_busy;
	uint8_t pin_reset;
	uint8_t pin_dio1;
} leos_lora_hw_t;

typedef struct {
	uint32_t frequency_hz;
	int8_t tx_power_dbm;
	uint16_t preamble_symbols;
	sx126x_lora_bw_t bandwidth;
	sx126x_lora_sf_t spreading_factor;
	sx126x_lora_cr_t coding_rate;
	uint8_t sync_word;
	bool crc_enabled;
	bool invert_iq;
	bool low_data_rate_optimize;
} leos_lora_config_t;

typedef struct leos_lora_handle leos_lora_handle_t;

typedef void (*leos_lora_tx_callback_t)(leos_lora_handle_t *handle,
									 leos_lora_result_t result,
									 void *user_reference);

typedef void (*leos_lora_rx_callback_t)(leos_lora_handle_t *handle,
									 const uint8_t *payload,
									 uint8_t payload_len,
									 const sx126x_pkt_status_lora_t *packet_status,
									 void *user_reference);

typedef struct {
	leos_lora_tx_callback_t tx_done;
	leos_lora_rx_callback_t rx_done;
} leos_lora_callbacks_t;

struct leos_lora_handle {
	leos_lora_hw_t hw;
	leos_lora_config_t config;
	bool initialized;
	volatile bool irq_pending;
	volatile uint32_t irq_events_mask;
	volatile bool tx_in_progress;
	volatile bool rx_in_progress;
	uint64_t tx_deadline_us;
	leos_lora_callbacks_t callbacks;
	void *callbacks_user_reference;
	uint8_t tx_payload[LEOS_LORA_MAX_PAYLOAD_LEN];
	uint8_t tx_payload_len;
};

void leos_lora_get_default_config(leos_lora_config_t *config);

leos_lora_result_t leos_lora_init(leos_lora_handle_t *handle,
								  const leos_lora_hw_t *hw,
								  const leos_lora_config_t *config);

void leos_lora_set_callbacks(leos_lora_handle_t *handle,
							 const leos_lora_callbacks_t *callbacks,
							 void *user_reference);

leos_lora_result_t leos_lora_transmit(leos_lora_handle_t *handle,
								  const uint8_t *payload,
								  uint8_t payload_len);

leos_lora_result_t leos_lora_start_rx(leos_lora_handle_t *handle);
leos_lora_result_t leos_lora_stop_rx(leos_lora_handle_t *handle);

leos_lora_result_t leos_lora_task(leos_lora_handle_t *handle);

leos_lora_result_t leos_lora_process(leos_lora_handle_t *handle);

bool leos_lora_tx_busy(const leos_lora_handle_t *handle);

void leos_lora_irq_handler(unsigned int gpio, uint32_t events_mask);