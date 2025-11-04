#pragma once
#include "canard.h"



typedef uint32_t (*CanardRxFrameHandler)(
    void *user_reference,
    struct CanardInstance *can);

    
typedef struct leos_canbus_transport
{
    // Called by canardTxPoll() to send a single frame out
    CanardTxFrameHandler tx_frame;

    // Called from the main loop (or cyphal_task) when RX work is pending
    CanardRxFrameHandler rx_process;

    // Transport-specific context pointer (e.g., MCP251XFD*, FDCAN*, etc.)
    void *user_reference;
} leos_canbus_transport_t;

typedef void (*leos_canbus_message_handler)(
    const struct CanardRxTransfer *transfer,
    void *user_reference);

typedef struct {
    leos_canbus_message_handler handler;
    void *user_reference;
} leos_canbus_subcription_callback_t;