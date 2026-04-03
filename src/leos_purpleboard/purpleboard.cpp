#include "leos/purpleboard.h"
#include "leos/log.h"
#include "Adafruit_BME680.h"
#include "Adafruit_LTR390.h"
#include "Adafruit_PM25AQI.h"
#include "Adafruit_TSL2591.h"

namespace {
constexpr float kDefaultSeaLevelPressureHpa = 1013.25f; // Default sea-level reference for derived altitude; can be made launch-day configurable later.
}

struct leos_purpleboard {
    i2c_inst_t *i2c_block;
    uint sda;
    uint scl;
    TwoWire* wire;

    Adafruit_BME680* bme;
    Adafruit_LTR390* ltr;
    Adafruit_PM25AQI* aqi;
    Adafruit_TSL2591* tsl;
};
leos_purpleboard_result_t leos_purpleboard_init(i2c_inst_t *i2c, uint sda, uint scl, leos_purpleboard_t **out_pb) {
    leos_purpleboard_result_t result = PB_OK;
    LOG_DEBUG("Initializing Purpleboard stack");
    leos_purpleboard_t *pb = new leos_purpleboard; // assign a new purpleboard struct into a heap location
    pb->i2c_block = i2c;
    pb->sda = sda;
    pb->scl = scl;
    pb->wire = new TwoWire(i2c, sda, scl);

    pb->bme = new Adafruit_BME680(pb->wire);
    pb->ltr = new Adafruit_LTR390();
    pb->aqi = new Adafruit_PM25AQI();
    pb->tsl = new  Adafruit_TSL2591();

    if (!pb->bme->begin()){
        LOG_ERROR("Failed to initialize BME680 on Purpleboard");
        result = PB_SENSOR_NO_DETECT;
    } else{
        pb->bme->setTemperatureOversampling(BME680_OS_8X);
        pb->bme->setHumidityOversampling(BME680_OS_2X);
        pb->bme->setPressureOversampling(BME680_OS_4X);
        pb->bme->setIIRFilterSize(BME680_FILTER_SIZE_3);
        pb->bme->setGasHeater(320, 150);
        LOG_DEBUG("Initialized BME680 on Purpleboard");
    }


    if (!pb->ltr->begin(pb->wire)) {
        LOG_ERROR("Failed to initialize LTR390 on Purpleboard");
        result = PB_SENSOR_NO_DETECT;
    } else {
        pb->ltr->setMode(LTR390_MODE_UVS);
        pb->ltr->setResolution(LTR390_RESOLUTION_16BIT);
        pb->ltr->setGain(LTR390_GAIN_1);
        LOG_DEBUG("Initialized LTR390 on Purpleboard");
    }

    if (!pb->aqi->begin_I2C(pb->wire)) {
        LOG_ERROR("Failed to initialize PM25AQI on Purpleboard");
        result = PB_SENSOR_NO_DETECT;
    } else {
        LOG_DEBUG("Initialized PM25AQI on Purpleboard");
    }

    if (!pb->tsl->begin(pb->wire)) {
        LOG_ERROR("Failed to initialize TSL2591 on Purpleboard");
        result = PB_SENSOR_NO_DETECT;
    } else {
        pb->tsl->setGain(TSL2591_GAIN_MED);
        pb->tsl->setTiming(TSL2591_INTEGRATIONTIME_300MS);
        LOG_DEBUG("Initialized TSL2591 on Purpleboard");
    }
    

    LOG_DEBUG("Purpleboard finished initializing");
    *out_pb = pb;
    return result;
}

leos_purpleboard_result_t leos_purpleboard_read(leos_purpleboard_t* pb, leos_purpleboard_readings_t *sensor_data) {
    if (pb == NULL) {
        LOG_WARNING("leos_purpleboard_read was given a null ptr to pb");
        return PB_PARAMETER_ERROR;
    }
    leos_purpleboard_result_t result = PB_OK;

    if (!pb->bme->performReading()) {
        LOG_WARNING("Failed to read BME680 values");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->temperature_c = 0.0f;
        sensor_data->pressure_mb = 0.0f;
        sensor_data->humidity = 0.0f;
        sensor_data->altitude_m = 0.0f;
        sensor_data->gas_resistance = 0;
    } else {
        sensor_data->temperature_c = pb->bme->temperature;
        sensor_data->pressure_mb = pb->bme->pressure / 100.0f;
        sensor_data->humidity = pb->bme->humidity;
        sensor_data->altitude_m = pb->bme->readAltitude(kDefaultSeaLevelPressureHpa);
        sensor_data->gas_resistance = pb->bme->gas_resistance;
    }

    // The LTR390 read API returns the raw UV value directly, so callers should pair this with sensor-level validity.
    sensor_data->ltr390_uvs = pb->ltr->readUVS();

    PM25_AQI_Data aqi_data;
    if (!pb->aqi->read(&aqi_data)) {
        LOG_WARNING("Failed to read Purpleboard air sensor");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->pm10_env = 0;
        sensor_data->pm25_env = 0;
        sensor_data->pm100_env = 0;
        sensor_data->aqi_pm25_us = 0;
        sensor_data->aqi_pm100_us = 0;
        sensor_data->particles_03um = 0;
        sensor_data->particles_05um = 0;
        sensor_data->particles_10um = 0;
        sensor_data->particles_25um = 0;
        sensor_data->particles_50um = 0;
        sensor_data->particles_100um = 0;
    } else {
        sensor_data->pm10_env = aqi_data.pm10_env;
        sensor_data->pm25_env = aqi_data.pm25_env;
        sensor_data->pm100_env = aqi_data.pm100_env;
        sensor_data->aqi_pm25_us = aqi_data.aqi_pm25_us;
        sensor_data->aqi_pm100_us = aqi_data.aqi_pm100_us;
        sensor_data->particles_03um = aqi_data.particles_03um;
        sensor_data->particles_05um = aqi_data.particles_05um;
        sensor_data->particles_10um = aqi_data.particles_10um;
        sensor_data->particles_25um = aqi_data.particles_25um;
        sensor_data->particles_50um = aqi_data.particles_50um;
        sensor_data->particles_100um = aqi_data.particles_100um;
    }

    sensors_event_t event;
    if(!pb->tsl->getEvent(&event) || !event.light) {
        LOG_WARNING("Failed to read TSL2591 light value");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->lux = 0.0f;
        sensor_data->raw_visible = 0;
        sensor_data->raw_infrared = 0;
        sensor_data->raw_full_spectrum = 0;
    } else {
        sensor_data->lux = event.light;
        const uint32_t full_spectrum = pb->tsl->getFullLuminosity();
        sensor_data->raw_full_spectrum = full_spectrum;
        sensor_data->raw_infrared = static_cast<uint16_t>(full_spectrum >> 16);
        sensor_data->raw_visible = static_cast<uint16_t>(full_spectrum & 0xFFFF);
    }

    return result;
}


void leos_purpleboard_deinit(leos_purpleboard_t *pb) {
    if (pb == NULL) {
        return;
    }
    pb->ltr->enable(false);
    delete pb->bme;
    delete pb->ltr;
    delete pb->aqi;
    delete pb->tsl;
    delete pb->wire;
    delete pb;
}
