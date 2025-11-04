#include "leos/log.h"
#include "leos/mcp251xfd.h"
#include "leos/mcp251xfd/config.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "platform.h"
#include <string.h>

static const leos_mcp251xfd_config_t *s_mcp_by_gpio[48] = {0};

void leos_mcp251xfd_check_irq(uint gpio, uint32_t events) {
    if (!(events & GPIO_IRQ_EDGE_FALL)) return;

    const leos_mcp251xfd_config_t *config = s_mcp_by_gpio[gpio];
    if (!config || !config->irq_callback)
        return;

    config->irq_callback(config->irq_user_ref);
}


/* ========== MCP251XFD Initialization Helpers ========== */

eERRORRESULT leos_mcp251xfd_init(MCP251XFD *device, const leos_mcp251xfd_hw_t *hw, const leos_mcp251xfd_config_t *config)
{
    // Ensure the device structure starts zeroed to avoid garbage DriverConfig or other flags
    memset(device, 0, sizeof(*device));

    // Setup IRQ
    gpio_init(hw->pin_irq);
    gpio_set_dir(hw->pin_irq, GPIO_IN);
    gpio_pull_up(hw->pin_irq);
    gpio_set_irq_enabled(hw->pin_irq, GPIO_IRQ_EDGE_FALL, true);
    
    // Store the config in a array map to the irq gpio
    if (hw->pin_irq >= count_of(s_mcp_by_gpio))
        return ERR__OUT_OF_RANGE;
    s_mcp_by_gpio[hw->pin_irq] = config;


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

    // Since the config is now const, we should NOT fill the RamInfo return slots, and instead they must be
    // initialized as NULL to signify they are not used.


    // MCP251XFD_RAMInfos FIFO_ram_info[config->num_fifos]; // Will hold info for fifos
    // for (int i = 0; i < config->num_fifos; i++) { // Set RAM Info return pointers to our array
    //     config->fifo[i].RAMInfos = &FIFO_ram_info[i];
    // }

    eERRORRESULT fifo_result = MCP251XFD_ConfigureFIFOList(device, (MCP251XFD_FIFO*) config->fifo, config->num_fifos);
    if (fifo_result != ERR_OK) {
        return fifo_result;
    }

    eERRORRESULT filter_result = MCP251XFD_ConfigureFilterList(device, MCP251XFD_D_NET_FILTER_DISABLE, (MCP251XFD_Filter*) config->filter, config->num_filters);
    if (filter_result != ERR_OK) {
        return filter_result;
    }

    // Opting to not setup sleep mode yet.

    // Start chip in CAN-FD mode. Configuration is closed.
    return MCP251XFD_RequestOperationMode(device, config->initial_mode, true);
}
