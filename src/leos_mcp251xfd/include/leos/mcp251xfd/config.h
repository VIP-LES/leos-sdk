#pragma once
#include "hardware/spi.h"
#include "MCP251XFD.h"

/**
 * @brief SPI configuration for the Raspberry Pi Pico.
 *
 * This structure holds the information required to configure an SPI
 * interface for use with the HAL driver initialization function.
 *
 * @typedef leos_mcp251xfd_hw_t
 */
typedef struct {
    spi_inst_t *spi;
    uint8_t pin_mosi;
    uint8_t pin_miso;
    uint8_t pin_sck;
    uint8_t pin_cs;
    uint8_t pin_int;
    uint8_t pin_irq;
    uint32_t spi_baud;
} leos_mcp251xfd_hw_t;

typedef struct {
    uint32_t xtal_hz;
    uint32_t osc_hz;
    eMCP251XFD_CLKINtoSYSCLK sysclk_config;
    uint32_t nominal_bitrate;
    uint32_t data_bitrate;
    eMCP251XFD_Bandwidth bandwidth;
    setMCP251XFD_CANCtrlFlags ctrl_flags;
    setMCP251XFD_InterruptEvents irq_flags;
    const MCP251XFD_FIFO *fifo;
    size_t num_fifos;
    const MCP251XFD_Filter *filter;
    size_t num_filters;
    eMCP251XFD_OperationMode initial_mode;
} leos_mcp251xfd_config_t;
