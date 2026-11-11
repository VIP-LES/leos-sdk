#pragma once

#include <cstdint>

#include "Adafruit_BME680.h"
#include "Adafruit_LTR390.h"
#include "Adafruit_PM25AQI.h"
#include "Adafruit_TSL2591.h"
#include "Wire.h"
#include "hardware/i2c.h"
#include "pico/time.h"

namespace leos {

// The Purpleboard owns its I2C bus wrapper and all four sensor drivers.
// Construct it once, call init(), then run one sampling cycle at a time.
class Purpleboard final {
public:
    enum class Result : std::uint8_t {
        Ok,
        SensorNoDetect,
        SensorReadDegraded,
        NotInitialized,
        NotReady,
        NotSampling,
    };

    struct Readings {
        bool bme680_valid = false;
        float temperature_c = 0.0f;
        float pressure_mb = 0.0f;
        float humidity = 0.0f;
        float altitude_m = 0.0f;
        std::uint32_t gas_resistance = 0;

        bool tsl2591_valid = false;
        float lux = 0.0f;
        std::uint16_t raw_visible = 0;
        std::uint16_t raw_infrared = 0;
        std::uint32_t raw_full_spectrum = 0;

        bool ltr390_valid = false;
        std::uint32_t ltr390_uvs = 0;

        bool pmsa003i_valid = false;
        std::uint32_t pm10_env = 0;
        std::uint32_t pm25_env = 0;
        std::uint32_t pm100_env = 0;
        std::uint32_t aqi_pm25_us = 0;
        std::uint32_t aqi_pm100_us = 0;
        std::uint32_t particles_03um = 0;
        std::uint32_t particles_05um = 0;
        std::uint32_t particles_10um = 0;
        std::uint32_t particles_25um = 0;
        std::uint32_t particles_50um = 0;
        std::uint32_t particles_100um = 0;
    };

    Purpleboard(i2c_inst_t* i2c, unsigned int sda, unsigned int scl);
    ~Purpleboard();

    Purpleboard(const Purpleboard&) = delete;
    Purpleboard& operator=(const Purpleboard&) = delete;
    Purpleboard(Purpleboard&&) = delete;
    Purpleboard& operator=(Purpleboard&&) = delete;

    // Each sensor is attempted even if another fails. A failed sensor stays
    // invalid in subsequent readings; working sensors remain available.
    // Repeated calls return the original detection result without reinitializing.
    Result init();

    // Start the BME680 measurement and TSL2591 light integration together.
    // Returns false before init() or while a sample is already in progress.
    bool startSampling();

    // Advances light integration when needed; may perform a short I2C write.
    // Call regularly while servicing other board work.
    bool samplingReady();

    // Nonblocking until samplingReady() is true. Invalid sensor fields are
    // zeroed; a degraded result still contains data from working sensors.
    Result finishSampling(Readings& readings);

private:
    bool readTslChannel(std::uint8_t reg, std::uint16_t& value);

    i2c_inst_t* i2c_;
    TwoWire wire_;
    Adafruit_BME680 bme_;
    Adafruit_LTR390 ltr_;
    Adafruit_PM25AQI aqi_;
    Adafruit_TSL2591 tsl_;
    bool initialized_ = false;
    bool bme_present_ = false;
    bool ltr_present_ = false;
    bool aqi_present_ = false;
    bool tsl_present_ = false;
    bool sampling_ = false;
    bool bme_started_ = false;
    bool tsl_first_integration_ = false;
    absolute_time_t tsl_ready_at_{};
};

} // namespace leos
