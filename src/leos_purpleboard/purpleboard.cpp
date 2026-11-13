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

leos_purpleboard_result_t leos_purpleboard_fakeinit(i2c_inst_t *i2c, uint sda, uint scl, leos_purpleboard_t **out_pb) {
    return PB_OK;
}

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
        pb->tsl->setTiming(TSL2591_INTEGRATIONTIME_100MS);
        LOG_DEBUG("Initialized TSL2591 on Purpleboard");
    }
    

    LOG_DEBUG("Purpleboard finished initializing");
    *out_pb = pb;
    return result;
}

leos_purpleboard_result_t leos_purpleboard_read_bme680(leos_purpleboard_t *pb, leos_purpleboard_bme680_readings_t *readings, bool *did_read, int *remaining_time_millis) {
    if ((pb == NULL) || (readings == NULL) || (remaining_time_millis == NULL) || (did_read == NULL)) {
        LOG_WARNING("leos_purpleboard_read_bme680 was given a null ptr");
        return PB_PARAMETER_ERROR;
    }
    leos_purpleboard_result_t result = PB_OK;
    *remaining_time_millis = 0;
    *did_read = false;

    int remaining_millis = pb->bme->remainingReadingMillis();
    if (remaining_millis == -1) { // No reading started yet
        int deadline_millis = pb->bme->beginReading();
        unsigned long current_millis = millis();
        *remaining_time_millis = deadline_millis > current_millis ? deadline_millis - current_millis : 0;
    } else if (remaining_millis == 0) {
        *did_read = true;
        if (!pb->bme->endReading()) {
            LOG_WARNING("Failed to read BME680 values");
            result = PB_SENSOR_READ_ERROR;
            readings->valid = false;
            readings->temperature_c = 0.0f;
            readings->pressure_mb = 0.0f;
            readings->humidity = 0.0f;
            readings->altitude_m = 0.0f;
            readings->gas_resistance = 0;
        } else {
            readings->valid = true;
            readings->temperature_c = pb->bme->temperature;
            readings->pressure_mb = pb->bme->pressure / 100.0f;
            readings->humidity = pb->bme->humidity;
            readings->altitude_m = 44330.0 * (1.0 - pow(readings->pressure_mb / kDefaultSeaLevelPressureHpa, 0.1903));
            readings->gas_resistance = pb->bme->gas_resistance;
        }
    } else {
        *remaining_time_millis = remaining_millis;
    }

    return result;
}


leos_purpleboard_result_t leos_purpleboard_read_tsl2591(leos_purpleboard_t *pb, leos_purpleboard_tsl2591_readings_t *readings) {
    if ((pb == NULL) || (readings == NULL)) {
        LOG_WARNING("leos_purpleboard_read_tsl2591 was given a null ptr");
        return PB_PARAMETER_ERROR;
    }
    leos_purpleboard_result_t result = PB_OK;
    
    sensors_event_t event;
    if(!pb->tsl->getEvent(&event)) {
        LOG_WARNING("Failed to read TSL2591 light value");
        result = PB_SENSOR_READ_ERROR;
        readings->valid = false;
        readings->lux = 0.0f;
        readings->raw_visible = 0;
        readings->raw_infrared = 0;
        readings->raw_full_spectrum = 0;
    } else {
        readings->valid = true;
        readings->lux = event.light;
        const uint32_t full_spectrum = pb->tsl->getFullLuminosity();
        readings->raw_full_spectrum = full_spectrum;
        readings->raw_infrared = static_cast<uint16_t>(full_spectrum >> 16);
        readings->raw_visible = static_cast<uint16_t>(full_spectrum & 0xFFFF);
    }

    return result;
}


leos_purpleboard_result_t leos_purpleboard_read_ltr390(leos_purpleboard_t *pb, leos_purpleboard_ltr390_readings_t *readings, bool *did_read) {
    if ((pb == NULL) || (readings == NULL) || (did_read == NULL)) {
        LOG_WARNING("leos_purpleboard_read_ltr390 was given a null ptr");
        return PB_PARAMETER_ERROR;
    }
    leos_purpleboard_result_t result = PB_OK;

    *did_read = pb->ltr->newDataAvailable();

    if (!*did_read) return result;
    
    readings->valid = pb->ltr->getMode() == LTR390_MODE_UVS;
    if (!readings->valid) {
        result = PB_SENSOR_READ_ERROR;
        readings->ltr390_uvs = 0;
    } else {
        readings->ltr390_uvs = pb->ltr->readUVS();
    }

    return result;
}

