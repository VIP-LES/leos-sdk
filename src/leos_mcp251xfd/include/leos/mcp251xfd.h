#pragma once

#include "mcp251xfd/config.h"
#include "MCP251XFD.h"

/**
 * @brief Caller-owned MCP251XFD device and Pico hardware configuration.
 *
 * Fill in @c hw before initialization. Keep this object at a stable address
 * while initialized: the vendor handle passes its address to the Pico SPI HAL.
 * Use @c &device.native when calling the vendor MCP251XFD API directly.
 */
typedef struct
{
    MCP251XFD native;                    /**< Vendor controller state. */
    leos_mcp251xfd_hw_t hw;              /**< Pico SPI and GPIO configuration. */
    volatile bool irq_pending;           /**< IRQ work to service outside the ISR. */
} leos_mcp251xfd_t;


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
 *     Pointer to caller-owned @ref leos_mcp251xfd_t storage. The function
 *     initializes the embedded vendor handle. Fill in @c device->hw first.
 *     Deinitialize before initializing the same object again.
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
eERRORRESULT leos_mcp251xfd_init(leos_mcp251xfd_t *device,
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
void leos_mcp251xfd_irq_handler(leos_mcp251xfd_t *device);

/**
 * @brief Process pending MCP251XFD interrupts and service RX/TX events.
 *
 * This function must be called regularly (e.g., inside the main loop or a
 * cooperative task scheduler). If an interrupt was signaled via
 * @ref leos_mcp251xfd_irq_handler(), this function handles it by:
 *
 *  - Querying and clearing interrupt causes
 *  - Handling non-RX interrupt events
 *  - Leaving FIFO1 frames for the caller's receive path to drain
 *
 * No SPI work occurs in the ISR. If FIFO1 is pending, this function returns
 * with the IRQ latched so the caller can drain FIFO1, then call this function
 * again to process any other pending events. The Cyphal MCP transport drains
 * FIFO1 through leos_cyphal_rx_process().
 *
 * @param dev Pointer to the device to service.
 */
void leos_mcp251xfd_task(leos_mcp251xfd_t *dev);

/**
 * @brief Deinitialize an MCP251XFD device instance.
 *
 * This clears the driver's interrupt state. The
 * application owns GPIO interrupt configuration and must disable its IRQ
 * routing before deinitializing the device.
 *
 * After calling this function:
 *   - The embedded vendor handle must not be used again unless re-initialized.
 *   - leos_mcp251xfd_task() must no longer be called for this device.
 *
 * This function is safe to call even if initialization failed or was only
 * partially completed (it performs null checks).
 *
 * @param dev Pointer to the device instance that was previously
 *            initialized with leos_mcp251xfd_init(). May be NULL.
 *
 * @note This does not power down the MCP251XFD hardware or alter SPI pins.
 *       The caller is responsible for disabling SPI peripherals or placing
 *       the CAN controller into sleep mode if needed.
 *
 */
void leos_mcp251xfd_deinit(leos_mcp251xfd_t *dev);
