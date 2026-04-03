#include "leos/purpleboard.h"
#include "leos/log.h"
#include "Adafruit_BME680.h"
#include "Adafruit_LTR390.h"
#include "Adafruit_PM25AQI.h"
#include "Adafruit_TSL2591.h"


struct leos_purpleboard {
    i2c_inst_t *i2c_block;
    uint sda;
    uint scl;
    TwoWire* wire;

    Adafruit_BME680* bmp;
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

    pb->bmp = new Adafruit_BME680(pb->wire);
    pb->ltr = new Adafruit_LTR390();
    pb->aqi = new Adafruit_PM25AQI();
    pb->tsl = new  Adafruit_TSL2591();

    if (!pb->bmp->begin()){
        LOG_ERROR("Failed to initialize BME680 on Purpleboard");
        result = PB_SENSOR_NO_DETECT;
    } else{
        pb->bmp->setTemperatureOversampling(BME680_OS_8X);
        pb->bmp->setHumidityOversampling(BME680_OS_2X);
        pb->bmp->setPressureOversampling(BME680_OS_4X);
        pb->bmp->setIIRFilterSize(BME680_FILTER_SIZE_3);
        pb->bmp->setGasHeater(320, 150);
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
    

    pb->aqi->begin_I2C();

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

    if (!pb->bmp->performReading()) {
        LOG_WARNING("Failed to read BMP388 values");
        result = PB_SENSOR_READ_DEGRADED;
        sensor_data->temperature_c = -1.0;
        sensor_data->pressure_mb = -1.0;
    } else {
        sensor_data->temperature_c = pb->bmp->temperature;
        sensor_data->pressure_mb = pb->bmp->pressure / 100.0;
    }

    //  I have no way to validate this!!
    sensor_data->ltr390_uvs = pb->ltr->readUVS();
    // Data may be stale, if desired, poll the ltr until new data is available.


    PM25_AQI_Data aqi_data;
    if (!pb->aqi->read(&aqi_data)) {
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
        sensor_data->lux = -1;
    } else {
        sensor_data->lux = event.light;
    }

    return result;
}


void leos_purpleboard_deinit(leos_purpleboard_t *pb) {
    pb->ltr->enable(false);
    delete pb->tsl;
    delete pb->wire;
    delete pb;
}