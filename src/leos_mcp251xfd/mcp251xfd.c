#include "leos/log.h"
#include "leos/mcp251xfd.h"
#include "leos/mcp251xfd/config.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include "platform.h"
#include <string.h>


eERRORRESULT leos_mcp251xfd_init(leos_mcp251xfd_t *device,
                                 const leos_mcp251xfd_config_t *config)
{
    if ((device == NULL) || (config == NULL))
        return ERR__PARAMETER_ERROR;
    // Pico requires an IRQ handler before a GPIO interrupt may be enabled.
    if (!irq_has_handler(IO_IRQ_BANK0))
        return ERR__CONFIGURATION;

    const leos_mcp251xfd_hw_t *hw = &device->hw;
    memset(&device->native, 0, sizeof(device->native));
    device->irq_pending = false;
    device->txq_full_logged = false;
    device->tx_error_logged = false;
    device->error_status_known = false;
    device->last_error_status = 0;
    MCP251XFD *native = &device->native;
    LOG_INFO("Initializing MCP251XFD");
    LOG_TRACE("SPI Block: %d", hw->spi);
    LOG_TRACE("SPI Baud: %d", hw->spi_baud);
    LOG_TRACE("MOSI: %d, MISO: %d, SCK: %d, CS: %d, IRQ: %d", hw->pin_mosi, hw->pin_miso, hw->pin_sck, hw->pin_cs, hw->pin_irq);

    // Setup IRQ
    gpio_init(hw->pin_irq);
    gpio_set_dir(hw->pin_irq, GPIO_IN);
    gpio_pull_up(hw->pin_irq);
    // Fill device with HAL configuration
    native->SPI_ChipSelect = hw->pin_cs;
    native->SPIClockSpeed = hw->spi_baud;
    native->InterfaceDevice = device;
    // HAL functions
    native->fnComputeCRC16 = MCP251XFD_ComputeCRC16_Pico;
    native->fnGetCurrentms = MCP251XFD_GetCurrentMs_Pico;
    native->fnSPI_Init = MCP251XFD_InterfaceInit_Pico;
    native->fnSPI_Transfer = MCP251XFD_InterfaceTransfer_Pico;
    // Set default GPIO level to 0 I think?
    native->GPIOsOutLevel = 0;

    // Variables filled by initialization
    uint32_t sysclk_speed;
    MCP251XFD_BitTimeStats bit_stats;

    MCP251XFD_Config mcp_config = {
        .SYSCLK_Result = &sysclk_speed,
        .BitTimeStats = &bit_stats,
        .GPIO0PinMode = MCP251XFD_PIN_AS_GPIO0_OUT,
        .GPIO1PinMode = MCP251XFD_PIN_AS_INT1_RX,
        .INTsOutMode = MCP251XFD_PINS_PUSHPULL_OUT,
        .TXCANOutMode = MCP251XFD_PINS_PUSHPULL_OUT,
    };
    mcp_config.XtalFreq = config->xtal_hz;
    mcp_config.OscFreq = config->osc_hz;
    mcp_config.SysclkConfig = config->sysclk_config;
    mcp_config.NominalBitrate = config->nominal_bitrate;
    mcp_config.DataBitrate = config->data_bitrate;
    mcp_config.Bandwidth = config->data_bitrate;
    mcp_config.ControlFlags = config->ctrl_flags;
    mcp_config.SysInterruptFlags = config->irq_flags;

    // Delay at least 3 more ms before initialization to configure chip clock
    sleep_ms(3);

    eERRORRESULT result = Init_MCP251XFD(native, &mcp_config);
    if (result == ERR__FREQUENCY_ERROR) {
        // A frequency error is unrecoverable.
        // The out of range frequency value will be stored in
        // `sysclk_speed`. In the future, it would be nice to log this error.

        // For now, we do nothing and the sysclk_result will get dropped.
        LOG_ERROR("Unrecoverable frequency error. SYSCLK out of range: %lu Hz", sysclk_speed);
        leos_mcp251xfd_deinit(device);
        return ERR__FREQUENCY_ERROR;
    } else if (result != ERR_OK) {
        LOG_ERROR("Failed to initialize MCP251XFD, error: %d", result);
        leos_mcp251xfd_deinit(device);
        return result;
    }

    // Setup timestamp
    eERRORRESULT timestamp_result = MCP251XFD_ConfigureTimeStamp(native,
        true,
        MCP251XFD_TS_CAN20_SOF_CANFD_SOF, // capture on SOF of every frame
        40, // prescaler (40 = 1 µs ticks @ 40 MHz SYSCLK)
        false // We aren't using timestamp super heavily, so no need to interrupt on timer rollover
    );
    if (timestamp_result != ERR_OK) {
        leos_mcp251xfd_deinit(device);
        return timestamp_result;
    }

    eERRORRESULT fifo_result = MCP251XFD_ConfigureFIFOList(native, (MCP251XFD_FIFO*) config->fifo, config->num_fifos);
    if (fifo_result != ERR_OK) {
        LOG_ERROR("MCP251XFD Instance failed to configure FIFOs");
        leos_mcp251xfd_deinit(device);
        return fifo_result;
    }

    eERRORRESULT filter_result = MCP251XFD_ConfigureFilterList(native, MCP251XFD_D_NET_FILTER_DISABLE, (MCP251XFD_Filter*) config->filter, config->num_filters);
    if (filter_result != ERR_OK) {
        LOG_ERROR("MCP251XFD Instance failed to configure Filters");
        leos_mcp251xfd_deinit(device);
        return filter_result;
    }

    // Opting to not setup sleep mode yet.

    // Start chip in CAN-FD mode. Configuration is closed.
    result = MCP251XFD_RequestOperationMode(native, config->initial_mode, true);
    if (result != ERR_OK) {
        leos_mcp251xfd_deinit(device);
        return result;
    }

    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_irq_enabled(hw->pin_irq, GPIO_IRQ_EDGE_FALL, true);
    if (!gpio_get(hw->pin_irq))
        leos_mcp251xfd_irq_handler(device);
    return result;
}

