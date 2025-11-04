#include "leos/cyphal.h"
#include "leos/canbus.h"
#include "leos/log.h"
#include "leos/cyphal/memory.h"
#include "pico/stdlib.h"
#include "canard.h"
#include <uavcan/node/Heartbeat_1_0.h>

int8_t tx_callback(void *const node_ref, const CanardMicrosecond deadline_usec, struct CanardMutableFrame *const frame) {
    leos_cyphal_node_t *node = (leos_cyphal_node_t*) node_ref;
    leos_canbus_transport_t transport = node->transport;
    CanardTxFrameHandler tx = transport.tx_frame;
    return tx(transport.user_reference, deadline_usec, frame);
}

leos_cyphal_result_t leos_cyphal_init(leos_cyphal_node_t *node, leos_canbus_transport_t transport, uint8_t node_id) {
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

void leos_cyphal_task(leos_cyphal_node_t *node) {
    if (node->rx_pending) {
        node->transport.rx_process(node->transport.user_reference, &node->can);
        node->rx_pending = false;
    }

    int poll = canardTxPoll(&node->txq, &node->can, to_us_since_boot(get_absolute_time()), node, tx_callback, NULL, NULL);
    if (poll < 0)
        LOG_ERROR("canardTxPoll failed with error: %d", poll);

    if (get_absolute_time() > node->next_heartbeat) {
        leos_cyphal_result_t result = leos_cyphal_publish_heartbeat(node);
        node->next_heartbeat = make_timeout_time_ms(HEARTBEAT_INTERVAL_MS);
    }
}

leos_cyphal_result_t leos_cyphal_push(leos_cyphal_node_t* node, const struct CanardTransferMetadata *meta, const struct CanardPayload payload) {
    uint64_t expired;
    int32_t r = canardTxPush(&node->txq, &node->can, to_us_since_boot(make_timeout_time_ms(500)), meta, payload, 0, &expired);
    return (r < 0) ? LEOS_CYPHAL_TX_ERROR : LEOS_CYPHAL_OK;
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
