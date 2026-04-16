#include "config.h"

leos_mcp251xfd_hw_t can_hw_config = {
    .spi = spi0,
    .spi_baud = 10000000, // 10 MHz,
    .pin_sck = 2,
    .pin_mosi = 3,
    .pin_miso = 4,
    .pin_cs = 5,
    .pin_irq = 6
};

MCP251XFD_FIFO fifo_configs[] = {
    {
        .Name = MCP251XFD_TXQ,
        .Size = MCP251XFD_FIFO_8_MESSAGE_DEEP,
        .Payload = MCP251XFD_PAYLOAD_64BYTE,
        .Attempts = MCP251XFD_THREE_ATTEMPTS, // Will retry enough to make it past arbitration, but won't deadlock when only device on bus.
        .Priority = MCP251XFD_MESSAGE_TX_PRIORITY16,
        .ControlFlags = MCP251XFD_FIFO_NO_RTR_RESPONSE,
        .InterruptFlags = MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT,
    },
    {
        .Name = MCP251XFD_FIFO1,
        .Size = MCP251XFD_FIFO_16_MESSAGE_DEEP,
        .Payload = MCP251XFD_PAYLOAD_64BYTE,
        .Direction = MCP251XFD_RECEIVE_FIFO,
        .ControlFlags = MCP251XFD_FIFO_ADD_TIMESTAMP_ON_RX,
        .InterruptFlags = MCP251XFD_FIFO_OVERFLOW_INT + MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT,
    }};

MCP251XFD_Filter filter_configs[] = {
    {.Filter = MCP251XFD_FILTER0,
     .EnableFilter = true,
     .AcceptanceID = MCP251XFD_ACCEPT_ALL_MESSAGES,
     .AcceptanceMask = MCP251XFD_ACCEPT_ALL_MESSAGES,
     .Match = MCP251XFD_MATCH_SID_EID,
     .PointTo = MCP251XFD_FIFO1}
};

leos_mcp251xfd_config_t can_config = {
    .xtal_hz = 40000000, // 40 MHz,
    .osc_hz = 0,
    .sysclk_config = MCP251XFD_SYSCLK_IS_CLKIN, // Our crystal needs no multiplication
    .nominal_bitrate = 1000000,                 // 1 MHz
    .data_bitrate = 4000000,                    // 4Mhz, increase if needed
    .bandwidth = MCP251XFD_NO_DELAY,            // No delay between sending packets
    .ctrl_flags = MCP251XFD_CAN_RESTRICTED_MODE_ON_ERROR |
                  MCP251XFD_CAN_ESI_REFLECTS_ERROR_STATUS |
                  MCP251XFD_CAN_RESTRICTED_RETRANS_ATTEMPTS |
                  MCP251XFD_CANFD_BITRATE_SWITCHING_ENABLE |
                  MCP251XFD_CAN_PROTOCOL_EXCEPT_AS_FORM_ERROR |
                  MCP251XFD_CANFD_USE_ISO_CRC |
                  MCP251XFD_CANFD_DONT_USE_RRS_BIT_AS_SID11,
    .irq_flags = MCP251XFD_INT_RX_EVENT | MCP251XFD_INT_BUS_ERROR_EVENT,
    .initial_mode = MCP251XFD_NORMAL_CANFD_MODE,

    .fifo = fifo_configs,
    .num_fifos = count_of(fifo_configs),

    .filter = filter_configs,
    .num_filters = count_of(filter_configs)};