void leos_mcp251xfd_deinit(leos_mcp251xfd_t *dev) {
    if (!dev) return;
    gpio_set_irq_enabled(dev->hw.pin_irq, GPIO_IRQ_EDGE_FALL, false);
    dev->irq_pending = false;
    dev->txq_full_logged = false;
    dev->tx_error_logged = false;
    dev->error_status_known = false;
    dev->native.InterfaceDevice = NULL;
}

static void log_can_error_state(leos_mcp251xfd_t *dev, bool bus_error_event)
{
    uint8_t tx_errors = 0;
    uint8_t rx_errors = 0;
    eMCP251XFD_TXRXErrorStatus status;
    eERRORRESULT result = MCP251XFD_GetTransmitReceiveErrorCountAndStatus(
        &dev->native, &tx_errors, &rx_errors, &status);
    if (result != ERR_OK) {
        LOG_ERROR("MCP251XFD: could not read CAN error state: %d", result);
        return;
    }

    const uint8_t flags = (uint8_t)status;
    if (!dev->error_status_known || (flags != dev->last_error_status)) {
        if (flags & MCP251XFD_TX_BUS_OFF_STATE)
            LOG_ERROR("CAN bus off (TEC=%u REC=%u)", (unsigned)tx_errors, (unsigned)rx_errors);
        else if (flags & (MCP251XFD_TX_BUS_PASSIVE_STATE | MCP251XFD_RX_BUS_PASSIVE_STATE))
            LOG_WARNING("CAN error passive (status=0x%02x TEC=%u REC=%u)",
                        (unsigned)flags, (unsigned)tx_errors, (unsigned)rx_errors);
        else if (flags & (MCP251XFD_TX_WARNING_STATE | MCP251XFD_RX_WARNING_STATE |
                          MCP251XFD_TX_RX_WARNING_STATE))
            LOG_DEBUG("CAN error warning (status=0x%02x TEC=%u REC=%u)",
                      (unsigned)flags, (unsigned)tx_errors, (unsigned)rx_errors);
        else if (dev->error_status_known && dev->last_error_status != 0u)
            LOG_INFO("CAN error state recovered (TEC=%u REC=%u)",
                     (unsigned)tx_errors, (unsigned)rx_errors);
        else if (bus_error_event)
            LOG_DEBUG("CAN bus error (TEC=%u REC=%u)", (unsigned)tx_errors, (unsigned)rx_errors);
        else
            LOG_TRACE("CAN error state nominal (TEC=%u REC=%u)",
                      (unsigned)tx_errors, (unsigned)rx_errors);
        dev->last_error_status = flags;
        dev->error_status_known = true;
    } else {
        LOG_TRACE("CAN bus error state unchanged (status=0x%02x TEC=%u REC=%u)",
                  (unsigned)flags, (unsigned)tx_errors, (unsigned)rx_errors);
    }
}

static uint8_t payload_length_to_dlc(size_t size)
{
    if (size <= 8) return (uint8_t)size;
    if (size <= 12) return 9;
    if (size <= 16) return 10;
    if (size <= 20) return 11;
    if (size <= 24) return 12;
    if (size <= 32) return 13;
    if (size <= 48) return 14;
    return 15;
}

