#include "leos/peripheral/mcp41100.hpp"

#include "pico/stdlib.h"

namespace leos::peripheral {

void Mcp41100::init() noexcept {
    // Keep CS high before enabling its output driver.
    gpio_init(config_.cs_pin);
    gpio_put(config_.cs_pin, true);
    gpio_set_dir(config_.cs_pin, GPIO_OUT);

    spi_init(config_.spi, config_.baud_hz);
    spi_set_format(config_.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(config_.sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(config_.mosi_pin, GPIO_FUNC_SPI);
}

bool Mcp41100::setWiper(std::uint8_t code) noexcept {
    constexpr std::uint8_t write_wiper_command = 0x11;
    const std::uint8_t command[] = {write_wiper_command, code};

    gpio_put(config_.cs_pin, false);
    sleep_us(1);
    const int sent = spi_write_blocking(config_.spi, command, sizeof(command));
    sleep_us(1);
    gpio_put(config_.cs_pin, true);

    return sent == static_cast<int>(sizeof(command));
}

} // namespace leos::peripheral
