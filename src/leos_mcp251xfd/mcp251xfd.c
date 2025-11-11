#include "leos/log.h"
#include "leos/mcp251xfd.h"
#include "leos/mcp251xfd/config.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "platform.h"
#include <string.h>


// Dynamically allocated struct storing config info and state for the driver
typedef struct {
    const leos_mcp251xfd_hw_t *hw;
    const leos_mcp251xfd_config_t *cfg;
    volatile bool irq_pending;
    leos_mcp251xfd_rx_cb rx_cb;
    void* cb_user_reference;
} leos_mcp251xfd_ctx_t;

// Lookup table for an initialized controller by its interrupt pin
static leos_mcp251xfd_ctx_t *s_ctx_by_gpio[48] = {0};

eERRORRESULT leos_mcp251xfd_init(MCP251XFD *device, const leos_mcp251xfd_hw_t *hw, const leos_mcp251xfd_config_t *config, bool attachInterrupt)
{
    memset(device, 0, sizeof(MCP251XFD));
    LOG_INFO("Initializing MCP251XFD");
    LOG_TRACE("SPI Block: %d", hw->spi);
    LOG_TRACE("SPI Baud: %d", hw->spi_baud);
    LOG_TRACE("MOSI: %d, MISO: %d, SCK: %d, CS: %d, IRQ: %d", hw->pin_mosi, hw->pin_miso, hw->pin_sck, hw->pin_cs, hw->pin_irq);

    // Setup a context struct to store in the user reference
    leos_mcp251xfd_ctx_t* ctx = (leos_mcp251xfd_ctx_t*) malloc(sizeof(leos_mcp251xfd_ctx_t));
    if (ctx == NULL) {
        LOG_ERROR("leos_mcp251xfd: Could not malloc for the context struct");
        return ERR__OUT_OF_MEMORY;
    }
    ctx->hw = hw;
    ctx->cfg = config;
    ctx->irq_pending = false;
    ctx->rx_cb = NULL;
    ctx->cb_user_reference = NULL;
    device->UserDriverData = (void *)ctx;

    // Setup IRQ
    gpio_init(hw->pin_irq);
    gpio_set_dir(hw->pin_irq, GPIO_IN);
    gpio_pull_up(hw->pin_irq);
    if (attachInterrupt) {
        leos_mcp251xfd_attach_gpio_interrupt();
    }
    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_irq_enabled(hw->pin_irq, GPIO_IRQ_EDGE_FALL, true);

    // Store the config in a array map to the irq gpio
    if (hw->pin_irq >= count_of(s_ctx_by_gpio)) {
        LOG_ERROR("MCP251XFD IRQ pin out of range for RP2350 Limits (48)");
        return ERR__OUT_OF_RANGE;
    }
    s_ctx_by_gpio[hw->pin_irq] = ctx;


    // Fill device with HAL configuration
    device->SPI_ChipSelect = hw->pin_cs;
    device->SPIClockSpeed = hw->spi_baud;
    device->InterfaceDevice = (void*)hw;
    // HAL functions
    device->fnComputeCRC16 = MCP251XFD_ComputeCRC16_Pico;
    device->fnGetCurrentms = MCP251XFD_GetCurrentMs_Pico;
    device->fnSPI_Init = MCP251XFD_InterfaceInit_Pico;
    device->fnSPI_Transfer = MCP251XFD_InterfaceTransfer_Pico;
    // Set default GPIO level to 0 I think?
    device->GPIOsOutLevel = 0;

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

    eERRORRESULT result = Init_MCP251XFD(device, &mcp_config);
    if (result == ERR__FREQUENCY_ERROR) {
        // A frequency error is unrecoverable.
        // The out of range frequency value will be stored in
        // `sysclk_speed`. In the future, it would be nice to log this error.

        // For now, we do nothing and the sysclk_result will get dropped.
        LOG_ERROR("Unrecoverable frequency error. SYSCLK out of range: %lu Hz", sysclk_speed);
        return ERR__FREQUENCY_ERROR;
    } else if (result != ERR_OK) {
        LOG_ERROR("Failed to initialize MCP251XFD, error: %d", result);
        return result;
    }

    // Setup timestamp
    eERRORRESULT timestamp_result = MCP251XFD_ConfigureTimeStamp(device,
        true,
        MCP251XFD_TS_CAN20_SOF_CANFD_SOF, // capture on SOF of every frame
        40, // prescaler (40 = 1 µs ticks @ 40 MHz SYSCLK)
        false // We aren't using timestamp super heavily, so no need to interrupt on timer rollover
    );
    if (timestamp_result != ERR_OK) {
        return timestamp_result;
    }

    eERRORRESULT fifo_result = MCP251XFD_ConfigureFIFOList(device, (MCP251XFD_FIFO*) config->fifo, config->num_fifos);
    if (fifo_result != ERR_OK) {
        LOG_ERROR("MCP251XFD Instance failed to configure FIFOs");
        return fifo_result;
    }

    eERRORRESULT filter_result = MCP251XFD_ConfigureFilterList(device, MCP251XFD_D_NET_FILTER_DISABLE, (MCP251XFD_Filter*) config->filter, config->num_filters);
    if (filter_result != ERR_OK) {
        LOG_ERROR("MCP251XFD Instance failed to configure Filters");
        return filter_result;
    }

    // Opting to not setup sleep mode yet.

    // Start chip in CAN-FD mode. Configuration is closed.
    return MCP251XFD_RequestOperationMode(device, config->initial_mode, true);
}