eERRORRESULT leos_mcp251xfd_send(leos_mcp251xfd_t *dev, uint32_t can_id,
                                 const uint8_t *data, size_t size)
{
    if ((dev == NULL) || (dev->native.InterfaceDevice != dev) ||
        (can_id > 0x1FFFFFFFu) || (size > 64u) || ((size > 0u) && (data == NULL)))
        return ERR__PARAMETER_ERROR;

    eMCP251XFD_TXQstatus status;
    eERRORRESULT result = MCP251XFD_GetTXQStatus(&dev->native, &status);
    if (result != ERR_OK) {
        if (!dev->tx_error_logged)
            LOG_ERROR("MCP251XFD: TXQ status read failed: %d", result);
        dev->tx_error_logged = true;
        return result;
    }
    if (!(status & MCP251XFD_TXQ_NOT_FULL)) {
        if (!dev->txq_full_logged) {
            LOG_DEBUG("CAN TXQ full; frame 0x%08lx will be retried", (unsigned long)can_id);
            log_can_error_state(dev, false);
            dev->txq_full_logged = true;
        }
        return ERR__BUFFER_FULL;
    }
    if (dev->txq_full_logged) {
        LOG_DEBUG("CAN TXQ has room again");
        dev->txq_full_logged = false;
        if (dev->error_status_known && dev->last_error_status != 0u)
            log_can_error_state(dev, false);
    }

    uint8_t payload[64] = {0};
    if (size > 0u) memcpy(payload, data, size);
    MCP251XFD_CANMessage message = {
        .MessageID = can_id,
        .DLC = (eMCP251XFD_DataLength)payload_length_to_dlc(size),
        .ControlFlags = MCP251XFD_CANFD_FRAME | MCP251XFD_EXTENDED_MESSAGE_ID,
        .PayloadData = payload,
    };
    result = MCP251XFD_TransmitMessageToTXQ(&dev->native, &message, true);
    if (result == ERR_OK) {
        if (dev->tx_error_logged) {
            LOG_INFO("MCP251XFD: TXQ send recovered");
            dev->tx_error_logged = false;
        }
        LOG_TRACE("CAN TX queued id=0x%08lx bytes=%u", (unsigned long)can_id,
                  (unsigned)MCP251XFD_DLCToByte(message.DLC, true));
    } else {
        if (!dev->tx_error_logged)
            LOG_ERROR("MCP251XFD: TXQ write failed: %d", result);
        dev->tx_error_logged = true;
    }
    return result;
}

eERRORRESULT leos_mcp251xfd_receive(leos_mcp251xfd_t *dev, uint32_t *can_id,
                                    uint8_t *data, size_t capacity,
                                    size_t *size, uint32_t *timestamp_us)
{
    if ((dev == NULL) || (dev->native.InterfaceDevice != dev) ||
        (can_id == NULL) || (data == NULL) || (capacity < 64u) || (size == NULL))
        return ERR__PARAMETER_ERROR;

    eMCP251XFD_FIFOstatus status;
    eERRORRESULT result = MCP251XFD_GetFIFOStatus(&dev->native, MCP251XFD_FIFO1, &status);
    if (result != ERR_OK) {
        LOG_ERROR("MCP251XFD: FIFO1 status read failed: %d", result);
        return result;
    }
    if (!(status & MCP251XFD_RX_FIFO_NOT_EMPTY)) return ERR__NO_DATA_AVAILABLE;

    MCP251XFD_CANMessage message = {.PayloadData = data};
    uint32_t timestamp = 0;
    result = MCP251XFD_ReceiveMessageFromFIFO(&dev->native, &message,
                                               MCP251XFD_PAYLOAD_64BYTE,
                                               &timestamp, MCP251XFD_FIFO1);
    if (result != ERR_OK) {
        LOG_ERROR("MCP251XFD: FIFO1 read failed: %d", result);
        return result;
    }
    *can_id = message.MessageID;
    *size = MCP251XFD_DLCToByte(message.DLC,
                                (message.ControlFlags & MCP251XFD_CANFD_FRAME) != 0);
    if (timestamp_us != NULL) *timestamp_us = timestamp;
    LOG_TRACE("CAN RX id=0x%08lx bytes=%u timestamp_us=%lu",
              (unsigned long)*can_id, (unsigned)*size, (unsigned long)timestamp);
    return ERR_OK;
}


void leos_mcp251xfd_irq_handler(leos_mcp251xfd_t *dev) {
    if (dev == NULL) return;
    dev->irq_pending = true;
}

bool leos_mcp251xfd_handle_gpio_irq(leos_mcp251xfd_t *dev, uint gpio,
                                   uint32_t event_mask) {
    if ((dev == NULL) || (gpio != dev->hw.pin_irq) ||
        !(event_mask & GPIO_IRQ_EDGE_FALL))
        return false;
    leos_mcp251xfd_irq_handler(dev);
    return true;
}

