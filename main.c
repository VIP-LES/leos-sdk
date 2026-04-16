#include "leos/log.h"
#include "config.h"
#include "leos/mcp251xfd.h"
#include "leos/cyphal/transport/mcp251xfd.h"
#include "leos/mcp251xfd/debug.h"
#include "leos/cyphal/node.h"
#include "pico/stdlib.h"

#include <uavcan/node/Heartbeat_1_0.h>

void mcp_read_pending_cb(MCP251XFD* dev, void *node_ref) {
    leos_cyphal_node_t *node = (leos_cyphal_node_t *)node_ref;
    leos_cyphal_rx_process(node);
}

void onHeartbeat(struct CanardRxTransfer *transfer, void* ref) {
    (void)ref;

    uavcan_node_Heartbeat_1_0 hb;
    int size = transfer->payload.size;
    if(!uavcan_node_Heartbeat_1_0_deserialize_(&hb, transfer->payload.data, &size)) {
        LOG_INFO("Received Heartbeat: Node#%d - Health=%d, Mode=%d", transfer->metadata.remote_node_id, hb.health, hb.mode);
    } else {
        LOG_WARNING("Failed to deserialize a heartbeat from Node#%d", transfer->metadata.remote_node_id);
    }
}

int main()
{
    // --- INITIALIZE MODULE ---
    leos_log_init_console(ULOG_TRACE_LEVEL);
    while (!stdio_usb_connected())
        tight_loop_contents();
    LOG_INFO("USB STDIO Connected. Starting application.");

    MCP251XFD dev;
    eERRORRESULT mcp_result = leos_mcp251xfd_init(&dev, &can_hw_config, &can_config, true);
    if (mcp_result != ERR_OK) {
        printf("%s\n", mcp251xfd_debug_error_reason(mcp_result));
        while(true)
            tight_loop_contents();
    }
    leos_cyphal_transport_t transport = leos_cyphal_transport_mcp251xfd(&dev);

    leos_cyphal_node_t node;
    leos_cyphal_result_t can_result = leos_cyphal_init(&node, transport, CAN_ID_SENSOR_MODULE);
    leos_mcp251xfd_set_rx_handler(&dev, mcp_read_pending_cb, &node);


    leos_cyphal_result_t result = leos_cyphal_subscribe(&node, 
        CanardTransferKindMessage, 
        uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_, 
        uavcan_node_Heartbeat_1_0_EXTENT_BYTES_, 
        onHeartbeat, 
        NULL
    );
    if (result == LEOS_CYPHAL_OK) {
        LOG_INFO("Subscribed to heartbeat");
    } else {
        LOG_WARNING("Couldn't subscribe to heartbeat: %d", result);
    }

    // After finishing initialization, set our mode to operational
    node.mode.value = uavcan_node_Mode_1_0_OPERATIONAL;

    // --- MAIN LOOP ---
    LOG_INFO("Entering main loop...");
    while (true)
    {   
        leos_mcp251xfd_task(&dev);
        leos_cyphal_task(&node);

        // Your looping code goes here
    }
}