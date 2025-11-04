#pragma once

#include "mcp251xfd/config.h"
#include "MCP251XFD.h"

/**
 * @brief GPIO interrupt dispatch entry for MCP251XFD instances.
 *
 * This function is intended to be called from the global GPIO IRQ handler
 * (e.g., the callback registered via gpio_set_irq_enabled_with_callback()).
 * It checks whether the provided GPIO pin corresponds to a registered MCP251XFD
 * device instance and, if so, invokes the instance-specific IRQ callback hook
 * configured during initialization.
 *
 * The callback invoked by this function must be lightweight and must NOT
 * perform SPI communication or Cyphal/canard processing. The expected behavior
 * is to simply signal that RX processing is pending (e.g., by setting a flag
 * or releasing a semaphore). Any frame handling should occur in a deferred
 * task context outside of the interrupt handler.
 *
 * @param gpio   The GPIO pin number that triggered the interrupt.
 * @param events The GPIO event mask passed by the RP2040 IRQ dispatcher.
 */
void leos_mcp251xfd_check_irq(uint gpio, uint32_t events);

/* ========== MCP251XFD Initialization Helpers ========== */

/**
 * @brief Initialize the MCP251XFD CAN device with Raspberry Pi Pico–specific settings.
 *
 * This function configures the MCP251XFD device using the provided SPI interface
 * and applies static configuration values required for operation on the Raspberry Pi Pico.
 *
 * @param[in]  hw           Pointer to hardware pin ports for the MCP251XFD.
 * @param[in]  config       Pointer to configuration parameters for the MCP251XFD family of chips.
 * @param[out] dev          Pointer to the MCP251XFD device state structure. Must be zero-initialized
 *                          before calling this function. On success, it will be populated with
 *                          the driver’s internal state.
 *
 * @return
 * - @ref ERR_OK if the device was successfully initialized.
 * - One of the other @ref eERRORRESULT values if initialization fails.
 *
 * @note The SPI clock frequency must be less than 80% of the MCP251XFD system clock (SYSCLK).
 */
eERRORRESULT leos_mcp251xfd_init(MCP251XFD *dev, const leos_mcp251xfd_hw_t *hw, const leos_mcp251xfd_config_t *config);
