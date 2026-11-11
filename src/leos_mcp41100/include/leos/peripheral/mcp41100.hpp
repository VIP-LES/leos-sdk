#pragma once

#include <cstdint>
#include "hardware/spi.h"

namespace leos::peripheral {

class Mcp41100 {
public:
    struct Config {
        spi_inst_t* spi;
        std::uint32_t baud_hz;
        uint sck_pin;
        uint mosi_pin;
        uint cs_pin;
    };

    explicit Mcp41100(Config config) noexcept : config_(config) {}

    void init() noexcept;

    // Write an 8-bit wiper code. This confirms the SPI write, not the output voltage.
    bool setWiper(std::uint8_t code) noexcept;

private:
    Config config_;
};

} // namespace leos::peripheral
