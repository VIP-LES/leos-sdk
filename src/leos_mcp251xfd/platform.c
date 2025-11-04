#include "leos/log.h"
#include "leos/mcp251xfd/config.h"
#include "MCP251XFD.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#define SPI_TIMEOUT_US 5000

/* ==========  CAN DRIVERS AND HAL FUNCTIONS   ========== */

uint32_t MCP251XFD_GetCurrentMs_Pico(void)
{
    return to_ms_since_boot(get_absolute_time());
}

// Polynomial: 0x8005 (reflected 0xA001), Init = 0xFFFF, XOR out = 0x0000
// Taken from internet. I don't think we'll even use the CRC style so just lgtm.
uint16_t crc16_cms(const uint8_t* data, size_t size)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001; // 0xA001 is the reflected form of 0x8005
            else
                crc >>= 1;
        }
    }

    return crc;
}

uint16_t MCP251XFD_ComputeCRC16_Pico(const uint8_t* data, size_t size)
{
    return crc16_cms(data, size);
}

eERRORRESULT MCP251XFD_InterfaceInit_Pico(void* pIntDev, uint8_t chipSelect, const uint32_t sckFreq)
{
    if (pIntDev == NULL)
        return ERR__SPI_PARAMETER_ERROR;
    leos_mcp251xfd_hw_t* hw = (leos_mcp251xfd_hw_t*)pIntDev;

    // Initialize Pico SPI and pins
    spi_init(hw->spi, sckFreq);
    gpio_set_function(hw->pin_miso, GPIO_FUNC_SPI);
    gpio_set_function(hw->pin_mosi, GPIO_FUNC_SPI);
    gpio_set_function(hw->pin_sck, GPIO_FUNC_SPI);
    // CS pin setup. HIGH when SPI is not transferring
    gpio_init(chipSelect);
    gpio_set_dir(chipSelect, GPIO_OUT);
    gpio_put(chipSelect, 1);
    // Set according to datasheet
    spi_set_format(hw->spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    return ERR_OK;
}

eERRORRESULT MCP251XFD_InterfaceTransfer_Pico(void* pIntDev, uint8_t chipSelect, uint8_t* txData, uint8_t* rxData,
    size_t size)
{
    if (pIntDev == NULL)
        return ERR__SPI_PARAMETER_ERROR;
    if (txData == NULL)
        return ERR__SPI_PARAMETER_ERROR;

    // Cast from config ptr
    leos_mcp251xfd_hw_t* hw = (leos_mcp251xfd_hw_t*)pIntDev;

    // Disable interrupts during transfer process
    uint32_t irqs = save_and_disable_interrupts();

    // Set CS low to start transfer
    gpio_put(chipSelect, 0);

    // SPI is a byte for byte protocol
    // In the case of this driver, the length of TX will be length of RX.
    // For most cases, the driver will be padding the TX with zeroes
    // or garbage data so that it gets back the desired RX length.

    // The protocol for the MCP251X family of chips uses SPI as a direct
    // memory bus access for RAM, so this is the lowest level of data.

    for (int i = 0; i < size; i++) {

        absolute_time_t stop_time = make_timeout_time_us(SPI_TIMEOUT_US);
        while (!spi_is_writable(hw->spi)) {
            if (get_absolute_time() >= stop_time) {
                gpio_put(chipSelect, 1);
                restore_interrupts(irqs);
                LOG_ERROR("SPI write timeout on cs_pin %d", chipSelect);
                return ERR__SPI_TIMEOUT;
            }
        }

        // Using raw register access to store the TX byte in the data register, sending over SPI
        spi_get_hw(hw->spi)->dr = *txData++;

        stop_time = make_timeout_time_us(SPI_TIMEOUT_US);
        while (!spi_is_readable(hw->spi)) {
            if (get_absolute_time() >= stop_time) {
                gpio_put(chipSelect, 1);
                restore_interrupts(irqs);
                LOG_ERROR("SPI read timeout on cs_pin %d", chipSelect);
                return ERR__SPI_TIMEOUT;
            }
        }

        // Load lower byte of data register as a byte received
        uint8_t dataRead = (uint8_t)(spi_get_hw(hw->spi)->dr & 0x000000FF);
        if (rxData)
            *rxData++ = dataRead;
    }

    gpio_put(chipSelect, 1);
    restore_interrupts(irqs);

    return ERR_OK;
}
