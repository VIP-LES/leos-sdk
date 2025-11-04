#include "leos/log.h"
#include "leos/canbus/mcp251xfd.h"
#include "canard.h"
#include <stdint.h>

static inline uint8_t bytes_to_dlc(uint8_t n)
{
    if (n <= 8)
        return n;
    if (n <= 12)
        return 9;
    if (n <= 16)
        return 10;
    if (n <= 20)
        return 11;
    if (n <= 24)
        return 12;
    if (n <= 32)
        return 13;
    if (n <= 48)
        return 14;
    return 15; // 64 bytes
}

int8_t canard_mcp_tx_frame(void* const mcp251xfd, const CanardMicrosecond deadline_usec, struct CanardMutableFrame* const frame)
{
    (void)deadline_usec; // Not currently using the deadline. a timestamp for the frame will be injected by the MCP251XFD
    MCP251XFD* dev = (MCP251XFD*)mcp251xfd;

    MCP251XFD_CANMessage msg = {
        .MessageID = frame->extended_can_id,
        .DLC = bytes_to_dlc(frame->payload.size),
        .ControlFlags = MCP251XFD_CANFD_FRAME | MCP251XFD_EXTENDED_MESSAGE_ID,
        .PayloadData = (uint8_t*)frame->payload.data
    };

    eERRORRESULT result;
    eMCP251XFD_TXQstatus txq_status;
    result = MCP251XFD_GetTXQStatus(dev, &txq_status);
    if (result != ERR_OK)
        return -result; // negate MCP error to make negative

    if (!(txq_status & MCP251XFD_TX_FIFO_NOT_FULL)) {
        LOG_WARNING("CAN TX FIFO is full. Frame transmission delayed.");
        return 0; // Transmit FIFO is full. Hold off on this frame.
    }

    // There are arguments to be made that this should not be flushed immediately, and instead a flush should occur
    // inside the mainloop or other service routine. It works for now, but may not be efficient.
    result = MCP251XFD_TransmitMessageToTXQ(dev, &msg, true);
    if (result != ERR_OK) {
        LOG_ERROR("MCP251XFD_TransmitMessageToTXQ failed with code: %d", result);
        return -result;
    }
    return 1; // SUCCESS

    // Possible return values:
    // Zero - Could not send frame yet, but didn't fail. This would be used if the TXQ is full
    // Positive - Success
    // Negative - Failure, all frames for this transport will be dropped.
}

uint32_t canard_mcp_rx_process(void* dev, struct CanardInstance* can)
{
    uint32_t frame_count = 0;
    eERRORRESULT result;
    eMCP251XFD_FIFOstatus rx_status;
    // FIFO1 is hardcoded to a single RX fifo. This should be drawn from config, somehow
    // This would require this port having a config struct.
    result = MCP251XFD_GetFIFOStatus(dev, MCP251XFD_FIFO1, &rx_status);
    if (result != ERR_OK)
        return result;

    if (!(rx_status & MCP251XFD_RX_FIFO_NOT_EMPTY))
        return result;

    do {
        MCP251XFD_CANMessage msg;
        uint8_t payload[64];
        uint32_t timestamp_us = 0;
        msg.PayloadData = payload;

        result = MCP251XFD_ReceiveMessageFromFIFO(dev, &msg, MCP251XFD_PAYLOAD_64BYTE, &timestamp_us, MCP251XFD_FIFO1);
        if (result != ERR_OK) {
            LOG_ERROR("Failed to receive message from FIFO, error: %d", result); // <-- New log
            break;
        }

        frame_count++;

        struct CanardFrame rx_frame = {
            .extended_can_id = msg.MessageID,
            .payload = {
                .data = msg.PayloadData,
                .size = MCP251XFD_DLCToByte(msg.DLC, true) }
        };

        struct CanardRxTransfer transfer;
        struct CanardRxSubscription* subscription;
        result = canardRxAccept(can, timestamp_us, &rx_frame, 0, &transfer, &subscription);

        if (result == 1) {
            // call subscription handler
            LOG_INFO("Accepted CAN transfer for port_id %d", transfer.metadata.port_id);
            leos_canbus_subcription_callback_t* callback = (leos_canbus_subcription_callback_t*)subscription->user_reference;
            callback->handler(&transfer, callback->user_reference);

            can->memory.deallocate(can->memory.user_reference, transfer.payload.allocated_size, transfer.payload.data);
        } else if (result < 0) {
            // An error has occurred: either an argument is invalid or we've ran out of memory.
            // It is possible to statically prove that an out-of-memory will never occur for a given application if
            // the heap is sized correctly; for background, refer to the Robson's Proof and the documentation for O1Heap.
            // Reception of an invalid frame is NOT an error.
            LOG_ERROR("canardRxAccept failed with error: %d. Possible OOM.", result);
            break;
        }

        // Check if more frames are available
        result = MCP251XFD_GetFIFOStatus(dev, MCP251XFD_FIFO1, &rx_status);
        if (result != ERR_OK)
            LOG_ERROR("canardRxAccept failed with error: %d. Possible OOM.", result);
        break;

    } while (rx_status & MCP251XFD_RX_FIFO_NOT_EMPTY);
    if (frame_count > 0) {
        LOG_INFO("Processed %lu incoming CAN frames.", frame_count);
    }
    return result;
}

leos_canbus_transport_t leos_canbus_transport_mcp251xfd(MCP251XFD *dev) {
    leos_canbus_transport_t t = {
        .tx_frame = canard_mcp_tx_frame,
        .rx_process = canard_mcp_rx_process,
        .user_reference = dev
    };
    return t;
}