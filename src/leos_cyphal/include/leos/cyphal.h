
#pragma once
#include "canard.h"

/**
 * @brief Result codes returned by Cyphal node and transport functions.
 */
typedef enum leos_cyphal_result
{
    LEOS_CYPHAL_OK = 0, ///< Operation completed successfully.

    LEOS_CYPHAL_INVALID_ARGUMENT = -1,    ///< One or more provided arguments are invalid.
    LEOS_CYPHAL_OUT_OF_MEMORY = -2,       ///< Memory allocator failed (libcanard allocation).
    LEOS_CYPHAL_SERIALIZATION_ERROR = -3, ///< DSDL serialization or deserialization failure.

    LEOS_CYPHAL_TX_ERROR = -4, ///< CAN transmit operation failed.
    LEOS_CYPHAL_RX_ERROR = -5, ///< CAN receive or frame handling failed.

    
    LEOS_CYPHAL_TOO_MANY_SUBSCRIPTIONS = -10,
    LEOS_CYPHAL_ERROR_UNKNOWN = -100, ///< Unclassified internal or unexpected failure.

} leos_cyphal_result_t;

typedef void (*leos_cyphal_rx_callback)(struct CanardRxTransfer *transfer, void *user_reference);

typedef struct
{
    struct CanardRxSubscription sub;
    leos_cyphal_rx_callback callback;
    void *user_ref;
} leos_cyphal_subscription_t;