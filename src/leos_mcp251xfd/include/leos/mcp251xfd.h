#pragma once

#include "mcp251xfd/config.h"
#include "MCP251XFD.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    bool txq_full_logged;                 /**< Suppress repeated full-queue messages. */
    bool tx_error_logged;                 /**< Suppress repeated SPI/TXQ send errors until recovery. */
    bool error_status_known;              /**< Whether last_error_status is valid. */
    uint8_t last_error_status;            /**< Last logged CAN error-state flags. */
} leos_mcp251xfd_t;


/* ========== MCP251XFD Initialization Helpers ========== */

/**
 * @brief Initialize and configure an MCP251XFD device for the Raspberry Pi Pico platform.
 *
 * This function:
 *  - Initializes the GPIO and SPI interface for the MCP251XFD.
 *  - Applies bitrate, clock source, filter, and FIFO configuration.
 *  - Configures the IRQ pin as an input and enables its falling-edge interrupt.
 *
 * Register a Pico GPIO IRQ handler on the calling core before initialization.
 * The driver enables IO_IRQ_BANK0 and the device pin after the MCP is ready.
 * The application can forward callback events to
 * leos_mcp251xfd_handle_gpio_irq() to match this device's IRQ pin.
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
 *     - @ref ERR__CONFIGURATION if no GPIO IRQ handler is registered.
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
 * @brief Match a Pico GPIO callback event to this device and latch its IRQ.
 *
 * Call this from the application's shared GPIO callback with its gpio and
 * event_mask arguments. Only a falling edge on device->hw.pin_irq is accepted.
 * No SPI work occurs here; call leos_mcp251xfd_task() outside the ISR.
 *
 * @return true if the event belonged to this device, false otherwise.
 */
bool leos_mcp251xfd_handle_gpio_irq(leos_mcp251xfd_t *device, uint gpio,
                                   uint32_t event_mask);

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
 * again to process any other pending events. The Cyphal node drains FIFO1
 * through leos::cyphal::Node::poll().
 *
 * @param dev Pointer to the device to service.
 */
void leos_mcp251xfd_task(leos_mcp251xfd_t *dev);

/**
 * @brief Queue one extended-ID CAN FD frame for transmission.
 *
 * Payload lengths between CAN FD DLC sizes are zero-padded to the next valid
 * wire length. For example, 9 bytes are sent as a 12-byte frame.
 *
 * @return ERR_OK if queued, ERR__BUFFER_FULL if TXQ cannot accept a frame,
 *         or another vendor error. The caller may retry a full queue later.
 */
eERRORRESULT leos_mcp251xfd_send(leos_mcp251xfd_t *dev, uint32_t can_id,
                                 const uint8_t *data, size_t size);

/**
 * @brief Read one frame from FIFO1 without blocking.
 *
 * @p data must have room for 64 bytes; @p capacity is checked before reading.
 * @p size receives the wire payload length derived from the frame's DLC.
 * @p timestamp_us may be NULL. The controller is configured for microsecond
 * timestamp ticks during initialization.
 *
 * @return ERR_OK for a frame, ERR__NO_DATA_AVAILABLE if FIFO1 is empty,
 *         or another vendor error.
 */
eERRORRESULT leos_mcp251xfd_receive(leos_mcp251xfd_t *dev, uint32_t *can_id,
                                    uint8_t *data, size_t capacity,
                                    size_t *size, uint32_t *timestamp_us);

/**
 * @brief Deinitialize an MCP251XFD device instance.
 *
 * This disables the device pin's falling-edge interrupt and clears the
 * driver's interrupt state. The application still owns its GPIO callback.
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

#ifdef __cplusplus
}
#endif