void leos_mcp251xfd_deinit(MCP251XFD *dev) {
    if (!dev) return;
    leos_mcp251xfd_ctx_t *ctx = (leos_mcp251xfd_ctx_t*) dev->UserDriverData;
    if (!ctx) return;

    gpio_set_irq_enabled(ctx->hw->pin_irq, GPIO_IRQ_EDGE_FALL, false);
    s_ctx_by_gpio[ctx->hw->pin_irq] = NULL;

    free(ctx);
    dev->UserDriverData = NULL;
}


void leos_mcp251xfd_irq_handler(uint gpio, uint32_t events) {
    if (!(events & GPIO_IRQ_EDGE_FALL)) return;

    if (gpio >= count_of(s_ctx_by_gpio)) return;
    leos_mcp251xfd_ctx_t* ctx = s_ctx_by_gpio[gpio];
    if (!ctx) return;
    ctx->irq_pending = true;
}

static void gpio_cb(uint gpio, uint32_t events) {
    leos_mcp251xfd_irq_handler(gpio, events);
}
void leos_mcp251xfd_attach_gpio_interrupt() {
    LOG_DEBUG("Binding this core's GPIO irq handler to the MCP251XFD driver");
    gpio_set_irq_callback(gpio_cb);
}

void leos_mcp251xfd_set_rx_handler(MCP251XFD *dev, leos_mcp251xfd_rx_cb callback, void *user_reference) {
    if (!dev) return;
    leos_mcp251xfd_ctx_t *ctx = (leos_mcp251xfd_ctx_t*) dev->UserDriverData;
    if (!ctx) return;

    ctx->rx_cb = callback;
    ctx->cb_user_reference = user_reference;
}

void leos_mcp251xfd_task(MCP251XFD *dev) {
    if (!dev)
        return;
    leos_mcp251xfd_ctx_t *ctx = (leos_mcp251xfd_ctx_t *)dev->UserDriverData;
    if (!ctx)
        return;

    // Only run when an IRQ was latched by the GPIO handler.
    if (!ctx->irq_pending)
        return;
    ctx->irq_pending = false;
    LOG_DEBUG("MCP251XFD: servicing IRQ");

    eERRORRESULT err;
    eMCP251XFD_InterruptFlagCode code;

    // Keep fetching the highest-priority pending event until none remain.
    while(true)
    {
        err = MCP251XFD_GetCurrentInterruptEvent(dev, &code);
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
            MCP251XFD_GetTransmitReceiveErrorStatus(dev, &bus_status);
            LOG_DEBUG("Bus Error: %d", bus_status);


            MCP251XFD_ClearInterruptEvents(dev, MCP251XFD_INT_BUS_ERROR_EVENT);
            break;
        }

        case MCP251XFD_WAKEUP_INTERRUPT:
        {
            MCP251XFD_ClearInterruptEvents(dev, MCP251XFD_INT_BUS_WAKEUP_EVENT);
            break;
        }

        case MCP251XFD_OPMODE_CHANGE_OCCURED:
        {
            eMCP251XFD_OperationMode op;
            if (MCP251XFD_GetActualOperationMode(dev, &op) == ERR_OK)
            {
                LOG_DEBUG("OPMODE -> %d", op);
            }
            MCP251XFD_ClearInterruptEvents(dev, MCP251XFD_INT_OPERATION_MODE_CHANGE_EVENT);
            break;
        }

        case MCP251XFD_TXQ_INTERRUPT:
        {
            // Clear any TXQ-related FIFO events, then the global TX event.
            eMCP251XFD_TXQstatus qst;
            err = MCP251XFD_GetTXQStatus(dev, &qst);
            if (err != ERR_OK)
            {
                LOG_ERROR("GetTXQStatus failed: %d", err);
                break;
            }
            if (qst & MCP251XFD_TXQ_ATTEMPTS_EXHAUSTED) {
                MCP251XFD_ResetTXQ(dev);
                // MCP251XFD_ClearTXQAttemptsEvent(dev);
            }
            // MCP251XFD_ClearInterruptEvents(dev, MCP251XFD_INT_TX_EVENT);
            break;
        }

        case MCP251XFD_FIFO1_INTERRUPT:
        {
                eMCP251XFD_FIFOstatus st;
                err = MCP251XFD_GetFIFOStatus(dev, MCP251XFD_FIFO1, &st);
                LOG_TRACE("RX FIFO Status: %d", st);
                if (err != ERR_OK) {
                    LOG_ERROR("GetFIFOStatus(FIFO1) failed: %d", err);
                    break;
                }
                if ((st & MCP251XFD_RX_FIFO_NOT_EMPTY)) {
                    if (ctx->rx_cb) {
                        ctx->rx_cb(dev, ctx->cb_user_reference);
                    } else {
                        // If no callback is installed, consume one frame to unblock IRQs.
                        // (Implement a small "discard" receive here if needed by your lib.)
                        // For now, just break to avoid a tight loop:
                        LOG_WARNING("RX FIFO not empty but no rx_cb installed");
                    }
                }
            MCP251XFD_ClearFIFOEvents(dev, MCP251XFD_FIFO1,
                                      MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT |
                                          MCP251XFD_FIFO_RECEIVE_FIFO_HALF_FULL_INT |
                                          MCP251XFD_FIFO_RECEIVE_FIFO_FULL_INT);
            MCP251XFD_ClearInterruptEvents(dev, MCP251XFD_INT_RX_EVENT);
            break;
        }

        default:
        {
            // Clear any clearable system bits to avoid stalls
            MCP251XFD_ClearInterruptEvents(dev,
                                           MCP251XFD_INT_CLEARABLE_FLAGS_MASK);
            LOG_TRACE("Unhandled event %d cleared (mask=CiINT clearables)", code);
            break;
        }
        }
    }
}
