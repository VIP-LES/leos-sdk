#pragma once
#include "canard.h"

typedef uint32_t (*CanardRxFrameHandler)(
    void *user_reference,
    struct CanardInstance *can);

typedef struct leos_cyphal_transport
{
    // Called by canardTxPoll() to send a single frame out
    CanardTxFrameHandler tx_frame;

    // Called from the main loop (or cyphal_task) when RX work is pending
    CanardRxFrameHandler rx_process;

    // Transport-specific context pointer (e.g., MCP251XFD*, FDCAN*, etc.)
    void *user_reference;
} leos_cyphal_transport_t;

/**
 * @brief Dispatches a completed Cyphal transfer to the user callback associated
 *        with the given subscription.
 *
 * This function is invoked after a transfer has been fully reconstructed by
 * libcanard (i.e., after `canardRxAccept()` returns a positive value).
 * It retrieves the application-level callback and user context from the
 * subscription's `user_reference` field, and then invokes the callback.
 *
 * The subscription must have been created via `leos_cyphal_subscribe()`,
 * which ensures that `subscription->user_reference` points to a valid
 * `leos_cyphal_subscription_t` structure containing:
 *   - A user callback function
 *   - An optional user reference pointer
 *
 * @note This function does not free the transfer payload. The caller
 *       (usually the transport RX processing function) is responsible for
 *       freeing the payload buffer via `node->can.memory.deallocate()`
 *       once the callback returns.
 *
 * @param[in] transfer
 *      The received transfer object populated by libcanard. Must be non-NULL
 *      and represent a completed transfer.
 *
 * @param[in] subscription
 *      The Cyphal subscription state object associated with the received
 *      transfer. Must have been registered earlier via `canardRxSubscribe()`.
 *      Its `user_reference` field must point to a valid
 *      `leos_cyphal_subscription_t`.
 *
 * @return void
 */
void leos_cyphal_subscription_dispatch(struct CanardRxTransfer* transfer, struct CanardRxSubscription *subscription);