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
 *  - Configures the IRQ pin as an input.
 *
 * The application owns the Pico GPIO IRQ callback. Its dispatcher calls
 * leos_mcp251xfd_irq_handler() for this device when the IRQ pin signals.
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
 * @return eERRORRESULT
 *     - @ref ERR_OK on success.
 *     - A specific error code if initialization fails.
 *
 * @note The SPI clock must not exceed 80% of SYSCLK. For SYSCLK = 40 MHz,
 *       SPI should be ≤ 32 MHz.
 */
eERRORRESULT leos_mcp251xfd_init(MCP251XFD *device,
                                 const leos_mcp251xfd_hw_t *hw,
                                 const leos_mcp251xfd_config_t *config);

/**
 * @brief Core GPIO interrupt handler entry point.
 *
 * Call this from the application's shared GPIO callback after routing the
 * interrupt pin to this device. It only latches a flag; call
 * leos_mcp251xfd_task() later in non-interrupt context to perform SPI work.
 *
 * @param device MCP251XFD device whose IRQ pin signaled.
 */
void leos_mcp251xfd_irq_handler(MCP251XFD *device);

/**
 * @brief Process pending MCP251XFD interrupts and service RX/TX events.
 *
 * This function must be called regularly (e.g., inside the main loop or a
 * cooperative task scheduler). If an interrupt was signaled via
 * @ref leos_mcp251xfd_irq_handler(), this function handles it by:
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
 * This frees the driver's context structure and detaches it from the device.
 * The application owns GPIO interrupt configuration and must disable its IRQ
 * routing before deinitializing the device.
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
 */
void leos_mcp251xfd_deinit(MCP251XFD *dev);
