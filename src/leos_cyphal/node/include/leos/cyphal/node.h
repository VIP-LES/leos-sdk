#pragma once
#include "leos/cyphal.h"
#include "leos/cyphal/transport.h"
#include "canard.h"
#include "pico/time.h"
#include <uavcan/node/Health_1_0.h>
#include <uavcan/node/Mode_1_0.h>

#define HEARTBEAT_INTERVAL_MS 1000
#define CYPHAL_MAX_SUBSCRIPTIONS 25 // Arbitrary limit

typedef struct leos_cyphal_node {
    // CAN transport this node uses (MCP, FDCAN, SocketCAN, etc.)
    leos_cyphal_transport_t transport;

    

    // libcanard instance (session state machine)
    struct CanardInstance can;

    // Outgoing transfer queue
    struct CanardTxQueue txq;

    // Node status reported in Heartbeat
    uavcan_node_Health_1_0 health;
    uavcan_node_Mode_1_0   mode;

    // Set by IRQ callback (or semaphore in RTOS)
    volatile bool rx_pending;

    // (Internal) Used for heartbeat transfer ID
    absolute_time_t next_heartbeat;
    uint8_t heartbeat_tid;

    // Subscription list
    leos_cyphal_subscription_t* subscriptions[CYPHAL_MAX_SUBSCRIPTIONS];
    


} leos_cyphal_node_t;

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
leos_cyphal_result_t leos_cyphal_init(leos_cyphal_node_t *node, leos_cyphal_transport_t transport, uint8_t node_id);

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

/**
 * @brief Subscribe to a Cyphal subject or service.
 *
 * This function registers a new subscription with the Cyphal transport layer.
 * The provided callback will be invoked whenever a complete transfer matching
 * the subscription parameters is received and successfully parsed by libcanard.
 *
 * The subscription state (RX session tracking, transfer-ID, payload reassembly buffer)
 * is allocated from the node's memory resource (typically o1heap). The caller does not
 * need to interact with the subscription object directly; only the callback and
 * user reference are exposed.
 *
 * If a subscription for the same (kind, port_id) already exists, that subscription
 * is replaced. The old subscription state is released back to the allocator.
 *
 * @param      node            Pointer to the Cyphal node instance.
 * @param      kind            Transfer kind (message, request, or response).
 * @param      port_id         Subject-ID or service-ID to subscribe to.
 * @param      extent          Maximum expected serialized payload size (per Cyphal spec).
 * @param      callback        Function invoked on successful reception of a transfer.
 * @param      user_reference  User-defined pointer passed back to the callback.
 *
 * @return LEOS_CYPHAL_OK on success.
 *         LEOS_CYPHAL_INVALID_ARGUMENT if @p node is NULL.
 *         LEOS_CYPHAL_OUT_OF_MEMORY if allocation failed.
 *         LEOS_CYPHAL_TOO_MANY_SUBSCRIPTIONS if no available slots.
 *         Negative libcanard error codes may be returned if subscription setup fails.
 */
leos_cyphal_result_t leos_cyphal_subscribe(
    leos_cyphal_node_t *node,
    const enum CanardTransferKind kind,
    const CanardPortID port_id,
    const size_t extent,
    leos_cyphal_rx_callback callback,
    void *user_reference);

/**
 * @brief Unsubscribe from a Cyphal subject or service.
 *
 * This removes a previously registered subscription matching the specified
 * transfer kind and port-ID. Any associated subscription state is released
 * back to the node's memory allocator.
 *
 * If no matching subscription is found, the call succeeds with no effect.
 *
 * @param node     Pointer to the Cyphal node instance.
 * @param kind     Transfer kind (message, request, or response).
 * @param port_id  Subject-ID or service-ID to unsubscribe from.
 *
 * @return LEOS_CYPHAL_OK on success (even if no subscription existed).
 *         LEOS_CYPHAL_INVALID_ARGUMENT if @p node is NULL.
 *         Negative libcanard error codes may be returned if unsubscribe fails.
 */
leos_cyphal_result_t leos_cyphal_unsubscribe(
    leos_cyphal_node_t *node,
    const enum CanardTransferKind kind,
    const CanardPortID port_id);

/**
 * @brief Process pending CAN/Cyphal RX transfers.
 *
 * This function must be called whenever the transport backend indicates that
 * one or more CAN frames are ready to be consumed (e.g., via IRQ or polling).
 * It drains all available RX frames from the underlying CAN transport and
 * forwards them into the libcanard instance for reassembly and delivery to
 * registered Cyphal subscriptions.
 *
 * @param node Pointer to an initialized Cyphal node instance.
 *
 * @return void. Subscription callbacks are invoked as frames are reassembled
 *         into valid Cyphal transfers.
 */
void leos_cyphal_rx_process(leos_cyphal_node_t *node);
