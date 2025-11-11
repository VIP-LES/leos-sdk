#include "leos/cyphal/node.h"
#include "leos/cyphal/transport.h"
#include "leos/log.h"
#include "memory.h"
#include "pico/stdlib.h"
#include <uavcan/node/Heartbeat_1_0.h>

int8_t tx_callback(void *const node_ref, const CanardMicrosecond deadline_usec, struct CanardMutableFrame *const frame) {
    leos_cyphal_node_t *node = (leos_cyphal_node_t*) node_ref;
    leos_cyphal_transport_t transport = node->transport;
    CanardTxFrameHandler tx = transport.tx_frame;
    return tx(transport.user_reference, deadline_usec, frame);
}

leos_cyphal_result_t leos_cyphal_init(leos_cyphal_node_t *node, leos_cyphal_transport_t transport, uint8_t node_id) {
    memset(node, 0, sizeof(*node));

    node->transport = transport;
    node->rx_pending = false;
    node->heartbeat_tid = 0;

    canard_memory_init();
    struct CanardMemoryResource mem = canard_memory_resource();

    node->can = canardInit(mem);
    node->can.node_id = node_id;

    node->txq = canardTxInit(32, CANARD_MTU_CAN_FD, mem);

    node->health.value = uavcan_node_Health_1_0_NOMINAL;
    node->mode.value   = uavcan_node_Mode_1_0_INITIALIZATION;

    LOG_INFO("Cyphal node initialized (ID = %d)", node_id);
    return LEOS_CYPHAL_OK;
}

void leos_cyphal_rx_process(leos_cyphal_node_t *node) {
        if (!node) return;
        node->transport.rx_process(node->transport.user_reference, &node->can);
}

void leos_cyphal_task(leos_cyphal_node_t *node) {
    int poll = canardTxPoll(&node->txq, &node->can, to_us_since_boot(get_absolute_time()), node, tx_callback, NULL, NULL);
    if (poll < 0)
        LOG_ERROR("canardTxPoll failed with error: %d", poll);

    if (get_absolute_time() > node->next_heartbeat) {
        leos_cyphal_result_t result = leos_cyphal_publish_heartbeat(node);
        node->next_heartbeat = make_timeout_time_ms(HEARTBEAT_INTERVAL_MS);
    }
}

leos_cyphal_result_t leos_cyphal_publish_heartbeat(leos_cyphal_node_t *node) {
    uavcan_node_Heartbeat_1_0 msg = {
        .uptime = to_ms_since_boot(get_absolute_time()),
        .health = node->health,
        .mode = node->mode,
        .vendor_specific_status_code = 0,
    };

    uint8_t buf[uavcan_node_Heartbeat_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_];
    size_t size = sizeof(buf);

    if (uavcan_node_Heartbeat_1_0_serialize_(&msg, buf, &size))
        return LEOS_CYPHAL_SERIALIZATION_ERROR;

    const struct CanardPayload payload = {.data = buf, .size = size};

    struct CanardTransferMetadata md = {
        .port_id = uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_,
        .priority = CanardPriorityNominal,
        .transfer_id = node->heartbeat_tid++,
        .transfer_kind = CanardTransferKindMessage,
        .remote_node_id = CANARD_NODE_ID_UNSET
    };

    return leos_cyphal_push(node, &md, payload);
}

leos_cyphal_result_t leos_cyphal_push(leos_cyphal_node_t* node, const struct CanardTransferMetadata *meta, const struct CanardPayload payload) {
    uint64_t expired;
    int32_t r = canardTxPush(&node->txq, &node->can, to_us_since_boot(make_timeout_time_ms(500)), meta, payload, 0, &expired);
    return (r < 0) ? LEOS_CYPHAL_TX_ERROR : LEOS_CYPHAL_OK;
}


leos_cyphal_result_t leos_cyphal_subscribe(
    leos_cyphal_node_t* node, 
    const enum CanardTransferKind kind, 
    const CanardPortID port_id, 
    const size_t extent,
    leos_cyphal_rx_callback callback,
    void* user_reference 
) {
    if (node == NULL)
        return LEOS_CYPHAL_INVALID_ARGUMENT;
    leos_cyphal_subscription_t* s = node->can.memory.allocate(NULL, sizeof(leos_cyphal_subscription_t));
     if (s == NULL) {
        LOG_ERROR("The o1Heap instance for the cyphal node could not allocate memory for a subscription state");
        return LEOS_CYPHAL_OUT_OF_MEMORY;
    }
    s->callback = callback;
    s->user_ref = user_reference;
    s->sub.user_reference = s;

    int result = canardRxSubscribe(&node->can, kind, port_id, extent, CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC, &s->sub);
    if (result < 0) {
        return result; // pass thru error code
    }
    if (result == 0) {
        // old subscription updated
        // Should find subscription and free it
        for (int i = 0; i < count_of(node->subscriptions); i++) {
            if (node->subscriptions[i] == NULL)
                continue;
            if (node->subscriptions[i]->sub.port_id == port_id) {
                // Found the old subscription, should free it.
                node->can.memory.deallocate(NULL, sizeof(leos_cyphal_subscription_t), node->subscriptions[i]);
                // Replace this sub with new ptr.
                node->subscriptions[i] = NULL;
                break;
            }
        }
    }
    for (int i = 0; i < count_of(node->subscriptions); i++) {
        if (node->subscriptions[i] == NULL) {
            node->subscriptions[i] = s;
            return LEOS_CYPHAL_OK;
        }
    }
    return LEOS_CYPHAL_TOO_MANY_SUBSCRIPTIONS;
};


leos_cyphal_result_t leos_cyphal_unsubscribe(
    leos_cyphal_node_t* node, 
    const enum CanardTransferKind kind, 
    const CanardPortID port_id
) {
    if (node == NULL)
        return LEOS_CYPHAL_INVALID_ARGUMENT;


    int result = canardRxUnsubscribe(&node->can, kind, port_id);
    if (result < 0) {
        return result; // pass thru error code
    }
    if (result == 0) {
        // No subscription existed
        return LEOS_CYPHAL_OK;
    }

    // Should find subscription and free it
    for (int i = 0; i < count_of(node->subscriptions); i++) {
        if (node->subscriptions[i] == NULL)
            continue;
        if (node->subscriptions[i]->sub.port_id == port_id) {
            // Found the old subscription, should free it.
            node->can.memory.deallocate(NULL, sizeof(leos_cyphal_subscription_t), node->subscriptions[i]);
            // Replace this sub with new ptr.
            node->subscriptions[i] = NULL;
            return LEOS_CYPHAL_OK;
        }
    }
    return LEOS_CYPHAL_ERROR_UNKNOWN;
};