#pragma once

#include "cyphal/node.h"
#include "cyphal/result.h"

#include "leos/cyphal.h"
#include "leos/canbus.h"
#include "leos/log.h"
#include "leos/cyphal/memory.h"
#include "pico/stdlib.h"
#include "canard.h"
#include <uavcan/node/Heartbeat_1_0.h>

#define HEARTBEAT_INTERVAL_MS 1000

/**
 * @brief Initialize a Cyphal node state object.
 *
 * This configures the Cyphal node and libcanard runtime state, binds the node to
 * the specified CAN transport, initializes the TX queue, and sets the initial
 * heartbeat reporting state.
 *
 * @param node        Pointer to the node state structure to initialize.
 * @param transport   A transport backend instance containing TX/RX function hooks.
 * @param node_id     The local Cyphal node-ID to assign.
 *
 * @return LEOS_CYPHAL_OK on success, otherwise an error code.
 */
leos_cyphal_result_t leos_cyphal_init(leos_cyphal_node_t *node, leos_canbus_transport_t transport, uint8_t node_id);

/**
 * @brief Perform one iteration of Cyphal network processing.
 *
 * This function must be called periodically (e.g. in the main application loop).
 * It performs:
 *   - CAN RX processing if signaled by the transport IRQ
 *   - CAN TX queue flushing via canardTxPoll()
 *   - Periodic heartbeat publication
 *
 * This function is non-blocking and executes quickly.
 *
 * @param node Pointer to the initialized Cyphal node instance.
 */
void leos_cyphal_task(leos_cyphal_node_t *node);

/**
 * @brief Queue a Cyphal transfer for transmission.
 *
 * The transfer will be emitted automatically during subsequent
 * calls to leos_cyphal_task().
 *
 * @param node   Pointer to the Cyphal node instance.
 * @param meta   Transfer metadata (subject/service ID, priority, etc.).
 * @param payload Serialized message payload.
 *
 * @return LEOS_CYPHAL_OK on success, LEOS_CYPHAL_TX_ERROR otherwise.
 */
leos_cyphal_result_t leos_cyphal_push(leos_cyphal_node_t *node, const struct CanardTransferMetadata *meta, const struct CanardPayload payload);


/**
 * @brief Publish the standard Cyphal heartbeat message.
 *
 * This function serializes and queues `uavcan.node.Heartbeat.1.0`.
 * Typically invoked automatically by leos_cyphal_task().
 *
 * @param node Pointer to the Cyphal node instance.
 *
 * @return LEOS_CYPHAL_OK on success,
 *         LEOS_CYPHAL_SERIALIZATION_ERROR on serialization failure,
 *         LEOS_CYPHAL_TX_ERROR if transmission queueing fails.
 */
leos_cyphal_result_t leos_cyphal_publish_heartbeat(leos_cyphal_node_t *node);