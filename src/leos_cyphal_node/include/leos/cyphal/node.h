#pragma once
#include "leos/canbus.h"
#include "canard.h"
#include "pico/time.h"
#include <uavcan/node/Health_1_0.h>
#include <uavcan/node/Mode_1_0.h>


typedef struct leos_cyphal_node {
    // CAN transport this node uses (MCP, FDCAN, SocketCAN, etc.)
    leos_canbus_transport_t transport;

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

} leos_cyphal_node_t;