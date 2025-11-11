#pragma once

#include "mcp251xfd/config.h"
#include "MCP251XFD.h"

/**
 * @brief User-provided callback invoked to service the receive FIFO.
 *
 * The callback is executed from a non-interrupt context (i.e., from
 * @ref leos_mcp251xfd_task()). The callback is responsible for draining
 * the configured receive FIFO (commonly FIFO1). This typically involves
 * repeatedly calling @ref MCP251XFD_ReceiveMessageFromFIFO() until the
 * FIFO is empty.
 *
 * @param device          Pointer to the MCP251XFD instance.
 * @param user_reference  Opaque pointer supplied during callback registration.
 */
typedef void (*leos_mcp251xfd_rx_cb)(MCP251XFD *device, void *user_reference);


/* ========== MCP251XFD Initialization Helpers ========== */

/**
 * @brief Initialize and configure an MCP251XFD device for the Raspberry Pi Pico platform.
 *
 * This function:
 *  - Initializes the GPIO and SPI interface for the MCP251XFD.
 *  - Applies bitrate, clock source, filter, and FIFO configuration.
 *  - Optionally installs the global GPIO IRQ callback used to wake the
 *    MCP251XFD service task.
 *
 * @param[in,out] device
 *     Pointer to a user-allocated @ref MCP251XFD structure. **Must be zeroed**
 *     before calling this function. On success, it will store the initialized
 *     driver state.
 *
 * @param[in] hw
 *     Hardware pin and SPI configuration for this device.
 *
 * @param[in] config
 *     Static configuration describing bit timing, filters, FIFOs, and mode.
 *
 * @param[in] attachInterrupt
 *     If `true`, this function installs a shared global IRQ handler for GPIO
 *     interrupts and enables interrupts for @ref hw->pin_irq.
 *
 *     If `false`, the caller is responsible for configuring the GPIO interrupt
 *     system and MUST manually call @ref leos_mcp251xfd_irq_handler() from their
 *     own ISR handler that is setup BEFORE calling @ref leos_mcp251xfd_init()!
 *
void leos_mcp251xfd_deinit(MCP251XFD *dev);
 * @return eERRORRESULT
 *     - @ref ERR_OK on success.
 *     - A specific error code if initialization fails.
 *
 * @note The SPI clock must not exceed 80% of SYSCLK. For SYSCLK = 40 MHz,
 *       SPI should be ≤ 32 MHz.
 */
eERRORRESULT leos_mcp251xfd_init(MCP251XFD *device,
                                 const leos_mcp251xfd_hw_t *hw,
                                 const leos_mcp251xfd_config_t *config,
                                 bool attachInterrupt);

/**
 * @brief Core GPIO interrupt handler entry point.
 *
 * This function is specifically intended to be invoked from the RP2040's
 * `gpio_set_irq_enabled_with_callback()` dispatch. It marks the MCP251XFD
 * context for deferred processing.
 *
 * @param gpio   GPIO index on which INT was observed.
 * @param events GPIO event mask.
 */
void leos_mcp251xfd_irq_handler(uint gpio, uint32_t events);

/**
 * @brief Register the global GPIO interrupt callback used for MCP251XFD devices.
 *
 * This sets up the RP2040 GPIO interrupt infrastructure (e.g., enabling
 * `IO_IRQ_BANK0` and installing a shared IRQ callback). This is usually
 * called automatically when `attachInterrupt == true` in @ref leos_mcp251xfd_init().
 */
void leos_mcp251xfd_attach_gpio_interrupt(void);

/**
 * @brief Process pending MCP251XFD interrupts and service RX/TX events.
 *
 * This function must be called regularly (e.g., inside the main loop or a
 * cooperative task scheduler). If an interrupt was signaled via
 * @ref leos_mcp251xfd_gpio_isr(), this function handles it by:
 *
 *  - Querying and clearing interrupt causes
 *  - Calling the user-provided RX callback to drain FIFO frames
 *  - Notifying TX-complete events if needed
 *
 * No actual interrupt or SPI work occurs in the ISR; all processing occurs here.
 *
 * @param dev Pointer to the MCP251XFD instance to service.
 */
void leos_mcp251xfd_task(MCP251XFD *dev);

/**
 * @brief Register a user RX callback for processing received CAN frames.
 *
 * The callback will be invoked by @ref leos_mcp251xfd_task() whenever the
 * receive FIFO signals pending frames. The callback must drain the FIFO
 * completely to ensure that the MCP251XFD INT pin deasserts correctly.
 *
 * @param dev             MCP251XFD instance to register the callback for.
 * @param callback        The receive callback function.
 * @param user_reference  Opaque pointer passed to the callback on invocation.
 */
void leos_mcp251xfd_set_rx_handler(MCP251XFD *dev,
                                    leos_mcp251xfd_rx_cb callback,
                                    void *user_reference);

/**
 * @brief Deinitialize an MCP251XFD device instance and release driver resources.
 *
 * This function reverses the initialization performed by leos_mcp251xfd_init().
 * It disables the interrupt associated with the device's IRQ pin, clears the
 * internal driver context lookup entry, frees the driver's context structure,
 * and detaches the context from the MCP251XFD handle.
 *
 * After calling this function:
 *   - The MCP251XFD *dev handle must NOT be used again unless re-initialized.
 *   - Any registered RX callback becomes invalid.
 *   - leos_mcp251xfd_task() must no longer be called for this device.
 *
 * This function is safe to call even if initialization failed or was only
 * partially completed (it performs null checks).
 *
 * @param dev Pointer to the MCP251XFD device instance that was previously
 *            initialized with leos_mcp251xfd_init(). May be NULL.
 *
 * @note This does not power down the MCP251XFD hardware or alter SPI pins.
 *       The caller is responsible for disabling SPI peripherals or placing
 *       the CAN controller into sleep mode if needed.
 *
 * @note If multiple MCP251XFD instances are used, this function affects only
 *       the instance associated with @p dev and its configured IRQ pin.
 */
void leos_mcp251xfd_deinit(MCP251XFD *dev);
