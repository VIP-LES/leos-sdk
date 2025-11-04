#include "MCP251XFD.h"
#include <stdio.h>

void mcp251xfd_debug_print_operation_mode(MCP251XFD* can)
{
    eMCP251XFD_OperationMode mode;
    eERRORRESULT result = MCP251XFD_GetActualOperationMode(can, &mode);
    if (result == ERR_OK) {
        printf("Current Operation Mode: ");
        switch (mode) {
        case MCP251XFD_NORMAL_CANFD_MODE:
            printf("Normal CAN-FD\n");
            break;
        case MCP251XFD_SLEEP_MODE:
            printf("Sleep\n");
            break;
        case MCP251XFD_INTERNAL_LOOPBACK_MODE:
            printf("Internal Loopback\n");
            break;
        case MCP251XFD_LISTEN_ONLY_MODE:
            printf("Listen Only\n");
            break;
        case MCP251XFD_CONFIGURATION_MODE:
            printf("Configuration\n");
            break;
        case MCP251XFD_EXTERNAL_LOOPBACK_MODE:
            printf("External Loopback\n");
            break;
        case MCP251XFD_NORMAL_CAN20_MODE:
            printf("Normal CAN 2.0\n");
            break;
        case MCP251XFD_RESTRICTED_OPERATION_MODE:
            printf("Restricted Operation\n");
            break;
        default:
            printf("Unknown\n");
            break;
        }
    } else {
        printf("Failed to get operation mode.\n");
    }
}

void mcp251xfd_debug_print_error_status(MCP251XFD* can)
{
    uint8_t tec, rec;
    eMCP251XFD_TXRXErrorStatus status;

    if (MCP251XFD_GetTransmitReceiveErrorCountAndStatus(can, &tec, &rec, &status) == ERR_OK) {
        printf("TEC: %3u | REC: %3u | Status: ", tec, rec);
        if (status & MCP251XFD_TX_BUS_OFF_STATE)
            printf("TX_BUS_OFF ");
        if (status & MCP251XFD_TX_BUS_PASSIVE_STATE)
            printf("TX_PASSIVE ");
        if (status & MCP251XFD_TX_WARNING_STATE)
            printf("TX_WARNING ");
        if (status & MCP251XFD_RX_BUS_PASSIVE_STATE)
            printf("RX_PASSIVE ");
        if (status & MCP251XFD_RX_WARNING_STATE)
            printf("RX_WARNING ");
        if (status == 0)
            printf("OK");
        printf("\n");
    }
}

const char * mcp251xfd_debug_error_reason(eERRORRESULT result) {
    return ERR_ErrorStrings[result];
}