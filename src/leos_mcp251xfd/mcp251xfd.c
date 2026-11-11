#include "leos/log.h"
#include "leos/mcp251xfd.h"
#include "leos/mcp251xfd/config.h"
#include "pico/stdlib.h"
#include "platform.h"
#include <string.h>


eERRORRESULT leos_mcp251xfd_init(leos_mcp251xfd_t *device,
                                 const leos_mcp251xfd_config_t *config)
{
    if ((device == NULL) || (config == NULL))
        return ERR__PARAMETER_ERROR;

    const leos_mcp251xfd_hw_t *hw = &device->hw;
    memset(&device->native, 0, sizeof(device->native));
    device->irq_pending = false;
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
    if (result != ERR_OK)
        leos_mcp251xfd_deinit(device);
    return result;
}

void leos_mcp251xfd_deinit(leos_mcp251xfd_t *dev) {
    if (!dev) return;
    dev->irq_pending = false;
    dev->native.InterfaceDevice = NULL;
}


void leos_mcp251xfd_irq_handler(leos_mcp251xfd_t *dev) {
    if (dev == NULL) return;
    dev->irq_pending = true;
}

void leos_mcp251xfd_task(leos_mcp251xfd_t *dev) {
    if (!dev)
        return;

    // Only run when an IRQ was latched by the GPIO handler.
    if (!dev->irq_pending)
        return;
    dev->irq_pending = false;
    MCP251XFD *native = &dev->native;
    LOG_DEBUG("MCP251XFD: servicing IRQ");

    eERRORRESULT err;
    eMCP251XFD_InterruptFlagCode code;

    // Keep fetching the highest-priority pending event until none remain.
    while(true)
    {
        err = MCP251XFD_GetCurrentInterruptEvent(native, &code);
        if (err != ERR_OK)
        {
            LOG_ERROR("GetCurrentInterruptEvent failed: %d", err);
            break;
        }
        if (code == MCP251XFD_NO_INTERRUPT)
        {
            LOG_TRACE("No int anymore");
            // All clear
            break;
        }

        LOG_TRACE("IRQ event: %d", code);

        switch (code)
        {
        case MCP251XFD_ERROR_INTERRUPT:
        {
            LOG_WARNING("There was an error on the CAN bus. Most likely no other devices are connected");
            eMCP251XFD_TXRXErrorStatus bus_status;
            MCP251XFD_GetTransmitReceiveErrorStatus(native, &bus_status);
            LOG_DEBUG("Bus Error: %d", bus_status);


            MCP251XFD_ClearInterruptEvents(native, MCP251XFD_INT_BUS_ERROR_EVENT);
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
        {
            // Clear any TXQ-related FIFO events, then the global TX event.
            eMCP251XFD_TXQstatus qst;
            err = MCP251XFD_GetTXQStatus(native, &qst);
            if (err != ERR_OK)
            {
                LOG_ERROR("GetTXQStatus failed: %d", err);
                break;
            }
            if (qst & MCP251XFD_TXQ_ATTEMPTS_EXHAUSTED) {
                MCP251XFD_ResetTXQ(native);
                // MCP251XFD_ClearTXQAttemptsEvent(dev);
            }
            // MCP251XFD_ClearInterruptEvents(dev, MCP251XFD_INT_TX_EVENT);
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
}