void leos_mcp251xfd_task(leos_mcp251xfd_t *dev) {
    if (!dev)
        return;

    // Only run when an IRQ was latched by the GPIO handler.
    if (!dev->irq_pending)
        return;
    dev->irq_pending = false;
    MCP251XFD *native = &dev->native;
    LOG_TRACE("MCP251XFD: servicing IRQ");

    eERRORRESULT err;
    eMCP251XFD_InterruptFlagCode code;

    // A stuck level interrupt must not monopolize the application poll loop.
    for (unsigned serviced = 0; serviced < 32u; serviced++)
    {
        err = MCP251XFD_GetCurrentInterruptEvent(native, &code);
        if (err != ERR_OK)
        {
            LOG_ERROR("GetCurrentInterruptEvent failed: %d", err);
            dev->irq_pending = true;
            return;
        }
        if (code == MCP251XFD_NO_INTERRUPT)
        {
            LOG_TRACE("No int anymore");
            // All clear
            return;
        }

        LOG_TRACE("IRQ event: %d", code);

        switch (code)
        {
        case MCP251XFD_ERROR_INTERRUPT:
        {
            log_can_error_state(dev, true);
            err = MCP251XFD_ClearInterruptEvents(native, MCP251XFD_INT_BUS_ERROR_EVENT);
            if (err != ERR_OK) LOG_ERROR("MCP251XFD: clearing bus error IRQ failed: %d", err);
            break;
        }

        case MCP251XFD_WAKEUP_INTERRUPT:
        {
            MCP251XFD_ClearInterruptEvents(native, MCP251XFD_INT_BUS_WAKEUP_EVENT);
            break;
        }

        case MCP251XFD_OPMODE_CHANGE_OCCURED:
        {
            eMCP251XFD_OperationMode op;
            if (MCP251XFD_GetActualOperationMode(native, &op) == ERR_OK)
            {
                LOG_DEBUG("OPMODE -> %d", op);
            }
            MCP251XFD_ClearInterruptEvents(native, MCP251XFD_INT_OPERATION_MODE_CHANGE_EVENT);
            break;
        }

        case MCP251XFD_TXQ_INTERRUPT:
        case MCP251XFD_TRANSMIT_ATTEMPT:
        {
            eMCP251XFD_TXQstatus qst;
            err = MCP251XFD_GetTXQStatus(native, &qst);
            if (err != ERR_OK)
            {
                LOG_ERROR("GetTXQStatus failed: %d", err);
                dev->irq_pending = true;
                return;
            }
            if (qst & MCP251XFD_TXQ_ATTEMPTS_EXHAUSTED) {
                LOG_WARNING("CAN TXQ attempts exhausted (status=0x%02x); resetting TXQ and dropping queued frames",
                            (unsigned)qst);
                log_can_error_state(dev, true);
                err = MCP251XFD_ResetTXQ(native);
                if (err != ERR_OK) LOG_ERROR("MCP251XFD: TXQ reset failed: %d", err);
            }
            // CiINT.TXIF is derived from TXQ status; clear its source bits.
            // NOT_FULL and EMPTY are level bits and cannot be cleared here.
            const uint8_t clearable = (uint8_t)qst &
                (MCP251XFD_TXQ_ATTEMPTS_EXHAUSTED | MCP251XFD_TXQ_BUS_ERROR |
                 MCP251XFD_TXQ_ARBITRATION_LOST | MCP251XFD_TXQ_ABORTED);
            if (clearable != 0u) {
                err = MCP251XFD_ClearFIFOEvents(native, MCP251XFD_TXQ, clearable);
                if (err != ERR_OK) {
                    LOG_ERROR("MCP251XFD: clearing TXQ events failed: %d", err);
                    dev->irq_pending = true;
                    return;
                }
            } else {
                // Recheck after the receive path drains FIFO1. A level bit
                // can keep IRQ low without another falling edge.
                dev->irq_pending = true;
                return;
            }
            break;
        }

        case MCP251XFD_FIFO1_INTERRUPT:
        {
            // The receive path owns FIFO1. Revisit other interrupt causes
            // after it drains the FIFO, even if the IRQ line stays low.
            dev->irq_pending = true;
            return;
        }

        default:
        {
            // Clear any clearable system bits to avoid stalls
            MCP251XFD_ClearInterruptEvents(native,
                                           MCP251XFD_INT_CLEARABLE_FLAGS_MASK);
            LOG_TRACE("Unhandled event %d cleared (mask=CiINT clearables)", code);
            break;
        }
        }
    }
    // More causes remain; resume on the next poll without waiting for an edge.
    dev->irq_pending = true;
}
