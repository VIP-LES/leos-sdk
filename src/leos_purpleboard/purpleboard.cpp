#include "leos/purpleboard.h"
#include "leos/log.h"
#include "Adafruit_BMP3XX.h"
#include "Adafruit_LTR390.h"
#include "Adafruit_PM25AQI_I2C.h"
#include "Adafruit_TSL2561_U.h"


struct leos_purpleboard {
    i2c_inst_t *i2c_block;
    uint sda;
    uint scl;
    TwoWire* wire;
    Adafruit_BMP3XX bmp;
    Adafruit_LTR390 ltr;
    Adafruit_PM25AQI_I2C aqi;
    Adafruit_TSL2561_Unified* tsl;
};

leos_purpleboard_result_t leos_purpleboard_init(i2c_inst_t *i2c, uint sda, uint scl, leos_purpleboard_t **out_pb) {
    LOG_DEBUG("Initializing Purpleboard stack");
    leos_purpleboard_t *pb = new leos_purpleboard; // assign a new purpleboard struct into a heap location
    pb->i2c_block = i2c;
    pb->sda = sda;
    pb->scl = scl;
    pb->wire = new TwoWire(i2c, sda, scl);
    pb->tsl = new Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT);

    if (!pb->bmp.begin_I2C(0x77, pb->wire)) {
        LOG_ERROR("Failed to initialize BMP388 on Purpleboard");
        delete pb->tsl;
        delete pb->wire;
        delete pb;
        return PB_SENSOR_NO_DETECT;
    }
    // Set up oversampling and filter initialization
    pb->bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    pb->bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    pb->bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    pb->bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    LOG_DEBUG("Initialized BMP388 on Purpleboard");

    if (!pb->ltr.begin(pb->wire)) {
        LOG_ERROR("Failed to initialize LTR390 on Purpleboard");
        delete pb->tsl;
        delete pb->wire;
        delete pb;
        return PB_SENSOR_NO_DETECT;
    }
    pb->ltr.setMode(LTR390_MODE_UVS);
    pb->ltr.setResolution(LTR390_RESOLUTION_16BIT);
    pb->ltr.setGain(LTR390_GAIN_1);
    LOG_DEBUG("Initialized LTR390 on Purpleboard");

    if (!pb->aqi.begin(pb->wire)) {
        LOG_ERROR("Failed to initialize PM25AQI on Purpleboard");
        delete pb->tsl;
        delete pb->wire;
        delete pb;
        return PB_SENSOR_NO_DETECT;
    }
    LOG_DEBUG("Initialized PM25AQI on Purpleboard");

    if (!pb->tsl->begin(pb->wire)) {
        LOG_ERROR("Failed to initialize TSL2651 on Purpleboard");
        delete pb->tsl;
        delete pb->wire;
        delete pb;
        return PB_SENSOR_NO_DETECT;
    }
    pb->tsl->enableAutoRange(true); // Auto gain
    pb->tsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS); // Medium resolution
    LOG_DEBUG("Initialized TSL2651 on Purpleboard");

    LOG_DEBUG("Purpleboard successfully initialized");
    *out_pb = pb;
    return PB_OK;
}

leos_purpleboard_result_t leos_purpleboard_read(leos_purpleboard_t* pb, leos_purpleboard_readings_t *sensor_data) {
    if (pb == NULL) {
        LOG_WARNING("leos_purpleboard_read was given a null ptr to pb");
        return PB_PARAMETER_ERROR;
    }
    leos_purpleboard_result_t result = PB_OK;

    if (!pb->bmp.performReading()) {
        LOG_WARNING("Failed to read BMP388 values");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->temperature_c = -1.0;
        sensor_data->pressure_mb = -1.0;
    } else {
        sensor_data->temperature_c = pb->bmp.temperature;
        sensor_data->pressure_mb = pb->bmp.pressure;
    }
    sensor_data->temperature_c = pb->bmp.temperature;
    sensor_data->pressure_mb = pb->bmp.pressure;

    //  I have no way to validate this!!
    sensor_data->uvs = pb->ltr.readUVS();
    // Data may be stale, if desired, poll the ltr until new data is available.


    PM25_AQI_Data aqi_data;
    if (!pb->aqi.read(&aqi_data)) {
        LOG_WARNING("Failed to read Purpleboard air sensor");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->pm10_env = -1;
        sensor_data->pm25_env = -1;
        sensor_data->pm100_env = -1;
        sensor_data->aqi_pm25_us = -1;
        sensor_data->aqi_pm100_us = -1;
    } else {
        sensor_data->pm10_env = aqi_data.pm10_env;
        sensor_data->pm25_env = aqi_data.pm25_env;
        sensor_data->pm100_env = aqi_data.pm100_env;
        sensor_data->aqi_pm25_us = aqi_data.aqi_pm25_us;
        sensor_data->aqi_pm100_us = aqi_data.aqi_pm100_us;
    }

    sensors_event_t event;
    if(!pb->tsl->getEvent(&event) || !event.light) {
        LOG_WARNING("Failed to read TSL2651 light value");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->light_lux = -1;
    } else {
        sensor_data->light_lux = event.light;
    }

    return result;
}


void leos_purpleboard_deinit(leos_purpleboard_t *pb) {
    pb->ltr.enable(false);
    delete pb->tsl;
    delete pb->wire;
    delete pb;
}