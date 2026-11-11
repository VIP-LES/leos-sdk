#include "leos/purpleboard.hpp"

#include "leos/log.h"

#include <cmath>

#define PURPLEBOARD_SEA_LEVEL_PRESSURE_HPA 1013.25f
// The TSL2591 library waits 120 ms for each of the three 300 ms integration steps.
#define PURPLEBOARD_TSL2591_READY_MS 360u
#define PURPLEBOARD_I2C_TIMEOUT_US 10000u

namespace leos {

Purpleboard::Purpleboard(i2c_inst_t* i2c, unsigned int sda, unsigned int scl)
    : i2c_(i2c), wire_(i2c, sda, scl), bme_(&wire_) {}

Purpleboard::~Purpleboard() {
    if (sampling_ && tsl_present_) {
        tsl_.disable();
    }
    if (ltr_present_) {
        ltr_.enable(false);
    }
}

Purpleboard::Result Purpleboard::init() {
    if (initialized_) {
        return (bme_present_ && ltr_present_ && aqi_present_ && tsl_present_)
            ? Result::Ok : Result::SensorNoDetect;
    }

    LOG_DEBUG("Initializing Purpleboard stack");
    bme_present_ = bme_.begin();
    if (bme_present_) {
        bme_.setTemperatureOversampling(BME680_OS_8X);
        bme_.setHumidityOversampling(BME680_OS_2X);
        bme_.setPressureOversampling(BME680_OS_4X);
        bme_.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme_.setGasHeater(320, 150);
        LOG_DEBUG("Initialized BME680 on Purpleboard");
    } else {
        LOG_ERROR("Failed to initialize BME680 on Purpleboard");
    }

    ltr_present_ = ltr_.begin(&wire_);
    if (ltr_present_) {
        ltr_.setMode(LTR390_MODE_UVS);
        ltr_.setResolution(LTR390_RESOLUTION_16BIT);
        ltr_.setGain(LTR390_GAIN_1);
        LOG_DEBUG("Initialized LTR390 on Purpleboard");
    } else {
        LOG_ERROR("Failed to initialize LTR390 on Purpleboard");
    }

    aqi_present_ = aqi_.begin_I2C(&wire_);
    if (aqi_present_) {
        LOG_DEBUG("Initialized PM25AQI on Purpleboard");
    } else {
        LOG_ERROR("Failed to initialize PM25AQI on Purpleboard");
    }

    tsl_present_ = tsl_.begin(&wire_);
    if (tsl_present_) {
        tsl_.setGain(TSL2591_GAIN_MED);
        tsl_.setTiming(TSL2591_INTEGRATIONTIME_300MS);
        LOG_DEBUG("Initialized TSL2591 on Purpleboard");
    } else {
        LOG_ERROR("Failed to initialize TSL2591 on Purpleboard");
    }

    initialized_ = true;
    LOG_DEBUG("Purpleboard finished initializing");
    return (bme_present_ && ltr_present_ && aqi_present_ && tsl_present_)
        ? Result::Ok : Result::SensorNoDetect;
}

bool Purpleboard::startSampling() {
    if (!initialized_ || sampling_) {
        return false;
    }

    bme_started_ = bme_present_ && bme_.beginReading() != 0;
    if (tsl_present_) {
        tsl_.enable();
        tsl_first_integration_ = true;
        tsl_ready_at_ = make_timeout_time_ms(PURPLEBOARD_TSL2591_READY_MS);
    }
    sampling_ = true;
    return true;
}

bool Purpleboard::samplingReady() {
    if (!sampling_) {
        return false;
    }
    if (tsl_present_ && time_reached(tsl_ready_at_) && tsl_first_integration_) {
        // The upstream TSL2591 getEvent() discards its first integration to
        // handle abrupt light changes. Restart here without blocking the loop.
        tsl_.disable();
        tsl_.enable();
        tsl_first_integration_ = false;
        tsl_ready_at_ = make_timeout_time_ms(PURPLEBOARD_TSL2591_READY_MS);
    }
    return (!bme_started_ || bme_.remainingReadingMillis() <= 0)
        && (!tsl_present_ || (!tsl_first_integration_ && time_reached(tsl_ready_at_)));
}

bool Purpleboard::readTslChannel(std::uint8_t reg, std::uint16_t& value) {
    const std::uint8_t command = TSL2591_COMMAND_BIT | reg;
    std::uint8_t bytes[2]{};
    if (i2c_write_timeout_us(i2c_, TSL2591_ADDR, &command, 1, true,
                             PURPLEBOARD_I2C_TIMEOUT_US) != 1 ||
        i2c_read_timeout_us(i2c_, TSL2591_ADDR, bytes, sizeof(bytes), false,
                            PURPLEBOARD_I2C_TIMEOUT_US) != sizeof(bytes)) {
        return false;
    }
    value = static_cast<std::uint16_t>(bytes[0])
          | static_cast<std::uint16_t>(bytes[1] << 8);
    return true;
}

Purpleboard::Result Purpleboard::finishSampling(Readings& readings) {
    readings = {};
    if (!initialized_) {
        return Result::NotInitialized;
    }
    if (!sampling_) {
        return Result::NotSampling;
    }
    if (!samplingReady()) {
        return Result::NotReady;
    }
    sampling_ = false;

    Result result = Result::Ok;

    if (bme_started_ && bme_.endReading()) {
        readings.bme680_valid = true;
        readings.temperature_c = bme_.temperature;
        readings.pressure_mb = bme_.pressure / 100.0f;
        readings.humidity = bme_.humidity;
        // readAltitude() starts another forced BME680 measurement. Derive
        // altitude from this cycle's pressure instead.
        readings.altitude_m = 44330.0f *
            (1.0f - std::pow(readings.pressure_mb / PURPLEBOARD_SEA_LEVEL_PRESSURE_HPA,
                              0.1903f));
        readings.gas_resistance = bme_.gas_resistance;
    } else {
        if (bme_present_) {
            LOG_WARNING("Failed to read BME680 values");
        }
        result = Result::SensorReadDegraded;
    }

    // LTR390 returns a raw value; its mode and data-ready bit indicate validity.
    if (ltr_present_ && ltr_.getMode() == LTR390_MODE_UVS && ltr_.newDataAvailable()) {
        readings.ltr390_valid = true;
        readings.ltr390_uvs = ltr_.readUVS();
    } else {
        result = Result::SensorReadDegraded;
    }

    PM25_AQI_Data aqi_data{};
    if (aqi_present_ && aqi_.read(&aqi_data)) {
        readings.pmsa003i_valid = true;
        readings.pm10_env = aqi_data.pm10_env;
        readings.pm25_env = aqi_data.pm25_env;
        readings.pm100_env = aqi_data.pm100_env;
        readings.aqi_pm25_us = aqi_data.aqi_pm25_us;
        readings.aqi_pm100_us = aqi_data.aqi_pm100_us;
        readings.particles_03um = aqi_data.particles_03um;
        readings.particles_05um = aqi_data.particles_05um;
        readings.particles_10um = aqi_data.particles_10um;
        readings.particles_25um = aqi_data.particles_25um;
        readings.particles_50um = aqi_data.particles_50um;
        readings.particles_100um = aqi_data.particles_100um;
    } else {
        if (aqi_present_) {
            LOG_WARNING("Failed to read Purpleboard air sensor");
        }
        result = Result::SensorReadDegraded;
    }

    if (tsl_present_) {
        std::uint16_t full = 0;
        std::uint16_t infrared = 0;
        // Read the completed integration once. getEvent() and
        // getFullLuminosity() each wait for another integration.
        const bool read_ok =
            readTslChannel(TSL2591_REGISTER_CHAN0_LOW, full) &&
            readTslChannel(TSL2591_REGISTER_CHAN1_LOW, infrared);
        tsl_.disable();
        if (read_ok) {
            readings.raw_full_spectrum = full;
            readings.raw_infrared = infrared;
            readings.raw_visible = full >= infrared ? full - infrared : 0;
            readings.lux = tsl_.calculateLux(full, infrared);
            readings.tsl2591_valid = readings.lux >= 0.0f;
        }
        if (!readings.tsl2591_valid) {
            LOG_WARNING("Failed to read TSL2591 light value");
            result = Result::SensorReadDegraded;
        }
    } else {
        result = Result::SensorReadDegraded;
    }

    return result;
}

} // namespace leos