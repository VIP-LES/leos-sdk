#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        PB_OK = 0,
        PB_SENSOR_NO_DETECT,
        PB_SENSOR_READ_DEGRADED,
        PB_PARAMETER_ERROR
    } leos_purpleboard_result_t;

    typedef struct leos_purpleboard_readings
    {
        bool bme680_valid;
        float temperature_c;
        float pressure_mb;
        float humidity; // BME680 Values
        float altitude_m;
        uint32_t gas_resistance;

        bool tsl2591_valid;
        float lux; // TSL2591
        uint16_t raw_visible;
        uint16_t raw_infrared;
        uint32_t raw_full_spectrum;

        bool ltr390_valid;
        uint32_t ltr390_uvs; // LTR390

        bool pmsa003i_valid;
        uint32_t pm10_env;
        uint32_t pm25_env;
        uint32_t pm100_env;
        uint32_t aqi_pm25_us;
        uint32_t aqi_pm100_us;
        uint32_t particles_03um;
        uint32_t particles_05um;
        uint32_t particles_10um;
        uint32_t particles_25um;
        uint32_t particles_50um;
        uint32_t particles_100um; // PMSA003I
    } leos_purpleboard_readings_t;

    // The state struct managing the purpleboard classes
    typedef struct leos_purpleboard leos_purpleboard_t;

    leos_purpleboard_result_t leos_purpleboard_init(i2c_inst_t *i2c, uint sda, uint scl, leos_purpleboard_t **out_pb);

    leos_purpleboard_result_t leos_purpleboard_read(leos_purpleboard_t *pb, leos_purpleboard_readings_t *sensor_data);

    void leos_purpleboard_deinit(leos_purpleboard_t *pb);

#ifdef __cplusplus
}
#endif