leos_purpleboard_result_t leos_purpleboard_read_pmsa003i(leos_purpleboard_t *pb, leos_purpleboard_pmsa003i_readings_t *readings) {
    if ((pb == NULL) || (readings == NULL)) {
        LOG_WARNING("leos_purpleboard_read_pmsa003i was given a null ptr");
        return PB_PARAMETER_ERROR;
    }
    leos_purpleboard_result_t result = PB_OK;

    PM25_AQI_Data aqi_data;
    if (!pb->aqi->read(&aqi_data)) {
        LOG_WARNING("Failed to read Purpleboard air sensor");
        result = PB_SENSOR_READ_ERROR;
        readings->valid = false;
        readings->pm10_env = 0;
        readings->pm25_env = 0;
        readings->pm100_env = 0;
        readings->aqi_pm25_us = 0;
        readings->aqi_pm100_us = 0;
        readings->particles_03um = 0;
        readings->particles_05um = 0;
        readings->particles_10um = 0;
        readings->particles_25um = 0;
        readings->particles_50um = 0;
        readings->particles_100um = 0;
    } else {
        readings->valid = true;
        readings->pm10_env = aqi_data.pm10_env;
        readings->pm25_env = aqi_data.pm25_env;
        readings->pm100_env = aqi_data.pm100_env;
        readings->aqi_pm25_us = aqi_data.aqi_pm25_us;
        readings->aqi_pm100_us = aqi_data.aqi_pm100_us;
        readings->particles_03um = aqi_data.particles_03um;
        readings->particles_05um = aqi_data.particles_05um;
        readings->particles_10um = aqi_data.particles_10um;
        readings->particles_25um = aqi_data.particles_25um;
        readings->particles_50um = aqi_data.particles_50um;
        readings->particles_100um = aqi_data.particles_100um;
    }

    return result;
}

/*
leos_purpleboard_result_t leos_purpleboard_read(leos_purpleboard_t* pb, leos_purpleboard_readings_t* sensor_data) {
    if ((pb == NULL) || (sensor_data == NULL)) {
        LOG_WARNING("leos_purpleboard_read was given a null ptr to pb");
        return PB_PARAMETER_ERROR;
    }
    leos_purpleboard_result_t result = PB_OK;

    if (!pb->bme->performReading()) {
        LOG_WARNING("Failed to read BME680 values");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->bme680_valid = false;
        sensor_data->temperature_c = 0.0f;
        sensor_data->pressure_mb = 0.0f;
        sensor_data->humidity = 0.0f;
        sensor_data->altitude_m = 0.0f;
        sensor_data->gas_resistance = 0;
    } else {
        sensor_data->bme680_valid = true;
        sensor_data->temperature_c = pb->bme->temperature;
        sensor_data->pressure_mb = pb->bme->pressure / 100.0f;
        sensor_data->humidity = pb->bme->humidity;
        sensor_data->altitude_m = pb->bme->readAltitude(kDefaultSeaLevelPressureHpa);
        sensor_data->gas_resistance = pb->bme->gas_resistance;
    }

    LOG_INFO("Time4!\n");

    // The LTR390 read API returns the raw UV value directly, so use data-ready plus mode as the cycle validity signal.
    sensor_data->ltr390_valid = (pb->ltr->getMode() == LTR390_MODE_UVS) && pb->ltr->newDataAvailable();
    if (!sensor_data->ltr390_valid) {
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->ltr390_uvs = 0;
    } else {
        sensor_data->ltr390_uvs = pb->ltr->readUVS();
    }
    LOG_INFO("Time3!\n");

    PM25_AQI_Data aqi_data;
    if (!pb->aqi->read(&aqi_data)) {
        LOG_WARNING("Failed to read Purpleboard air sensor");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->pmsa003i_valid = false;
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
        sensor_data->pmsa003i_valid = true;
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

    LOG_INFO("Time2!\n");

    sensors_event_t event;
    if(!pb->tsl->getEvent(&event)) {
        LOG_WARNING("Failed to read TSL2591 light value");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->tsl2591_valid = false;
        sensor_data->lux = 0.0f;
        sensor_data->raw_visible = 0;
        sensor_data->raw_infrared = 0;
        sensor_data->raw_full_spectrum = 0;
    } else {
        sensor_data->tsl2591_valid = true;
        sensor_data->lux = event.light;
        const uint32_t full_spectrum = pb->tsl->getFullLuminosity();
        sensor_data->raw_full_spectrum = full_spectrum;
        sensor_data->raw_infrared = static_cast<uint16_t>(full_spectrum >> 16);
        sensor_data->raw_visible = static_cast<uint16_t>(full_spectrum & 0xFFFF);
    }

    LOG_INFO("Time1!\n");


    return result;
}
    */


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
