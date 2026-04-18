#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "leos/sx126x.h"
#include "driver_sx1262.h"

#define LEOS_SX126X_MAX_PAYLOAD_LEN       255u
#define LEOS_SX126X_IRQ_MASK_ALL          0x03FFu
#define LEOS_SX126X_IRQ_MASK_TX           (SX1262_IRQ_TX_DONE | SX1262_IRQ_TIMEOUT)
#define LEOS_SX126X_IRQ_MASK_RX           (SX1262_IRQ_RX_DONE | SX1262_IRQ_TIMEOUT | SX1262_IRQ_CRC_ERR)

#define LEOS_SX126X_TX_TIMEOUT_MS         200u
#define LEOS_SX126X_BUSY_TIMEOUT_MS       50u

#define LEOS_SX126X_PA_DUTY_CYCLE         0x02u
#define LEOS_SX126X_PA_HP_MAX             0x03u
#define LEOS_SX126X_RX_GAIN               0x94u
#define LEOS_SX126X_OCP                   0x38u
#define LEOS_SX126X_TX_MODULATION_DEFAULT 0x04u
#define LEOS_SX126X_TX_CLAMP_MASK         0x1Eu
#define LEOS_SX126X_SYMB_NUM_TIMEOUT      0u

typedef struct
{
    leos_radio_t radio;
    sx1262_handle_t handle;
    leos_radio_hw_config_t hw_config;
    leos_radio_config_t config;
    leos_radio_mode_t mode;
    bool initialized;
    volatile bool irq_pending;
    volatile uint64_t irq_latched_at_ms;
    bool tx_in_flight;
    uint64_t tx_started_at_ms;
    leos_radio_status_t last_tx_status;
    bool rx_pending;
    bool rx_overrun;
    uint8_t rx_buf[LEOS_SX126X_MAX_PAYLOAD_LEN];
    size_t rx_len;
    leos_radio_packet_info_t rx_info;
} leos_sx126x_ctx_t;

extern leos_sx126x_ctx_t g_sx1262;
extern leos_sx126x_ctx_t g_sx1268;

leos_sx126x_ctx_t *leos_sx126x_ctx(leos_radio_t radio);
leos_radio_status_t leos_sx126x_map_status(uint8_t rc);
void leos_sx126x_receive_callback_impl(leos_sx126x_ctx_t *ctx, uint16_t type, uint8_t *buf, uint16_t len);
void leos_sx126x_link_handle(leos_sx126x_ctx_t *ctx);
uint64_t leos_sx126x_platform_now_ms(void);
void leos_sx126x_platform_idle(void);
uint8_t leos_sx126x_platform_read_dio1(leos_sx126x_ctx_t *ctx);
uint8_t leos_sx126x_platform_read_busy(leos_sx126x_ctx_t *ctx);
